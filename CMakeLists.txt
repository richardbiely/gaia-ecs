# Build
cmake_minimum_required(VERSION 3.14)

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
message(STATUS "Copyright (c) 2024 Richard Biely <richardbiely@gmail.com>")
message(STATUS "*************************************************************")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "*************************************************************")

include("${CMAKE_CURRENT_LIST_DIR}/cmake/sanitizers.cmake")

# CPU
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" lower_processor)

set(IsARM false)

if("${lower_processor}" MATCHES "arm" OR "${lower_processor}" MATCHES "aarch")
    set(IsARM true)
endif()

# Compiler
set(IsClangCompiler FALSE)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(IsClangCompiler TRUE)
endif()

# Configuration
option(GAIA_USE_LIBCPP "Use libc++." OFF)

if(IsClangCompiler)
    set(GAIA_USE_LIBCPP ON)
endif()

option(GAIA_BUILD_UNITTEST "Build unit test." OFF)
option(GAIA_BUILD_BENCHMARK "Build benchmark." OFF)
option(GAIA_BUILD_EXAMPLES "Build examples." OFF)
option(GAIA_GENERATE_CC "Use ninja to generate compile_commands.json as a post-build step." ON)
option(GAIA_MAKE_SINGLE_HEADER "Make the single file header automatically." OFF)

# Profiling settings
option(GAIA_PROFILER_CPU "Enable CPU profiling." OFF)
option(GAIA_PROFILER_MEM "Enable memory profiling." OFF)
option(GAIA_PROFILER_BUILD "Build profiler if possible." OFF)

# Library configuration
option(GAIA_DEVMODE "Enables various verification checks. Only useful for library maintainers." OFF)
option(GAIA_ECS_CHUNK_ALLOCATOR "If enabled, custom allocator is used for allocating archetype chunks." ON)
option(GAIA_FORCE_DEBUG "If enabled, GAIA_DEBUG will be defined despite using the optimized build configuration." OFF)
option(GAIA_DISABLE_ASSERTS "If enabled, no asserts will be thrown even in debug builds." OFF)

if(GAIA_BUILD_UNITTEST)
    set(GAIA_BUILD_SRC ON)
elseif(GAIA_BUILD_BENCHMARK)
    set(GAIA_BUILD_SRC ON)
elseif(GAIA_BUILD_EXAMPLES)
    set(GAIA_BUILD_SRC ON)
else()
    set(GAIA_BUILD_SRC OFF)
endif()

# libc++
if(NOT WIN32 AND GAIA_USE_LIBCPP)
    # Check for libc++ library presence
    find_library(LIBCXX_LIBRARY c++)

    if(LIBCXX_LIBRARY)
        include(CheckCXXSourceCompiles)
        include(CMakePushCheckState)

        cmake_push_check_state()
        set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -stdlib=libc++")

        check_cxx_source_compiles("
    #include <type_traits>
    int main() { return std::is_same_v<int, char>; }
" GAIA_USE_LIBCPP)

        cmake_pop_check_state()
    endif()

    if(NOT GAIA_USE_LIBCPP)
        message(WARNING "GAIA_USE_LIBCPP is ON but libc++ is not available. The flag will be ignored.")
    endif()
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