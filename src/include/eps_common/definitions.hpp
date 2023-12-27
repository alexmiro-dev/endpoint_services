#pragma once

#include <string>

namespace eps::defs {

static constexpr std::string kInitialServerVersion = "0.1.5";
static constexpr std::string kInitialClientVersion = "0.1.0";

namespace ws {
    static constexpr int kPort = 8'008;
    static constexpr std::string kServerCertificate = "server.crt";
    static constexpr std::string kServerKey = "server.key";
    static constexpr std::string kServerPem = "server.pem";
}

} // namespace eps::defs
