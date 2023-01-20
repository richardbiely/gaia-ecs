# Build
cmake_minimum_required(VERSION 3.12)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Prevent building in-tree
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "Prevented in-tree built in ${CMAKE_SOURCE_DIR}. Create a 'build' directory outside of the source code and call cmake from there.")
endif()

set(GAIA_VERSION_REGEX "#define GAIA_VERSION_.*[ \t]+(.+)")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/gaia/config/version.h" GAIA_VERSION REGEX ${GAIA_VERSION_REGEX})
list(TRANSFORM GAIA_VERSION REPLACE ${GAIA_VERSION_REGEX} "\\1")
string(JOIN "." GAIA_VERSION ${GAIA_VERSION})

project(
    gaia
    VERSION ${GAIA_VERSION}
    DESCRIPTION "ECS framework"
    LANGUAGES CXX
)

message(STATUS "*************************************************************")
message(STATUS "${PROJECT_NAME}-ecs v${PROJECT_VERSION} (${CMAKE_BUILD_TYPE})")
message(STATUS "Copyright (c) 2021 Richard Biely <richardbiely@gmail.com>")
message(STATUS "*************************************************************")

set(IsClangCompiler CMAKE_CXX_COMPILER_ID MATCHES "Clang")

include("${CMAKE_CURRENT_LIST_DIR}/cmake/sanitizers.cmake")

# Configuration
option(GAIA_USE_LIBCPP "Use libc++." IsClangCompiler) # ON for Clang by default
option(GAIA_BUILD_UNITTEST "Build unit test." OFF)
option(GAIA_BUILD_BENCHMARK "Build benchmark." OFF)
option(GAIA_BUILD_EXAMPLES "Build examples." OFF)
option(GAIA_GENERATE_CC "Use ninja to generate compile_commands.json as a post-build step." ON)

if(GAIA_BUILD_UNITTEST)
    set(GAIA_BUILD_SRC ON)
elseif(GAIA_BUILD_BENCHMARK)
    set(GAIA_BUILD_SRC ON)
elseif(GAIA_BUILD_EXAMPLES)
    set(GAIA_BUILD_SRC ON)
else()
    set(GAIA_BUILD_SRC OFF)
endif()

if(NOT WIN32 AND GAIA_USE_LIBCPP)
    include(CheckCXXSourceCompiles)
    include(CMakePushCheckState)

    cmake_push_check_state()

    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -stdlib=libc++")

    check_cxx_source_compiles("
        #include<type_traits>
        int main() { return std::is_same_v<int, char>; }
    " GAIA_HAS_LIBCPP)

    if(NOT GAIA_HAS_LIBCPP)
        message(WARNING "GAIA_USE_LIBCPP is set however libc++ is not available. The flag will be ignored.")
    endif()

    cmake_pop_check_state()
endif()

# Gaia target
include(GNUInstallDirs)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(
    ${PROJECT_NAME}
    INTERFACE
    $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(GAIA_HAS_LIBCPP)
    target_compile_options(${PROJECT_NAME} BEFORE INTERFACE -stdlib=libc++)
endif()

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)

# Install
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}_Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

include(CMakePackageConfigHelpers)
write_basic_package_version_file("${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

if(NOT INCLUDE_INSTALL_DIR)
    set(INCLUDE_INSTALL_DIR include/gaia)
endif()

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake
    PATH_VARS INCLUDE_INSTALL_DIR)

install(EXPORT ${PROJECT_NAME}_Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

if(GAIA_BUILD_SRC)
    add_subdirectory(src)
endif()

# Generated compile_commands.json
if(GAIA_GENERATE_CC)
    add_custom_target(generate_compile_commands ALL)
    add_custom_command(
        TARGET generate_compile_commands POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -G "Ninja" -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_SOURCE_DIR}/ninja
    )
endif()