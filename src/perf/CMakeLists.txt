# Preprocessor
add_compile_definitions(GAIA_DEBUG=0)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${picobench_SOURCE_DIR}/include)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
link_libraries(Threads::Threads)
link_libraries(picobench::picobench)

add_subdirectory(iter)
add_subdirectory(duel)
add_subdirectory(entity)
add_subdirectory(mt)
add_subdirectory(app)
