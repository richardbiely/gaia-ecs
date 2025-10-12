set(PROJ_NAME "gaia_example3")
add_executable(${PROJ_NAME} src/main.cpp)

target_include_directories(${PROJ_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJ_NAME} PRIVATE Threads::Threads)