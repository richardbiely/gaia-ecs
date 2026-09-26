set(PROJ_NAME "gaia_example_query_jobs")
add_executable(${PROJ_NAME} src/main.cpp)

target_include_directories(${PROJ_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJ_NAME} PRIVATE Threads::Threads)

if(BUILD_TESTING)
    add_test(NAME ${PROJ_NAME} COMMAND $<TARGET_FILE:${PROJ_NAME}>)
endif()
