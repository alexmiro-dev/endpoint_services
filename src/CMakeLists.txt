
set(Boost_USE_STATIC_LIBS ON)

find_package(Boost REQUIRED COMPONENTS system thread regex)
find_package(OpenSSL REQUIRED)

include(FetchContent)

FetchContent_Declare(
        Crow
        GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
        GIT_TAG        v1.0+5
)

FetchContent_MakeAvailable(crow)
FetchContent_GetProperties(crow)

if(NOT crow_POPULATED)
    FetchContent_Populate(crow)
    add_subdirectory(${crow_SOURCE_DIR} ${crow_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()


add_library(common INTERFACE)
add_library(nt::common ALIAS common)

set_property(TARGET common PROPERTY EXPORT_NAME nt_common)

target_include_directories(
        common
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

add_subdirectory(client)
add_subdirectory(server)
