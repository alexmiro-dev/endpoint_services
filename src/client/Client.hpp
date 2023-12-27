
#include "CommandLineInterface.hpp"
#include "nt_common/Protocol.hpp"
#include "nt_common/definitions.hpp"

#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <semver.hpp>

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
namespace fs = std::filesystem;
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

namespace nt {

// Report a failure
void fail(beast::error_code ec, char const *what) {
    std::cerr << what << ": " << ec.message() << "\n";
}

/**
 * Implements a client to connect to a Webservice using SSL
 *
 * @note: This implementation is a fork from the Boost example. Here:
 *        https://www.boost.org/doc/libs/1_70_0/libs/beast/example/websocket/client/async-ssl/websocket_client_async_ssl.cpp
 */
class Client : public std::enable_shared_from_this<Client> {
public:
    /**
     *
     * @param ioc
     * @param ioc
     */
    explicit Client(net::io_context &ioc, ssl::context &ctx)
        : version_{semver::version{defs::kInitialClientVersion}}
        , resolver_{net::make_strand(ioc)}
        , ws_{net::make_strand(ioc), ctx} {

        init();
    }

    // Start the asynchronous operation
    void run(std::string_view host, int port) {
        host_ = host.data();
        port_ = port;
        auto const strPort = std::to_string(port);

        // Look up the domain name
        resolver_.async_resolve(host, strPort,
                                beast::bind_front_handler(&Client::onResolve, shared_from_this()));
    }

private:
    void onResolve(beast::error_code ec, tcp::resolver::results_type results) {
        if (ec) {
            return fail(ec, "resolve");
        }
        // Set a timeout on the operation
        beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(ws_).async_connect(
            results, beast::bind_front_handler(&Client::onConnect, shared_from_this()));
    }

    void onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
        if (ec) {
            return fail(ec, "connect");
        }
        // Set a timeout on the operation
        beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

        // Perform the SSL handshake
        ws_.next_layer().async_handshake(
            ssl::stream_base::client,
            beast::bind_front_handler(&Client::onSSLHandshake, shared_from_this()));
    }

    void onSSLHandshake(beast::error_code ec) {
        if (ec) {
            return fail(ec, "ssl_handshake");
        }
        // Turn off the timeout on the tcp_stream, because
        // the websocket stream has its own timeout system.
        beast::get_lowest_layer(ws_).expires_never();

        // Set suggested timeout settings for the websocket
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

        // Set a decorator to change the User-Agent of the handshake
        ws_.set_option(websocket::stream_base::decorator([](websocket::request_type &req) {
            req.set(http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-async-ssl");
        }));

        // Perform the websocket handshake
        ws_.async_handshake(host_, "/ws",
                            beast::bind_front_handler(&Client::onHandshake, shared_from_this()));
    }

    void onHandshake(beast::error_code ec) {
        if (ec) {
            return fail(ec, "handshake");
        }
        wsInteractionThr_ = std::jthread([&](std::stop_token stopToken) { mainLoop(stopToken); });
        wsInteractionThr_.join();
    }

    void mainLoop(std::stop_token stopToken) {
        beast::flat_buffer b;

        auto const strPort = std::to_string(port_);

        while (!stopToken.stop_requested()) {
            if (!cmdLineIface_.tryToExecuteAction(version_.value.to_string(), host_, strPort)) {
                // User has chosen to quit the application.
                break;
            }
            ws_.read(b);
            try {
                nlohmann::json const dataJson =
                    nlohmann::json::parse(beast::buffers_to_string(b.data()));
                b.clear();
                auto received = proto::toMessage(dataJson);

                if (auto const response = messageHandler_.process(std::move(received)); response) {
                    ws_.write(net::buffer(proto::toString(response.value())));
                }
            } catch (nlohmann::json::exception const &ex) {
                // TODO: log the error but do nothing. The server should not send any malformed
                // message.
            }
        }
    }

    void init() {
        cmdLineIface_
            .option(
                {.label = "Check the server version", .action = [&] { requestServerVersion(); }})
            .option({.label = "Get updates", .action = [&] { requestUpdates(); }})
            .option({.label = "Push settings changes", .action = [&] { requestPushSettings(); }});

        messageHandler_
            .onVersionUpdatesAvailable([&](proto::Message &&message) {
                auto const strVersion = message.payload[proto::keys::kVersion].get<std::string>();
                std::cout << "\n\n**Attention** A new version is available: " << strVersion << "\n";
                return std::nullopt;
            })
            .onUpdates([&](proto::Message &&message) {
                // Should check better if the metrics we are receiving are valid but let's trust in
                // our server to make things easier
                metrics_.clear();

                auto const strVersion = message.payload[proto::keys::kVersion].get<std::string>();
                version_.value = semver::version{strVersion};

                for (auto &&m : message.payload[proto::keys::kMetrics]) {
                    proto::Metric metric = m;
                    metrics_.emplace(std::make_pair(metric.name, std::move(metric)));
                }
                std::cout << "\n\nThe metrics has been updated\n\n";
                return std::nullopt;
            })
            .onDeprecated([&](proto::Message &&message) {
                auto const strError = message.payload[proto::keys::kError].get<std::string>();
                std::cout << std::format("\n\n(ERROR) {}  <Update your version!>\n\n", strError);
                return std::nullopt;
            })
            .onAccepted([&](proto::Message &&message) {
                std::cout << "\n\nYour last request was accepted!\n\n";
                return std::nullopt;
            });
    }

    void requestServerVersion() {
        nlohmann::json data = nlohmann::json::object();
        data[proto::keys::kVersion] = version_.value.to_string();

        proto::Message request{.type = proto::MessageType::Version, .payload = data};
        ws_.write(net::buffer(proto::toString(request)));
    }

    void requestUpdates() {
        proto::Message request{.type = proto::MessageType::GetUpdates};
        ws_.write(net::buffer(proto::toString(request)));
    }

    void requestPushSettings() {
        proto::Message request{.type = proto::MessageType::PushSettings};
        nlohmann::json metricsArray = nlohmann::json::array();

        for (auto &&[k, m] : metrics_) {
            metricsArray.push_back(m);
        }
        request.payload[proto::keys::kMetrics] = metricsArray;
        request.payload[proto::keys::kVersion] = version_.value.to_string();
        ws_.write(net::buffer(proto::toString(request)));
    }

    proto::Version version_;
    tcp::resolver resolver_;
    websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws_;
    beast::flat_buffer buffer_;
    proto::MessageHandler messageHandler_;
    CommandLineInterface cmdLineIface_;
    std::jthread wsInteractionThr_;
    proto::metrics_umap_t metrics_ = proto::kMetricsDefault;
    std::string host_;
    int port_;
};

} // namespace nt
