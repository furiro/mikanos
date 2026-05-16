#pragma once
#include "msr_index.h"

#define EXIT_REASON_HLT     12
#define EXIT_REASON_WRMSR   32

typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
} VmExitContext;

extern VmxMsrEntry*    VmEnterMsrLoadArea;
extern uint64_t        VmEnterMsrLoadCount;
extern VmxMsrEntry*    VmExitMsrStoreArea;
extern uint64_t        VmExitMsrStoreCount;
extern VmxMsrEntry*    VmExitMsrLoadArea;
extern uint64_t        VmExitMsrLoadCount;


extern "C" {
    void vmexit_entry();
    bool vmexit_handler_msr(VmExitContext* context);
}

