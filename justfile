set shell := ["zsh", "-f", "-eu", "-o", "pipefail", "-c"]
set positional-arguments

architecture := replace(replace(arch(), "aarch64", "arm64"), "x86_64", "x64")
system := if os() == "macos" { "osx" } else { os() }
triplet := f'{{architecture}}-{{system}}'
cxx := env("CXX", if os() == "macos" { "/usr/bin/clang++" } else { "clang++" })

_:
    @just --list

deps:
    vcpkg install --triplet={{triplet}} --x-install-root=build/deps --x-wait-for-lock

configure configuration="Debug": deps
    CXX="{{cxx}}" cmake -S . -B build/cmake/{{configuration}} -G Ninja \
        -DCMAKE_BUILD_TYPE={{configuration}} \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
        -DVCPKG_MANIFEST_MODE=OFF \
        -DVCPKG_INSTALLED_DIR="{{justfile_directory()}}/build/deps" \
        -DVCPKG_TARGET_TRIPLET={{triplet}} \
        -DENABLE_SANITIZERS={{if configuration == "Debug" { "ON" } else { "OFF" }}}

test: (configure "Debug")
    cmake --build build/cmake/Debug --parallel 2
    ctest --test-dir build/cmake/Debug --output-on-failure --no-tests=error

build: (configure "Release")
    cmake --build build/cmake/Release --parallel 2
    ctest --test-dir build/cmake/Release --output-on-failure --no-tests=error

lint: (configure "Debug")
    "${CLANG_TIDY:-clang-tidy}" -p build/cmake/Debug src/main.cpp tests/test_main.cpp

test-mcp: build
    node scripts/test-mcp.mjs build/bin/shop-assistant-mcp search,offer,reviews,page,themes,open,navigate,click,fill,fill_form,press_key,wait_for,evaluate,snapshot,screen,screenshot,tab_open,tab_close,status

install:
    node scripts/homebrew.mjs install

install-local:
    node scripts/homebrew.mjs install-local

reinstall-local:
    node scripts/homebrew.mjs reinstall-local

start:
    node scripts/homebrew.mjs start

stop:
    node scripts/homebrew.mjs stop

restart:
    node scripts/homebrew.mjs restart

status:
    node scripts/homebrew.mjs status

uninstall:
    node scripts/homebrew.mjs uninstall

run *args:
    ./build/bin/shop-assistant-mcp "$@"

clean:
    rm -rf build Formula/.staging
