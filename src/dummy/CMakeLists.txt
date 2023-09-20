set(PROJ_NAME "gaia_dummy")
add_executable(${PROJ_NAME} src/main.cpp)

# System-specific threading library
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJ_NAME} PRIVATE Threads::Threads)

# Project files
target_include_directories(${PROJ_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)