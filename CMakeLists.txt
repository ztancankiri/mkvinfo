cmake_minimum_required(VERSION 3.10)
project(mkvinfo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DQT_DBUS_LIB -DQT_CORE_LIB -Wnon-virtual-dtor -Wextra -Wno-missing-field-initializers -Wunused -Wpedantic -Wno-maybe-uninitialized -Wlogical-op -Wno-extra-semi -Wmisleading-indentation -Wduplicated-cond -Wshadow-compatible-local -Wduplicated-branches -Wno-deprecated-copy -Wno-stringop-overflow")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FILE_OFFSET_BITS=64 -DQT_NO_KEYWORDS")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(fmt REQUIRED)
find_package(pugixml REQUIRED)
find_package(EBML REQUIRED)
find_package(Matroska REQUIRED)

find_package(pybind11 REQUIRED)
find_package(Python COMPONENTS Interpreter Development)

include_directories("include" "lib" ${Python_INCLUDE_DIRS})

file(GLOB_RECURSE SOURCES src/*.cpp)

pybind11_add_module(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE
    fmt::fmt
    Matroska::matroska
    EBML::ebml
    pugixml::pugixml
    Python::Python
    "-static-libgcc"
    "-static-libstdc++"
)