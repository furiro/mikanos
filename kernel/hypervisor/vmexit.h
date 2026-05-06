#pragma once


#define EXIT_REASON_HLT     12
#define EXIT_REASON_WRMSR   32

extern "C" {
    void vmexit_entry();
}

