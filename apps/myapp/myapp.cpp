#include <libver.hpp>
#include "../syscall.h"
#include <cstdio>

extern "C" void main() {
    GetVersion();
    printf("This is %x version\n", GetVersion());
    SyscallExit(static_cast<int>(GetVersion()));
}