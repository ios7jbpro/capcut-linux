#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
if [[ $# -gt 0 ]]; then
    echo "Usage: $0 (no arguments)"
    echo "CAPCUT_SKIP_TESTS=1 builds without running tests."
    echo "CAPCUT_TEST_PRESENT_GPU=1 additionally runs the Vulkan presentation test."
    exit 0
fi
mkdir -p build
llvm-dlltool -m i386:x86-64 -d kernel32.def -l build/kernel32.lib
clang --target=x86_64-pc-windows-msvc -c -O2 -ffreestanding -fno-builtin -fno-stack-protector -Wall -Wextra -Werror compat.c -o build/compat.obj
clang --target=x86_64-pc-windows-msvc -c forward.s -o build/forward.obj
lld-link /dll /entry:DllMain /nodefaultlib /machine:x64 /dynamicbase /nxcompat /out:build/version.dll /def:version.def build/compat.obj build/forward.obj build/kernel32.lib
gcc -shared -fPIC -O2 -Wall -Wextra -Werror -o build/libcapcut_xcopy.so xcopy_compat.c -ldl -pthread
gcc -O2 -Wall -Wextra -Werror -o build/xcopy_test xcopy_test.c -lX11

gcc -O2 -Wall -Wextra -Werror -o build/xrender_test xrender_test.c -lX11 -ldl
gcc -O2 -Wall -Wextra -Werror -o build/gc_test gc_test.c -lX11
gcc -shared -fPIC -O2 -o build/liba.so dlsym-test/a.c -ldl
gcc -shared -fPIC -O2 -o build/libb.so dlsym-test/b.c
gcc -O2 -o build/dlsym_test dlsym-test/main.c -Lbuild -la -lb -Wl,-rpath,'$ORIGIN'
objdump -d --disassemble=dlsym build/libcapcut_xcopy.so > build/dlsym-disassembly.txt
gcc -O2 -Wall -Wextra -Werror -o build/present_state_test present_state_test.c -ldl -pthread
gcc -O2 -Wall -Wextra -Werror -o build/present_test present_test.c -lX11 -ldl

echo "Built: $PWD/build/version.dll and $PWD/build/libcapcut_xcopy.so"
if [[ "${CAPCUT_SKIP_TESTS:-0}" == 1 ]]; then exit 0; fi
build/present_state_test
if [[ -z ${DISPLAY:-} ]]; then
    echo "X11 tests require DISPLAY. Use xvfb-run -a ./build.sh, or CAPCUT_SKIP_TESTS=1 ./build.sh for build-only." >&2
    exit 1
fi
CAPCUT_XCOPY_COMPAT=1 LD_PRELOAD="$PWD/build/libcapcut_xcopy.so" build/xcopy_test
CAPCUT_XCOPY_COMPAT=1 LD_PRELOAD="$PWD/build/libcapcut_xcopy.so" build/xrender_test
CAPCUT_XCOPY_COMPAT=1 LD_PRELOAD="$PWD/build/libcapcut_xcopy.so" build/gc_test
CAPCUT_XCOPY_COMPAT=1 LD_PRELOAD="$PWD/build/libcapcut_xcopy.so:$PWD/build/liba.so:$PWD/build/libb.so" build/dlsym_test
if [[ "${CAPCUT_TEST_PRESENT_GPU:-0}" == 1 ]]; then
    CAPCUT_XCOPY_COMPAT=1 CAPCUT_PRESENT_COMPAT=1 LD_PRELOAD="$PWD/build/libcapcut_xcopy.so" build/present_test
fi
