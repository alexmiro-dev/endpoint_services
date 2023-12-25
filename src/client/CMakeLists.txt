
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})

add_executable(_nt-client main-client.cpp Client.hpp)
target_compile_definitions(_nt-client PRIVATE CROW_ENABLE_SSL)
target_include_directories(_nt-client PRIVATE ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(_nt-client PRIVATE ${Boost_ASIO_LIBRARY} ${OPENSSL_LIBRARIES} Crow::Crow nt::common)
