
set(Boost_USE_STATIC_LIBS ON)

find_package(Boost REQUIRED COMPONENTS system thread regex)
find_package(OpenSSL REQUIRED)

include(FetchContent)

##################################################
# Crow for Websocket
##################################################
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

##################################################
# JSON
##################################################
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(nlohmann_json)

##################################################
# Semantic Version
##################################################
FetchContent_Declare(
        semver
        GIT_REPOSITORY https://github.com/Neargye/semver.git
        GIT_TAG v0.3.0
)

FetchContent_MakeAvailable(semver)

##################################################
# Magic enum
##################################################
FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG v0.9.5
)

FetchContent_MakeAvailable(magic_enum)

add_library(common INTERFACE
        include/nt_common/definitions.hpp
        include/nt_common/Protocol.hpp
)

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