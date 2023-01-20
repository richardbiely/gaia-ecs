enable_testing()

if(GAIA_FIND_CATCH2_PACKAGE)
  Include(FetchContent)

  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v2.13.7
  )

  FetchContent_GetProperties(Catch2)

  if(NOT catch2_POPULATED)
    FetchContent_Populate(Catch2)
    add_subdirectory(${catch2_SOURCE_DIR} ${catch2_BINARY_DIR})
  endif()

  # Shortcut for the above commands but only available since 3.14
  # FetchContent_MakeAvailable(Catch2)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/contrib)
else()
  find_package(Catch2 CONFIG REQUIRED 2)
endif()

add_executable(gaia_test src/main.cpp)

# Unit test framework
target_link_libraries(gaia_test PRIVATE Catch2::Catch2)

# Project files
target_include_directories(gaia_test PRIVATE ${PROJECT_SOURCE_DIR}/include)

include(CTest)
include(Catch)
catch_discover_tests(gaia_test)
