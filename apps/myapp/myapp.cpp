#include <libver.hpp>
#include "../syscall.h"
#include <cstdio>

extern "C" void main() {
    int pl_number;
    int cpu_number = 1;

    printf("0 < 1 < 2 < 0 \n");
    while(true) {

        printf("Enter a player number: ");
        scanf("%d", &pl_number); // to make sure that the C++ library is linked
        printf("You entered %d\n", pl_number);
        if (pl_number < 0 || pl_number > 2) {
            printf("Number out of bounds. Please enter a number between 0 and 2.\n");
            continue;
        }

        cpu_number = GetCpuNumber(pl_number);
        printf("CPU number is %d\n", cpu_number);
        if (cpu_number == pl_number) {
            printf("Draw\n");
            continue;
        }

        if ((pl_number == 0 && cpu_number == 2) ||
            (pl_number == 1 && cpu_number == 0) ||
            (pl_number == 2 && cpu_number == 1)) {
            printf("You win!\n");
            break;
        } else {
            printf("You lose!\n");
            break;
        }
    }
    SyscallExit(0);
}
