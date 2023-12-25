enable_testing()

FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

include(CTest)
include(Catch)

set(_test_sources
)

foreach(_name ${_test_sources})
    add_executable(${_name} ${_name}.cpp)
endforeach(_name ${_test_sources})

foreach(_name ${_test_sources})
    target_link_libraries(${_name} PRIVATE Catch2::Catch2WithMain)
    target_include_directories(${_name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    set_property(TARGET ${_name} PROPERTY CXX_STANDARD 23)
    set_property(TARGET ${_name} PROPERTY CXX_EXTENSIONS OFF)
endforeach(_name ${_test_sources})
