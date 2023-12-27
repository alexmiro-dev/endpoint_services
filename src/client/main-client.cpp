
#include "Client.hpp"

#include <cstdlib>
#include <iostream>

namespace fs = std::filesystem;

void loadRootCertificate(boost::asio::ssl::context &ctx, fs::path const &certificate) {
    if (!fs::exists(certificate) || !fs::is_regular_file(certificate)) {
        throw std::runtime_error(
            std::format("Invalid server certificate [{}]", certificate.string()));
    }
    if (std::ifstream certFile(certificate.string()); certFile.is_open()) {
        std::string certContents{(std::istreambuf_iterator<char>(certFile)),
                                 std::istreambuf_iterator<char>()};

        boost::system::error_code ec;
        ctx.add_certificate_authority(boost::asio::buffer(certContents.data(), certContents.size()),
                                      ec);
        if (ec) {
            throw std::runtime_error(
                std::format("Unable to obtain the certificate authority from the "
                            "server certificate [{}]",
                            certificate.string()));
        }
    } else {
        throw std::runtime_error(
            std::format("Unable to open the server certificate [{}]", certificate.string()));
    }
}

int main() {
    fs::path const sslServerCertificate = fs::current_path() / eps::defs::ws::kServerCertificate;

    boost::asio::io_context ioContext;
    boost::asio::ssl::context sslContext{ssl::context::sslv23};

    try {
        loadRootCertificate(sslContext, sslServerCertificate.string());

    } catch (std::exception const &ex) {
        std::cerr << "FATAL: cannot start the client: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    std::make_shared<eps::Client>(ioContext, sslContext)->run("localhost", eps::defs::ws::kPort);

    // Run the I/O service. The call will return when the socket is closed.
    ioContext.run();

    return EXIT_SUCCESS;
}
