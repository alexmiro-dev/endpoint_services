
#include "Server.hpp"

#include <iostream>
#include <cstdlib>

int main() {
    nt::Server server{nt::defs::ws::kPort};

    server.run();

    return EXIT_SUCCESS;
}
