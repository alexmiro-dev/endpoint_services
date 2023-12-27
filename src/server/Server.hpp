
#include "nt_common/Protocol.hpp"
#include "nt_common/definitions.hpp"

#include <crow.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream> // TODO delete this line
#include <latch>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace nt {

class Server {
public:
    explicit Server(int port)
        : version_{semver::version{defs::kInitialServerVersion}}, port_{port} {
        initMetrics();
        initMessageHandler();

        CROW_ROUTE(app_, "/ws")
            .websocket()
            .onopen([&](crow::websocket::connection &conn) {
                CROW_LOG_INFO << "new websocket connection from " << conn.get_remote_ip();
                std::lock_guard<std::mutex> _{connectionsMtx_};
                users.insert(&conn);
            })
            .onclose([&](crow::websocket::connection &conn, const std::string &reason) {
                CROW_LOG_INFO << "websocket connection closed: " << reason;
                std::lock_guard<std::mutex> _{connectionsMtx_};
                users.erase(&conn);
            })
            .onmessage(
                [&](crow::websocket::connection &conn, const std::string &data, bool isBinary) {
                    std::lock_guard<std::mutex> _{connectionsMtx_};
                    if (!isBinary) {
                        try {
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
        cmdLineInterfaceThr_ = std::jthread([&](std::stop_token st) { runCLI(st, workersLatch); });
        webServerThr_ = std::jthread([&](std::stop_token st) { runWebService(st, workersLatch); });
        workersLatch.wait();
    }

private:
    void initMessageHandler() {
        messageHandler_
            .onVersion([&](proto::Message &&message) {
                proto::Message response;

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

                for (auto&& [k, m] : metrics_) {
                    metricsArray.push_back(m);
                }
                response.payload[proto::keys::kMetrics] = metricsArray;
                auto x = response.payload.dump();
                return response;
            });
    }

    void runCLI(std::stop_token stopToken, std::latch &workersLatch) {
        constexpr auto prompt = "\nEnter a command ('quit' to shutdown the server): ";
        std::string userInput;

        while (!stopToken.stop_requested()) {
            std::cout << prompt;
            std::getline(std::cin, userInput);

            if ("quit" == userInput) {
                std::cout << "\n\nShutdown has been requested, bye!\n\n";
                quitLock_.test_and_set(std::memory_order_acquire);
                break;
            } else {
                std::cout << "\t<Error> Invalid command!\n";
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
        cmdLineInterfaceThr_.request_stop();
        webServerThr_.request_stop();
        cmdLineInterfaceThr_.join();
        webServerThr_.join();
    }

    /**
     * Simulate changes in Metrics for the current server version
     */
    void initMetrics() {
        metrics_ = proto::kMetricsDefault;

        metrics_.insert({ "error",
            {.name = "error", .description = "Latest error", .type = proto::MetricType::String}});
    }

    proto::Version version_;
    int port_{0};
    crow::SimpleApp app_;
    std::unordered_set<crow::websocket::connection *> users;
    std::jthread cmdLineInterfaceThr_;
    std::jthread webServerThr_;
    std::mutex connectionsMtx_;
    std::atomic_flag quitLock_ = ATOMIC_FLAG_INIT;
    proto::MessageHandler messageHandler_;
    proto::metrics_umap_t metrics_;
};

} // namespace nt