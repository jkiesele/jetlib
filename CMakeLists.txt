#----------------------------------------------------------------------------
# Setup the project
#
cmake_minimum_required(VERSION 3.16...3.21)
project(jetlib)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
include_directories(${PROJECT_SOURCE_DIR}/interface)

list(APPEND CMAKE_PREFIX_PATH $ENV{ROOTSYS})
find_package(ROOT REQUIRED COMPONENTS RIO Tree)
include("${ROOT_USE_FILE}")
include_directories(${ROOT_INCLUDE_DIR})

#----------------------------------------------------------------------------
# Find Pyhia8 
#
find_package(Pythia8 REQUIRED)
include_directories(${PYTHIA8_INCLUDE_DIRS})
#----------------------------------------------------------------------------
# Find FastJet using fastjet-config: from FASTJET_DIR (cmake -DFASTJET_DIR=...),
# otherwise from PATH
#
set(FASTJET_DIR "/opt/fastjet-3.5.0" CACHE PATH "FastJet installation prefix (contains bin/fastjet-config)")
find_program(FASTJET_CONFIG fastjet-config PATHS ${FASTJET_DIR}/bin)
if(NOT FASTJET_CONFIG)
    message(FATAL_ERROR "fastjet-config not found; set -DFASTJET_DIR=<FastJet prefix> or add it to PATH")
endif()
message(STATUS "FastJet: ${FASTJET_CONFIG}")

# Get FastJet compilation flags
execute_process(COMMAND ${FASTJET_CONFIG} --cxxflags
                OUTPUT_VARIABLE FASTJET_CXX_FLAGS
                OUTPUT_STRIP_TRAILING_WHITESPACE)

# Get FastJet library flags
execute_process(COMMAND ${FASTJET_CONFIG} --libs
                OUTPUT_VARIABLE FASTJET_LIBRARIES
                OUTPUT_STRIP_TRAILING_WHITESPACE)

# Get FastJet include directory
execute_process(COMMAND ${FASTJET_CONFIG} --prefix
                OUTPUT_VARIABLE FASTJET_PREFIX
                OUTPUT_STRIP_TRAILING_WHITESPACE)

set(FASTJET_INCLUDE_DIR "${FASTJET_PREFIX}/include")
include_directories(${FASTJET_INCLUDE_DIR})


#----------------------------------------------------------------------------
# Locate sources and headers for this project
# NB: headers are included so they will show up in IDEs
#
file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB headers ${PROJECT_SOURCE_DIR}/interface/*.h)

#----------------------------------------------------------------------------
# Python module jetlibpy (needs Python 3 development files and the pybind11
# submodule: git submodule update --init). -DJETLIB_PYTHON=OFF builds only the CLI.
#
option(JETLIB_PYTHON "Build the Python module jetlibpy" ON)
if(JETLIB_PYTHON)
    if(NOT EXISTS ${PROJECT_SOURCE_DIR}/extern/pybind11/CMakeLists.txt)
        message(FATAL_ERROR "extern/pybind11 is empty: run 'git submodule update --init', "
                            "or configure with -DJETLIB_PYTHON=OFF to build only the jetlib executable")
    endif()
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    add_subdirectory(extern/pybind11)
    file(GLOB_RECURSE BIND_SOURCES "${PROJECT_SOURCE_DIR}/bind/*.cc")
    pybind11_add_module(jetlibpy ${BIND_SOURCES} ${sources})
    target_link_libraries(jetlibpy PRIVATE ${PYTHIA8_LIBRARIES} ${FASTJET_LIBRARIES} ${ROOT_LIBRARIES})
endif()

#----------------------------------------------------------------------------
# The jetlib executable
#

# Git version of the sources, regenerated on every build (only rewritten when it changes)
set(JETLIB_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
add_custom_target(jetlib_version
    COMMAND ${CMAKE_COMMAND} -DSRC=${PROJECT_SOURCE_DIR} -DOUT=${JETLIB_GENERATED_DIR}/JetlibVersion.h
            -P ${PROJECT_SOURCE_DIR}/cmake/GitVersion.cmake
    BYPRODUCTS ${JETLIB_GENERATED_DIR}/JetlibVersion.h)

add_executable(jetlib jetlib.cc ${sources} ${headers})
add_dependencies(jetlib jetlib_version)
target_include_directories(jetlib PRIVATE ${JETLIB_GENERATED_DIR})
# default config file: config/ttbar.cfg of this source tree
target_compile_definitions(jetlib PRIVATE JETLIB_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(jetlib ${PYTHIA8_LIBRARIES} ${FASTJET_LIBRARIES} ${ROOT_LIBRARIES})

#----------------------------------------------------------------------------
# Install the executable to 'bin' directory under CMAKE_INSTALL_PREFIX
#

# Install the main executable as well
install(TARGETS jetlib DESTINATION bin)
