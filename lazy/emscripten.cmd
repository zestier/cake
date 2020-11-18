pushd ..
conan config install https://github.com/zstiers/conan-config.git
mkdir .build
pushd .build
mkdir emscripten
pushd emscripten
    conan remove zestier/test -f
    conan create ../.. zestier/test -pr emscripten-wasm --build=outdated -s build_type=Release
    conan install ../.. -pr emscripten-wasm
popd
popd
popd