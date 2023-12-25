
#include "nt_common/definitions.hpp"

#include <crow.h>

#include <chrono>
#include <latch>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <filesystem>
#include <atomic>

#include <iostream> // TODO delete this line

namespace nt {

class Server {
public:
    explicit Server(int port) : port_{port} {

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
            .onmessage([&](crow::websocket::connection & /*conn*/, const std::string &data,
                           bool is_binary) {
                std::lock_guard<std::mutex> _{connectionsMtx_};
                //                    for (auto u : users)
                //                        if (is_binary)
                //                            u->send_binary(data);
                //                        else
                //                            u->send_text(data);
            });
    }

    ~Server() {
        shutdown();
    }

    void run() {
        std::latch workersLatch{2U};
        cmdLineInterfaceThr_ = std::jthread([&](std::stop_token st) { runCLI(st, workersLatch); });
        webServerThr_ = std::jthread([&](std::stop_token st) { runWebService(st, workersLatch); });
        workersLatch.wait();
    }

private:
    void runCLI(std::stop_token stopToken, std::latch& workersLatch) {
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

    void runWebService(std::stop_token stopToken, std::latch& workersLatch) {
        namespace fs = std::filesystem;
        using namespace std::chrono_literals;

        fs::path cert = fs::current_path() / defs::ws::kServerCertificate;
        fs::path key = fs::current_path() / defs::ws::kServerKey;

        auto futureApp = app_.port(port_)
            .multithreaded()
            .ssl_file(cert.string(), key.string())
            .run_async();

        do {
            if (auto status = futureApp.wait_for(500ms);
                status == std::future_status::timeout && quitLock_.test(std::memory_order_relaxed)) {
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

    int port_{0};
    crow::SimpleApp app_;
    std::unordered_set<crow::websocket::connection *> users;
    std::jthread cmdLineInterfaceThr_;
    std::jthread webServerThr_;
    std::mutex connectionsMtx_;
    std::atomic_flag quitLock_ = ATOMIC_FLAG_INIT;
};

} // namespace nt