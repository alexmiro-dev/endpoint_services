
#include "eps_common/CommandLineInterface.hpp"
#include "eps_common/Protocol.hpp"
#include "eps_common/definitions.hpp"

#include <crow.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream> // TODO delete this line once we have a logger
#include <latch>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace eps {

class Server {
public:
    explicit Server(int port)
        : version_{semver::version{defs::kInitialServerVersion}}, port_{port} {
        initMetrics();
        initMessageHandler();
        app_.loglevel(crow::LogLevel::Warning);

        CROW_ROUTE(app_, "/ws")
            .websocket()
            .onopen([&](crow::websocket::connection &conn) {
                CROW_LOG_INFO << "new websocket connection from " << conn.get_remote_ip();
                std::lock_guard<std::mutex> _{connectionsMtx_};
                users_.insert(&conn);
            })
            .onclose([&](crow::websocket::connection &conn, const std::string &reason) {
                CROW_LOG_INFO << "websocket connection closed: " << reason;
                std::lock_guard<std::mutex> _{connectionsMtx_};
                users_.erase(&conn);
            })
            .onmessage(
                [&](crow::websocket::connection &conn, const std::string &data, bool isBinary) {
                    std::lock_guard<std::mutex> _{connectionsMtx_};
                    if (!isBinary) {
                        try {
                            std::cout << "Received: " << data << std::endl;
                            nlohmann::json const dataJson = nlohmann::json::parse(data);
                            auto message = proto::toMessage(dataJson);

                            if (auto const response = messageHandler_.process(std::move(message));
                                response) {
                                conn.send_text(proto::toString(response.value()));
                            }
                        } catch (nlohmann::json::exception const &ex) {
                            nlohmann::json p = nlohmann::json::object();
                            p[proto::keys::kRequest] = data;
                            proto::Message response{.type = proto::MessageType::BadRequest,
                                                    .payload = p};
                            conn.send_text(proto::toString(response));
                        }
                    }
                });
    }

    ~Server() { shutdown(); }

    void run() {
        std::latch workersLatch{2U};
        cmdLineIfaceThr_ = std::jthread([&](std::stop_token st) { runCLI(st, workersLatch); });
        webServerThr_ = std::jthread([&](std::stop_token st) { runWebService(st, workersLatch); });
        workersLatch.wait();
    }

private:
    void initMessageHandler() {
        messageHandler_
            .onVersion([&](proto::Message &&message) {
                proto::Message response{.type = proto::MessageType::Accepted};

                if (!message.payload.contains(proto::keys::kVersion)) {
                    response.type = proto::MessageType::BadRequest;
                    nlohmann::json data = {};
                    data[proto::keys::kRequest] = message.payload;
                    response.payload = data;
                    return response;
                }
                semver::version const clientVersion{
                    message.payload[proto::keys::kVersion].get<std::string>()};

                if (clientVersion < version_.value) {
                    response.type = proto::MessageType::VersionUpdatesAvailable;
                    nlohmann::json payload = {};
                    payload[proto::keys::kVersion] = version_.value.to_string();
                    response.payload = payload;
                }
                return response;
            })
            .onGetUpdates([&](proto::Message &&message) {
                proto::Message response{.type = proto::MessageType::Updates};

                nlohmann::json metricsArray = nlohmann::json::array();

                for (auto &&[k, m] : metrics_) {
                    metricsArray.push_back(m);
                }
                response.payload[proto::keys::kMetrics] = metricsArray;
                response.payload[proto::keys::kVersion] = version_.value.to_string();
                return response;
            })
            .onNotSupported([&](proto::Message&& message){
                proto::Message response{.type = proto::MessageType::BadRequest};
                return response;
            })
            .onPushSettings([&](proto::Message &&message) {
                proto::metrics_umap_t clientMetrics;
                for (auto &&m : message.payload[proto::keys::kMetrics]) {
                    proto::Metric metric = m;
                    clientMetrics.emplace(std::make_pair(metric.name, std::move(metric)));
                }
                std::string error;
                auto const missingMetrics =
                    findMissingMetrics<std::string, proto::Metric>(clientMetrics);

                if (!missingMetrics.empty()) {
                    error = "Missing metrics: ";

                    for (auto &&[m, _] : missingMetrics) {
                        error += m + " ";
                    }
                }
                semver::version const clientVersion{
                    message.payload[proto::keys::kVersion].get<std::string>()};

                if (clientVersion < version_.value) {
                    error += std::string{
                        std::format("| Deprecated version. Your version ({}), the server ({})",
                                    clientVersion.to_string(), version_.value.to_string())};
                }
                proto::Message response{.type = proto::MessageType::Accepted};

                if (!error.empty()) {
                    response.type = proto::MessageType::Deprecated;
                    nlohmann::json payload = {};
                    payload[proto::keys::kVersion] = version_.value.to_string();
                    payload[proto::keys::kError] = error;
                    response.payload = payload;
                }
                return response;
            });
    }

    template <typename K, typename V> struct NotFoundInMapPred {
        using map_t = std::unordered_map<K, V>;
        map_t const &map;

        explicit NotFoundInMapPred(map_t const &other) : map{other} {}

        bool operator()(std::pair<K, V> const &element) const {
            return map.find(element.first) == map.end();
        }
    };

    template <typename K, typename V>
    std::vector<std::pair<K, V>> findMissingMetrics(proto::metrics_umap_t const &clientMetrics) {
        std::vector<std::pair<K, V>> diff;

        std::copy_if(metrics_.begin(), metrics_.end(), std::back_inserter(diff),
                     NotFoundInMapPred<K, V>(clientMetrics));
        return diff;
    }

    void runCLI(std::stop_token stopToken, std::latch &workersLatch) {
        std::string const strPort = std::to_string(port_);
        cmdLineIface_.option({.label = std::format("Update to version {} and notify clients",
                                                   defs::kServerNewVersion),
                              .action = [&] {
                                  updateVersion();
                                  notifyNewVersion();
                              }});

        while (!stopToken.stop_requested()) {
            auto const title = std::string{
                std::format("[MENU] Server (v{}) port: {}", version_.value.to_string(), strPort)};

            if (!cmdLineIface_.tryToExecuteAction(title)) {
                std::cout << "\n\nShutdown has been requested, bye!\n\n";
                quitLock_.test_and_set(std::memory_order_acquire);
                break;
            }
        }
        workersLatch.count_down();
    }

    void runWebService(std::stop_token stopToken, std::latch &workersLatch) {
        namespace fs = std::filesystem;
        using namespace std::chrono_literals;

        fs::path cert = fs::current_path() / defs::ws::kServerCertificate;
        fs::path key = fs::current_path() / defs::ws::kServerKey;

        auto futureApp =
            app_.port(port_).multithreaded().ssl_file(cert.string(), key.string()).run_async();

        do {
            if (auto status = futureApp.wait_for(500ms);
                status == std::future_status::timeout &&
                quitLock_.test(std::memory_order_relaxed)) {

                workersLatch.count_down();
                app_.stop();
                break;
            }
        } while (!stopToken.stop_requested());
    }

    void shutdown() {
        cmdLineIfaceThr_.request_stop();
        webServerThr_.request_stop();
        cmdLineIfaceThr_.join();
        webServerThr_.join();
    }

    void initMetrics() {
        metrics_ = proto::kMetricsDefault;

        // Simulate changes in Metrics for the current server version
        metrics_.insert({"os_name",
                         {.name = "os_name",
                          .description = "Operational system name",
                          .type = proto::MetricType::String}});
    }

    void updateVersion() {
        version_.value = semver::version{defs::kServerNewVersion};
        metrics_.insert({"user_satisfaction",
                         {.name = "user_satisfaction",
                          .description = "The user satisfaction",
                          .type = proto::MetricType::Double}});
    }

    void notifyNewVersion() {
        proto::Message message{.type = proto::MessageType::VersionUpdatesAvailable};
        nlohmann::json payload = {};
        payload[proto::keys::kVersion] = version_.value.to_string();
        message.payload = payload;
        auto const messageStr = proto::toString(message);

        for (auto &&conn : users_) {
            conn->send_text(messageStr);
        }
    }

    proto::Version version_;
    int port_{0};
    crow::SimpleApp app_;
    std::unordered_set<crow::websocket::connection *> users_;
    std::jthread cmdLineIfaceThr_;
    std::jthread webServerThr_;
    std::mutex connectionsMtx_;
    std::atomic_flag quitLock_ = ATOMIC_FLAG_INIT;
    proto::MessageHandler messageHandler_;
    proto::metrics_umap_t metrics_;
    CommandLineInterface cmdLineIface_;
};
} // namespace eps