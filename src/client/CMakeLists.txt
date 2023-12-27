
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})

add_executable(eps-client
        main-client.cpp
        Client.hpp
        CommandLineInterface.hpp)
target_compile_definitions(eps-client PRIVATE CROW_ENABLE_SSL)
target_include_directories(eps-client PRIVATE ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(eps-client PRIVATE
        ${Boost_ASIO_LIBRARY}
        ${OPENSSL_LIBRARIES}
        Crow::Crow
        eps::common
        nlohmann_json::nlohmann_json
        semver
        magic_enum::magic_enum
)
