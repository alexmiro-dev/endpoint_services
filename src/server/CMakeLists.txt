
add_executable(eps-server main-server.cpp Server.hpp)

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})

target_compile_definitions(eps-server PRIVATE CROW_ENABLE_SSL)

target_include_directories(eps-server PRIVATE ${OPENSSL_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(eps-server PRIVATE
        ${Boost_ASIO_LIBRARY}
        ${OPENSSL_LIBRARIES}
        Crow::Crow
        eps::common
        nlohmann_json::nlohmann_json
        semver
        magic_enum::magic_enum
)
