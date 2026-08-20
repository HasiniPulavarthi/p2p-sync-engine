cmake_minimum_required(VERSION 3.14)
project(p2p_sync_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2")
add_compile_options(-Wall -Wextra)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(CORE_SOURCES
    src/sha256.cpp
    src/chunker.cpp
    src/merkle_tree.cpp
    src/vector_clock.cpp
    src/entry_codec.cpp
    src/crdt_store.cpp
    src/file_store.cpp
    src/sync_engine.cpp
    src/network.cpp
    src/node.cpp
)

add_library(sync_core STATIC ${CORE_SOURCES})

find_package(Threads REQUIRED)
target_link_libraries(sync_core PUBLIC Threads::Threads)

add_executable(syncnode src/main.cpp)
target_link_libraries(syncnode PRIVATE sync_core)

# --- Tests (plain-assert executables, no external framework needed) ---
enable_testing()

add_executable(test_chunker tests/test_chunker.cpp)
target_link_libraries(test_chunker PRIVATE sync_core)
add_test(NAME ChunkerTests COMMAND test_chunker)

add_executable(test_merkle tests/test_merkle.cpp)
target_link_libraries(test_merkle PRIVATE sync_core)
add_test(NAME MerkleTests COMMAND test_merkle)

add_executable(test_crdt tests/test_crdt.cpp)
target_link_libraries(test_crdt PRIVATE sync_core)
add_test(NAME CrdtTests COMMAND test_crdt)
