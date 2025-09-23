set(PROJ_NAME "gaia_test")
add_executable(${PROJ_NAME} src/main.cpp)

# System-specific threading library
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJ_NAME} PRIVATE Threads::Threads)

# Unit test framework
target_link_libraries(${PROJ_NAME} PRIVATE doctest::doctest)

# Project files
target_include_directories(${PROJ_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

if(MSVC)
  enable_cxx_compiler_flag_if_supported("/bigobj")
endif()

include(CTest)
enable_testing()
add_test(NAME ${PROJ_NAME} COMMAND ${PROJ_NAME})
