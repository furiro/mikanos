
#include "hypervisor.hpp"
#include "vmexit.h"
#include "asms.hpp"
#include "msr_index.h"
#include "../msr.hpp"
#include "../logger.hpp"
#include "../memory_manager.hpp"

#include <stdint.h>
#include <cstring>


VmxMsrEntry*    VmEnterMsrLoadArea  = nullptr;   // Guest Msr on VM-entry
uint64_t        VmEnterMsrLoadCount = 0;
VmxMsrEntry*    VmExitMsrStoreArea  = nullptr;   // Guest Msr on VM-exit
uint64_t        VmExitMsrStoreCount = 0;
VmxMsrEntry*    VmExitMsrLoadArea   = nullptr;   // Host Msr on VM-exit
uint64_t        VmExitMsrLoadCount  = 0;

bool vmexit_handler_msr(VmExitContext* context) {
    bool result = true;
    uint32_t msr = static_cast<uint32_t>(context->rcx);
    uint64_t value = ((context->rdx & 0xffffffffull) << 32) | (context->rax & 0xffffffffull);
    Log(kInfo, "Guest executed WRMSR: msr=0x%x value=0x%llx\n", msr, value);
    switch (msr)
    {
    case kIA32_EFER:
        result &= vmwrite_checked(GUEST_IA32_EFER, value);
        break;
    
    default:
        uint64_t i = 0;
        //
        // For Host State
        //
        for (i = 0; i < VmExitMsrLoadCount; ++i) {
            if (VmExitMsrLoadArea[i].msr_index == msr) {
                // If ther MSR is already in the VM-exit load area, need not update.
                break;
            }
        }
        if (i == VmExitMsrLoadCount) {  // if not found, add a new entry
            VmExitMsrLoadArea[VmExitMsrLoadCount].msr_index = msr;
            VmExitMsrLoadArea[VmExitMsrLoadCount].reserved = 0;
            VmExitMsrLoadArea[VmExitMsrLoadCount].value = rdmsr(msr);
            ++VmExitMsrLoadCount;
            result &= vmwrite_checked(VM_EXIT_MSR_LOAD_COUNT, VmExitMsrLoadCount);
        }

        //
        // For Guest State
        //
        // if the MSR is in the VM-entry load area, update the value to be loaded on VM-entry
        for (i = 0; i < VmEnterMsrLoadCount; ++i) {
            if (VmEnterMsrLoadArea[i].msr_index == msr) {
                VmEnterMsrLoadArea[i].value = value;
                break;
            }
        }
        if (i == VmEnterMsrLoadCount) { // if not found, add a new entry
            VmEnterMsrLoadArea[VmEnterMsrLoadCount].msr_index = msr;
            VmEnterMsrLoadArea[VmEnterMsrLoadCount].reserved = 0;
            VmEnterMsrLoadArea[VmEnterMsrLoadCount].value = value;
            ++VmEnterMsrLoadCount;
            result &= vmwrite_checked(VM_ENTRY_MSR_LOAD_COUNT, VmEnterMsrLoadCount);
        }

        break;
    }
    return result;
}
