set(PROJ_NAME "gaia_example_wasm")
project(${PROJ_NAME} LANGUAGES C CXX)

set(SRC_FILE "${CMAKE_CURRENT_LIST_DIR}/src/main.cpp")

# -----------------------------------------------------------------------------
# Detect if we're actually using the Emscripten toolchain
# -----------------------------------------------------------------------------
if(NOT EMSCRIPTEN)
  message(STATUS "Skipping ${PROJ_NAME} — not using emcmake (Emscripten toolchain).")
  return()
endif()

# -----------------------------------------------------------------------------
# Detect if Emscripten is available
# -----------------------------------------------------------------------------
find_program(EMCC_EXECUTABLE emcc)

if(NOT EMCC_EXECUTABLE)
  message(WARNING
    "Emscripten not found on this system. "
    "Skipping WASM example.\n"
    "To build it manually:\n"
    "  git clone https://github.com/emscripten-core/emsdk.git\n"
    "  cd emsdk && ./emsdk install latest && ./emsdk activate latest\n"
    "  source ./emsdk_env.sh\n"
    "Then re-run:\n"
    "  emcmake cmake -S . -B build-wasm && cmake --build build-wasm"
  )
  return()
endif()

# -----------------------------------------------------------------------------
# Normal WASM build
# -----------------------------------------------------------------------------
add_executable(${PROJ_NAME} ${SRC_FILE})

# target_include_directories(${PROJ_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_include_directories(${PROJ_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_compile_options(${PROJ_NAME} PRIVATE
  "-pthread"
  "-matomics"
  "-mbulk-memory"
)

target_link_options(${PROJ_NAME} PRIVATE
  "-pthread"
  "-sASSERTIONS=1"
  "-sMODULARIZE=1"
  "-sEXPORT_NAME=GaiaExampleWASM"
  "-sEXPORTED_FUNCTIONS=_main,_malloc,_free"
  "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap"

  # When using pthreads we need to set environemnt as worker, because threads in WebAssembly are implemented as Web Workers.
  "-sUSE_PTHREADS=1"
  "-sENVIRONMENT=web,worker"

  # If pthread is not necessary we set environment to web
  # "-sENVIRONMENT=web"

  # Memory settings
  "-sINITIAL_MEMORY=256MB"
  "-sALLOW_MEMORY_GROWTH=0"
)

add_custom_command(TARGET ${PROJ_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  "${CMAKE_CURRENT_LIST_DIR}/src/gaia_example_wasm.html"
  "$<TARGET_FILE_DIR:${PROJ_NAME}>/gaia_example_wasm.html"
)

message(STATUS "Gaia-ECS WASM example configured for Emscripten.")