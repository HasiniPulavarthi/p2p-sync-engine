#!/usr/bin/env bash
# Manual build fallback (used to originally verify this project since the
# sandbox it was built in didn't have cmake installed). Prefer the
# CMakeLists.txt / ctest workflow described in README.md when available.
set -euo pipefail
cd "$(dirname "$0")"

mkdir -p build/obj
CORE="sha256 chunker merkle_tree vector_clock entry_codec crdt_store file_store sync_engine network node"

for f in $CORE; do
  g++ -std=c++17 -O2 -Wall -Wextra -Iinclude -c "src/$f.cpp" -o "build/obj/$f.o"
done
ar rcs build/libsync_core.a build/obj/*.o

g++ -std=c++17 -O2 -Wall -Wextra -Iinclude -c src/main.cpp -o build/obj/main.o
g++ -std=c++17 build/obj/main.o build/libsync_core.a -lpthread -o build/syncnode

for t in test_chunker test_merkle test_crdt; do
  g++ -std=c++17 -O2 -Wall -Wextra -Iinclude -c "tests/$t.cpp" -o "build/obj/$t.o"
  g++ -std=c++17 "build/obj/$t.o" build/libsync_core.a -lpthread -o "build/$t"
done

echo "Built: build/syncnode, build/test_chunker, build/test_merkle, build/test_crdt"
echo "Run tests with: build/test_chunker && build/test_merkle && build/test_crdt"
