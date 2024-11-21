set -e 

mkdir -p build_aarch64 && pushd build_aarch64
cmake -DCMAKE_BUILD_TYPE=Release                        \
      -DCMAKE_SYSTEM_NAME=Linux                         \
      -DCMAKE_SYSTEM_PROCESSOR=aarch64                  \
      -DCMAKE_C_COMPILER="aarch64-none-linux-gnu-gcc"   \
      -DCMAKE_CXX_COMPILER="aarch64-none-linux-gnu-g++" \
      -DCMAKE_INSTALL_PREFIX=../install/arm64           \
      ..
cmake --build . --target install
popd

mkdir -p build_x64 && pushd build_x64
cmake -DCMAKE_BUILD_TYPE=Release                \
      -DCMAKE_INSTALL_PREFIX=../install/x64     \
      ..
cmake --build . --target install
popd
