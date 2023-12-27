
#include "Server.hpp"

#include <cstdlib>

int main() {
    eps::Server server{eps::defs::ws::kPort};

    server.run();

    return EXIT_SUCCESS;
}
