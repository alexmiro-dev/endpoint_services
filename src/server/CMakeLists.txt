
add_executable(_nt-server main-server.cpp Server.hpp)

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})

target_compile_definitions(_nt-server PRIVATE CROW_ENABLE_SSL)

target_include_directories(_nt-server PRIVATE ${OPENSSL_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(_nt-server PRIVATE ${Boost_ASIO_LIBRARY} ${OPENSSL_LIBRARIES} Crow::Crow nt::common)
