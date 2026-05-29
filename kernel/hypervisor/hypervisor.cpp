#include "hypervisor.hpp"
#include "asms.hpp"
#include "../memory_manager.hpp"
#include "../logger.hpp"
#include "../segment.hpp"
#include "../paging.hpp"
#include "../msr.hpp"
#include "../memory_map.hpp"
#include "msr_index.h"
#include "vmexit.h"
// #include "file.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

__attribute__((naked)) void GuestEntryPoint() {
    __asm__ __volatile__(
        "hlt\n\t"
        "jmp .\n\t"
    );
}

alignas(16) uint8_t *vm_enter_stack;
alignas(16) uint8_t *vm_exit_stack;

extern "C" void vmexit_handler_c(VmExitContext*context) {
    SetLogLevel(kInfo);
    VmreadResult rr = vmread(VM_EXIT_REASON);
    if (rr.cf || rr.zf) {
        Log(kError, "VMREAD(VM_EXIT_REASON) failed cf=%u zf=%u\n", rr.cf, rr.zf);
        for (;;) asm volatile("hlt");
    }
    const uint32_t reason = static_cast<uint32_t>(rr.value) & 0xffff;
    const uint32_t inst_len = static_cast<uint32_t>(vmread(VM_EXIT_INSTRUCTION_LEN).value);
    const uint64_t guest_rip = vmread(GUEST_RIP).value;

    Log(kInfo, "VM-exit: reason=%u guest_rip=0x%llx inst_len=%u\n",
        reason, guest_rip, inst_len);

    switch (reason) {    
        case EXIT_REASON_HLT:
        { 
            Log(kInfo, "Guest executed HLT instruction\n");
            break;
        }
        case EXIT_REASON_WRMSR:
        {
            if (!vmexit_handler_msr(context)) {
                Log(kError, "vmexit_handler_msr failed\n");
                for (;;) asm volatile("hlt");
            }
            break;
        }
        default:
        {
            Log(kInfo, "Unhandled VM-exit reason=%u\n", reason);
            for (;;) asm volatile("hlt");
            break;
        }
    }
    SetLogLevel(kWarn);

    vmwrite_checked(GUEST_RIP, guest_rip + inst_len);
    return;
}

uint32_t AdjustVmxControls(uint32_t requested, uint32_t msr_index) {
    uint64_t msr = rdmsr(msr_index);
    uint32_t allowed0 = static_cast<uint32_t>(msr);
    uint32_t allowed1 = static_cast<uint32_t>(msr >> 32);

    // Bit must be 1 if allowed0 says so, and may be 1 only if allowed1 says so.
    return (requested | allowed0) & allowed1;
}

uint32_t AdjustVmxControlsTrue(uint32_t requested,
                               uint32_t true_msr,
                               uint32_t old_msr,
                               uint64_t vmx_basic) {
    // If IA32_VMX_BASIC[55] = 1, TRUE_*_CTLS MSRs are supported and should be used.
    if (vmx_basic & (1ULL << 55)) {
        return AdjustVmxControls(requested, true_msr);
    }
    return AdjustVmxControls(requested, old_msr);
}

bool IsIntelCpu(){
    uint32_t eax,ebx,ecx,edx;

    // https://www.intel.co.jp/content/dam/www/public/ijkk/jp/ja/documents/developer/Processor_Identification_071405_i.pdf
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    if (ebx != 0x756e6547) {    // Genu
        return false;
    }
    if (edx != 0x49656e69) {    // ineI
        return false;
    }
    if (ecx != 0x6c65746e) {    // ntel
        return false;
    }
    return true;
}

bool IsVmxSupported() {
    uint32_t eax,ebx,ecx,edx;

    cpuid(1, 0, &eax, &ebx, &ecx, &edx);

    return (ecx & VMX_BIT) == VMX_BIT;
}



bool IsVmxEnabled() {
    uint64_t msr = rdmsr(MSR_IA32_FEAT_CTL);
    Log(kInfo, "MSR_IA32_FEAT_CTL %08X%08X\n", msr>>32, msr);

    if ((msr & FEAT_CTL_LOCKED) != FEAT_CTL_LOCKED) {
        return false;
    }
    if ((msr & FEAT_CTL_VMX_ENABLED_OUTSIDE_SMX) != FEAT_CTL_VMX_ENABLED_OUTSIDE_SMX) {
        return false;
    }

    return true;
}

void ConfigureCr0() {
    uint64_t cr0 = read_cr0();
    uint64_t MsrCr0Fixed0 = rdmsr(MSR_IA32_VMX_CR0_FIXED0);
    uint64_t MsrCr0Fixed1 = rdmsr(MSR_IA32_VMX_CR0_FIXED1);

    cr0 = (cr0 | MsrCr0Fixed0) & MsrCr0Fixed1;
    Log(kInfo, "cr0 %08X%08X\n", cr0>>32, cr0);
    write_cr0(cr0);
}

void ConfigureCr4() {
    uint64_t cr4 = read_cr4();
    uint64_t MsrCr4Fixed0 = rdmsr(MSR_IA32_VMX_CR4_FIXED0);
    uint64_t MsrCr4Fixed1 = rdmsr(MSR_IA32_VMX_CR4_FIXED1);

    cr4 = (cr4 | MsrCr4Fixed0) & MsrCr4Fixed1;
    cr4 |= CR4_VMXE;
    Log(kInfo, "cr4 %08X%08X\n", cr4>>32, cr4);
    write_cr4(cr4);
}



static bool WriteSegmentFields(uint64_t selector_field,
                               uint64_t base_field,
                               uint64_t limit_field,
                               uint64_t access_field,
                               uint16_t selector,
                               uint64_t base,
                               uint32_t limit,
                               uint32_t access_rights) {
    bool success = true;

    success &= vmwrite_checked(selector_field, selector);
    success &= vmwrite_checked(base_field, base);
    success &= vmwrite_checked(limit_field, limit);
    success &= vmwrite_checked(access_field, access_rights);

    return success;
}

VMX_REGIONS* InitializeVmxRegion() {
    uint64_t MsrVmxBasic    = rdmsr(MSR_IA32_VMX_BASIC);
    Log(kInfo, "MsrVmxBasic %08X%08X\n", MsrVmxBasic>>32, MsrVmxBasic);
    uint32_t revisions      = MsrVmxBasic & 0xFFFFFFFF;
    Log(kInfo, "revisions   %08X\n", revisions);
    uint32_t VmcsSize       = (MsrVmxBasic & BITS(44, 32)) >> 32;
    Log(kInfo, "VmcsSize    %08X\n", VmcsSize);
    size_t   num_frames     = (VmcsSize+0xfff)/0x1000;
    VMX_REGIONS* VmcsRegion;

    // num_frames = total size / page size
    auto frame = memory_manager->Allocate(num_frames);
    if (frame.error) {
        Log(kError, "malloc error\n");
        return nullptr;
    }

    VmcsRegion = reinterpret_cast<VMX_REGIONS*>(frame.value.Frame());
    memset(VmcsRegion, 0, num_frames * 0x1000);
    VmcsRegion->revisions = revisions;
    return VmcsRegion;
}

bool vmwrite_checked(uint64_t field, uint64_t value) {
    VmxResult r = vmwrite(field, value);
    if (r.cf || r.zf) {
        Log(kError, "VMWRITE failed field=%llx value=%llx cf=%u zf=%u\n",
            field, value, r.cf, r.zf);
        return false;
    }
    return true;
}


#define TEMP_READ BIT(0)
#define TEMP_WRITE BIT(1)
#define TEMP_EXECUTE BIT(2)
#define TEMP_ALL (TEMP_READ | TEMP_WRITE | TEMP_EXECUTE)
#define TEMP_WB 6 << 3
#define TEMP_LARGE_PAGE BIT(7)


bool vmwrite_EPT() {
    auto ept_pml4_frame = memory_manager->Allocate(1);
    if (ept_pml4_frame.error) {
        Log(kError, "malloc error\n");
        return false;
    }
    auto ept_pml4 = reinterpret_cast<uint64_t*>(ept_pml4_frame.value.Frame());

    auto ept_pdpt_frame = memory_manager->Allocate(1);
    if (ept_pdpt_frame.error) {
        Log(kError, "malloc error\n");
        return false;
    }
    auto ept_pdpt = reinterpret_cast<uint64_t*>(ept_pdpt_frame.value.Frame());

    auto ept_pd_frame = memory_manager->Allocate(1 * 512);
    if (ept_pd_frame.error) {
        Log(kError, "malloc error\n");
        return false;
    }
    auto ept_pd = reinterpret_cast<uint64_t*>(ept_pd_frame.value.Frame());

    ept_pml4[0] = (reinterpret_cast<uint64_t>(ept_pdpt) & 0x000ffffffffff000) | TEMP_ALL;
    for (auto i_ept_pdpt = 0; i_ept_pdpt < 512; ++i_ept_pdpt) {
        ept_pdpt[i_ept_pdpt] = (reinterpret_cast<uint64_t>(&ept_pd[i_ept_pdpt * 512]) & 0x000ffffffffff000) | TEMP_ALL;
        for (auto i_ept_pd = 0; i_ept_pd < 512; ++i_ept_pd) {
            ept_pd[i_ept_pdpt * 512 + i_ept_pd] = (i_ept_pdpt * kPageSize1G) + (i_ept_pd * kPageSize2M) | TEMP_ALL | TEMP_LARGE_PAGE | TEMP_WB;
        }
    }

    const uint64_t eptp =
    (reinterpret_cast<uint64_t>(ept_pml4) & 0x000ffffffffff000ULL)| 0x1e;
    return vmwrite_checked(EPT_POINTER, eptp);
}

bool VmcsConfiguration(uint64_t guest_rip, VM_ENTER_CONTEXT context) {
    bool success = true;

    VmEnterMsrLoadArea = reinterpret_cast<VmxMsrEntry*>(memory_manager->Allocate(1).value.Frame());
    VmExitMsrStoreArea = reinterpret_cast<VmxMsrEntry*>(memory_manager->Allocate(1).value.Frame());
    VmExitMsrLoadArea = reinterpret_cast<VmxMsrEntry*>(memory_manager->Allocate(1).value.Frame());


    const uint64_t vmx_basic = rdmsr(MSR_IA32_VMX_BASIC);

    //
    // Read current machine state
    //
    const uint64_t cr0 = read_cr0();
    const uint64_t cr3 = read_cr3();
    const uint64_t cr4 = read_cr4();
    const uint64_t rsp = read_rsp();
    const uint64_t rflags = read_rflags();
    const uint64_t efer = rdmsr(kIA32_EFER);
    
    const uint16_t cs = read_cs();
    const uint16_t ss = read_ss();
    const uint16_t ds = read_ds();
    const uint16_t es = read_es();
    const uint16_t fs = read_fs();
    const uint16_t gs = read_gs();
    const uint16_t tr = read_tr();
    const uint16_t ldtr = read_ldtr();
    Log(kInfo, "Read MSRs\n");
    Log(kInfo, "cr0: %08X%08X\n", cr0 >> 32, cr0);
    Log(kInfo, "cr3: %08X%08X\n", cr3 >> 32, cr3);
    Log(kInfo, "cr4: %08X%08X\n", cr4 >> 32, cr4);

    DescriptorTablePtr gdtr{};
    DescriptorTablePtr idtr{};
    sgdt(&gdtr);
    sidt(&idtr);

    const uint64_t fs_base = rdmsr(MSR_IA32_FS_BASE);
    const uint64_t gs_base = rdmsr(MSR_IA32_GS_BASE);

    const uint64_t cs_base = GetSegmentBaseGdtOnly(gdtr, cs);
    const uint64_t ss_base = GetSegmentBaseGdtOnly(gdtr, ss);
    const uint64_t ds_base = GetSegmentBaseGdtOnly(gdtr, ds);
    const uint64_t es_base = GetSegmentBaseGdtOnly(gdtr, es);
    const uint64_t tr_base = GetSegmentBaseGdtOnly(gdtr, tr);
    const uint64_t ldtr_base = (ldtr != 0) ? GetSegmentBaseGdtOnly(gdtr, ldtr) : 0;

    const uint32_t cs_limit = GetSegmentLimitGdtOnly(gdtr, cs);
    const uint32_t ss_limit = GetSegmentLimitGdtOnly(gdtr, ss);
    const uint32_t ds_limit = GetSegmentLimitGdtOnly(gdtr, ds);
    const uint32_t es_limit = GetSegmentLimitGdtOnly(gdtr, es);
    const uint32_t fs_limit = GetSegmentLimitGdtOnly(gdtr, fs);
    const uint32_t gs_limit = GetSegmentLimitGdtOnly(gdtr, gs);
    const uint32_t tr_limit = GetSegmentLimitGdtOnly(gdtr, tr);
    const uint32_t ldtr_limit = (ldtr != 0) ? GetSegmentLimitGdtOnly(gdtr, ldtr) : 0;

    const uint32_t cs_ar = GetSegmentAccessRightsGdtOnly(gdtr, cs);
    const uint32_t ss_ar = GetSegmentAccessRightsGdtOnly(gdtr, ss);
    const uint32_t ds_ar = GetSegmentAccessRightsGdtOnly(gdtr, ds);
    const uint32_t es_ar = GetSegmentAccessRightsGdtOnly(gdtr, es);
    const uint32_t fs_ar = GetSegmentAccessRightsGdtOnly(gdtr, fs);
    const uint32_t gs_ar = GetSegmentAccessRightsGdtOnly(gdtr, gs);
    const uint32_t tr_ar = GetSegmentAccessRightsGdtOnly(gdtr, tr);
    const uint32_t ldtr_ar = (ldtr != 0)
        ? GetSegmentAccessRightsGdtOnly(gdtr, ldtr)
        : 0x10000; // unusable

    const bool host_is_ia32e = (efer & (1ULL << 10)) != 0; // EFER.LMA

    //
    // Control fields
    //
    uint32_t pinbased_ctls = 0;
    uint32_t procbased_ctls = 0;
    uint32_t procbased_ctls2 = 0;
    uint32_t exit_ctls = 0;
    uint32_t entry_ctls = 0;

    if (host_is_ia32e) {
        exit_ctls |= VM_EXIT_HOST_ADDR_SPACE_SIZE;
        entry_ctls |= VM_ENTRY_IA32E_MODE;
    }

    pinbased_ctls = AdjustVmxControlsTrue(
        pinbased_ctls,
        MSR_IA32_VMX_TRUE_PINBASED_CTLS,
        MSR_IA32_VMX_PINBASED_CTLS,
        vmx_basic
    );

    // procbased_ctls |= CPU_BASED_HLT_EXITING;
    uint32_t procbased_ctls_request = CPU_BASED_ACTIVATE_SECONDARY_CONTROLS;
    procbased_ctls |= procbased_ctls_request;
    procbased_ctls = AdjustVmxControlsTrue(
        procbased_ctls,
        MSR_IA32_VMX_TRUE_PROCBASED_CTLS,
        MSR_IA32_VMX_PROCBASED_CTLS,
        vmx_basic
    );
    if ((procbased_ctls & procbased_ctls_request) != procbased_ctls_request) {
        Log(kError, "Some of the requested procbased controls are not supported by this processor.\n");
        return false;
    }

    // Secondary controls are active only if primary bit31 is set.
    uint32_t procbased_ctls2_request = SECONDARY_EXEC_ENABLE_EPT;
    if (procbased_ctls & CPU_BASED_ACTIVATE_SECONDARY_CONTROLS) {
        procbased_ctls2 |= procbased_ctls2_request;
        procbased_ctls2 = AdjustVmxControls(
            procbased_ctls2,
            MSR_IA32_VMX_PROCBASED_CTLS2
        );
        if ((procbased_ctls2 & procbased_ctls2_request) != procbased_ctls2_request) {
            Log(kError, "Some of the requested secondary procbased controls are not supported by this processor.\n");
            return false;
        }
    } else {
        procbased_ctls2 = 0;
    }

    exit_ctls = AdjustVmxControlsTrue(
        exit_ctls,
        MSR_IA32_VMX_TRUE_EXIT_CTLS,
        MSR_IA32_VMX_EXIT_CTLS,
        vmx_basic
    );

    entry_ctls = AdjustVmxControlsTrue(
        entry_ctls,
        MSR_IA32_VMX_TRUE_ENTRY_CTLS,
        MSR_IA32_VMX_ENTRY_CTLS,
        vmx_basic
    );

    //
    // Host-state area
    //
    success &= vmwrite_checked(HOST_CR0, cr0);
    success &= vmwrite_checked(HOST_CR3, cr3);
    success &= vmwrite_checked(HOST_CR4, cr4);

    success &= vmwrite_checked(HOST_FS_BASE, fs_base);
    success &= vmwrite_checked(HOST_GS_BASE, gs_base);
    success &= vmwrite_checked(HOST_TR_BASE, tr_base);
    success &= vmwrite_checked(HOST_GDTR_BASE, gdtr.base);
    success &= vmwrite_checked(HOST_IDTR_BASE, idtr.base);

    success &= vmwrite_checked(HOST_CS_SELECTOR, cs & ~0x7);
    success &= vmwrite_checked(HOST_SS_SELECTOR, ss & ~0x7);
    success &= vmwrite_checked(HOST_DS_SELECTOR, ds & ~0x7);
    success &= vmwrite_checked(HOST_ES_SELECTOR, es & ~0x7);
    success &= vmwrite_checked(HOST_FS_SELECTOR, fs & ~0x7);
    success &= vmwrite_checked(HOST_GS_SELECTOR, gs & ~0x7);
    success &= vmwrite_checked(HOST_TR_SELECTOR, tr & ~0x7);


    // num_frames = total size / page size
    WithError<FrameID> frame = memory_manager->Allocate(0x100);
    if (frame.error) {
        Log(kError, "malloc error\n");
        return false;
    }

    vm_exit_stack = reinterpret_cast<uint8_t*>(frame.value.Frame());
    memset(vm_exit_stack, 0, 0x100 * 0x1000);
    success &= vmwrite_checked(HOST_RSP, reinterpret_cast<uint64_t>(vm_exit_stack + 0x100 * 0x1000));
    success &= vmwrite_checked(HOST_RIP, reinterpret_cast<uint64_t>(vmexit_entry));

    success &= vmwrite_checked(HOST_IA32_EFER, efer);
    success &= vmwrite_checked(HOST_IA32_SYSENTER_CS,  rdmsr(MSR_IA32_SYSENTER_CS));
    success &= vmwrite_checked(HOST_IA32_SYSENTER_ESP, rdmsr(MSR_IA32_SYSENTER_ESP));
    success &= vmwrite_checked(HOST_IA32_SYSENTER_EIP, rdmsr(MSR_IA32_SYSENTER_EIP));

    //
    // VM-execution control fields
    //

    success &= vmwrite_checked(VM_ENTRY_MSR_LOAD_ADDR, reinterpret_cast<uint64_t>(VmEnterMsrLoadArea));
    success &= vmwrite_checked(VM_ENTRY_MSR_LOAD_COUNT, VmEnterMsrLoadCount);
    success &= vmwrite_checked(VM_EXIT_MSR_STORE_ADDR, reinterpret_cast<uint64_t>(VmExitMsrStoreArea));
    success &= vmwrite_checked(VM_EXIT_MSR_STORE_COUNT, VmExitMsrStoreCount);
    success &= vmwrite_checked(VM_EXIT_MSR_LOAD_ADDR, reinterpret_cast<uint64_t>(VmExitMsrLoadArea));
    success &= vmwrite_checked(VM_EXIT_MSR_LOAD_COUNT, VmExitMsrLoadCount);


    success &= vmwrite_checked(EXCEPTION_BITMAP, 0);
    success &= vmwrite_checked(PAGE_FAULT_ERROR_CODE_MASK, 0);
    success &= vmwrite_checked(PAGE_FAULT_ERROR_CODE_MATCH, 0);
    success &= vmwrite_checked(CR3_TARGET_COUNT, 0);
    success &= vmwrite_checked(VM_EXIT_CONTROLS, exit_ctls);
    success &= vmwrite_checked(VM_ENTRY_CONTROLS, entry_ctls);

    //
    // Minimal MSR/entry event fields
    //
    success &= vmwrite_checked(VM_EXIT_MSR_STORE_COUNT, 0);
    success &= vmwrite_checked(VM_EXIT_MSR_LOAD_COUNT, 0);
    success &= vmwrite_checked(VM_ENTRY_MSR_LOAD_COUNT, 0);

    success &= vmwrite_checked(VM_ENTRY_INTR_INFO_FIELD, 0);
    success &= vmwrite_checked(VM_ENTRY_EXCEPTION_ERROR_CODE, 0);
    success &= vmwrite_checked(VM_ENTRY_INSTRUCTION_LEN, 0);

    //
    // CR masks/shadows: no interception
    //
    success &= vmwrite_checked(CR0_GUEST_HOST_MASK, 0);
    success &= vmwrite_checked(CR4_GUEST_HOST_MASK, 0);
    success &= vmwrite_checked(CR0_READ_SHADOW, cr0);
    success &= vmwrite_checked(CR4_READ_SHADOW, cr4);

    // 今後、ここの前でEPT等の対応をする
    success &= vmwrite_checked(PIN_BASED_VM_EXEC_CONTROL, pinbased_ctls);
    success &= vmwrite_checked(CPU_BASED_VM_EXEC_CONTROL, procbased_ctls);
    if (procbased_ctls & CPU_BASED_ACTIVATE_SECONDARY_CONTROLS) {
        success &= vmwrite_checked(SECONDARY_VM_EXEC_CONTROL, procbased_ctls2);
        if (procbased_ctls2 & SECONDARY_EXEC_ENABLE_EPT) {
            success &= vmwrite_EPT();
        }
    }


    //
    // Guest-state area
    // Guest = current state
    //

    success &= vmwrite_checked(GUEST_CR0, cr0);
    success &= vmwrite_checked(GUEST_CR3, cr3);
    success &= vmwrite_checked(GUEST_CR4, cr4);

    success &= vmwrite_checked(GUEST_DR7, read_dr7());

    // num_frames = total size / page size
    // 400 kB
    frame = memory_manager->Allocate(0x100);
    if (frame.error) {
        Log(kError, "malloc error\n");
        return false;
    }

    vm_enter_stack = reinterpret_cast<uint8_t*>(frame.value.Frame());
    memset(vm_enter_stack, 0, 0x100 * 0x1000);
    // GUEST_RSP is set to the top of the stack, and the first push in GuestEntryPoint will write below it.
    // 
    // Guest Stack COnfiguration:
    //
    // +---------------------------------------+ <- GUEST_RSP (vm_enter_stack + 0x100 * 0x1000 - 8)
    // | pointer to VM_ENTER_CONTEXT (context) |    This is for the guest to know where the VM_ENTER_CONTEXT is
    // +---------------------------------------+
    // |(unused)                               |
    // +---------------------------------------+
    // | VM_ENTER_CONTEXT (context)            |    This is for the guest to know the initial state of the guest, and other information.
    // +---------------------------------------+ <- vm_enter_stack
    //
    auto allocate_map = memory_manager->Allocate( (sizeof(BitmapMemoryManager::MapTableArrayType) + 0xFFF)/ 0x1000);
    if (allocate_map.error) {
        Log(kError, "malloc error\n");
        return false;
    }

    memory_manager->ExportAllocateMap(reinterpret_cast<BitmapMemoryManager::MapTableArrayType*>(allocate_map.value.Frame()));
    context.Arg6 = reinterpret_cast<uint64_t>(allocate_map.value.Frame());
    *reinterpret_cast<VM_ENTER_CONTEXT*>(vm_enter_stack) = context;
    *reinterpret_cast<uint64_t*>(vm_enter_stack + 0x100 * 0x1000 - 8) = reinterpret_cast<uint64_t>(vm_enter_stack);
    // ここで、メモリマップを渡す必要があるので、ここまでに、メモリマップを作成しておく必要がある。
    success &= vmwrite_checked(GUEST_RSP, reinterpret_cast<uint64_t>(vm_enter_stack + 0x100 * 0x1000 - 8));
    success &= vmwrite_checked(GUEST_RIP, reinterpret_cast<uint64_t>(guest_rip));
    success &= vmwrite_checked(GUEST_RFLAGS, 0x2);

    success &= WriteSegmentFields(GUEST_CS_SELECTOR, GUEST_CS_BASE, GUEST_CS_LIMIT, GUEST_CS_ACCESS_RIGHTS,
                       cs, cs_base, cs_limit, cs_ar);
    success &= WriteSegmentFields(GUEST_SS_SELECTOR, GUEST_SS_BASE, GUEST_SS_LIMIT, GUEST_SS_ACCESS_RIGHTS,
                       ss, ss_base, ss_limit, ss_ar);
    success &= WriteSegmentFields(GUEST_DS_SELECTOR, GUEST_DS_BASE, GUEST_DS_LIMIT, GUEST_DS_ACCESS_RIGHTS,
                       ds, ds_base, ds_limit, ds_ar);
    success &= WriteSegmentFields(GUEST_ES_SELECTOR, GUEST_ES_BASE, GUEST_ES_LIMIT, GUEST_ES_ACCESS_RIGHTS,
                       es, es_base, es_limit, es_ar);
    success &= WriteSegmentFields(GUEST_FS_SELECTOR, GUEST_FS_BASE, GUEST_FS_LIMIT, GUEST_FS_ACCESS_RIGHTS,
                       fs, fs_base, fs_limit, fs_ar);
    success &= WriteSegmentFields(GUEST_GS_SELECTOR, GUEST_GS_BASE, GUEST_GS_LIMIT, GUEST_GS_ACCESS_RIGHTS,
                       gs, gs_base, gs_limit, gs_ar);
    success &= WriteSegmentFields(GUEST_TR_SELECTOR, GUEST_TR_BASE, GUEST_TR_LIMIT, GUEST_TR_ACCESS_RIGHTS,
                       tr, tr_base, tr_limit, tr_ar);
    success &= WriteSegmentFields(GUEST_LDTR_SELECTOR, GUEST_LDTR_BASE, GUEST_LDTR_LIMIT, GUEST_LDTR_ACCESS_RIGHTS,
                       ldtr, ldtr_base, ldtr_limit, ldtr_ar);

    success &= vmwrite_checked(GUEST_GDTR_BASE, gdtr.base);
    success &= vmwrite_checked(GUEST_GDTR_LIMIT, gdtr.limit);
    success &= vmwrite_checked(GUEST_IDTR_BASE, idtr.base);
    success &= vmwrite_checked(GUEST_IDTR_LIMIT, idtr.limit);

    success &= vmwrite_checked(GUEST_SYSENTER_CS, rdmsr(MSR_IA32_SYSENTER_CS));
    success &= vmwrite_checked(GUEST_SYSENTER_ESP, rdmsr(MSR_IA32_SYSENTER_ESP));
    success &= vmwrite_checked(GUEST_SYSENTER_EIP, rdmsr(MSR_IA32_SYSENTER_EIP));

    success &= vmwrite_checked(GUEST_IA32_EFER, efer);

    success &= vmwrite_checked(VMCS_LINK_POINTER, ~0ULL);

    success &= vmwrite_checked(GUEST_ACTIVITY_STATE, 0);
    success &= vmwrite_checked(GUEST_INTERRUPTIBILITY_STATE, 0);
    success &= vmwrite_checked(GUEST_PENDING_DBG_EXCEPTIONS, 0);
    success &= vmwrite_checked(GUEST_VMCS_PREEMPTION_TIMER_VALUE, 0);




    return success;
}

void HypervisorMain(uint64_t guest_rip, VM_ENTER_CONTEXT context) {
    VMX_REGIONS* VmxonRegion = nullptr;
    VMX_REGIONS* VmcsRegion = nullptr;

    //
    // Check VMX support and enable VMX operation
    //
    if (!IsIntelCpu()) {
        Log(kError, "Not Intel CPU\n");
        return;
    }

    if (!IsVmxSupported()) {
        Log(kError, "Not VMX Supported\n");
        return;
    }

    if (!IsVmxEnabled()) {
        Log(kError, "Not VMX Enabled\n");
        return;
    }

    //
    // Configure control registers and VMXON region
    //
    ConfigureCr0();
    ConfigureCr4();

    VmxonRegion = InitializeVmxRegion();
    if (VmxonRegion == nullptr) {
        Log(kError, "Not Vmxon Region\n");
        return;
    }

    VmxResult result = vmxon((uint64_t)VmxonRegion);
    if ((result.zf != 0) || (result.cf != 0)) {
        Log(kError, "VMXON Fail zf = %02X, cf = %02X\n", result.zf, result.cf);
        return;
    }

    VmcsRegion = InitializeVmxRegion();
    if (VmcsRegion == nullptr) {
        Log(kError, "Not Vmcs Region\n");
        return;
    }

    if (!vmclear((uint64_t)VmcsRegion)) {
        Log(kError, "VMCLEAR Failed\n");
        return;
    }

    if (!vmptrld((uint64_t)VmcsRegion)) {
        Log(kError, "VMPTRLD Failed\n");
        return;
    }

    if (!VmcsConfiguration(guest_rip, context)) {
        Log(kError, "VmcsConfiguration Fail\n");
        return;
    }

    VmxResult launch = vmlaunch();
    if (launch.cf || launch.zf) {
        Log(kError, "VMLAUNCH failed cf=%u zf=%u\n", launch.cf, launch.zf);

        if (launch.zf) {
            VmreadResult err = vmread(VM_INSTRUCTION_ERROR);
            if (!err.cf && !err.zf) {
                Log(kError, "VM instruction error = %llu\n", err.value);
            } else {
                Log(kError, "VMREAD(VM_INSTRUCTION_ERROR) failed cf=%u zf=%u\n",
                    err.cf, err.zf);
            }
        }
        return;
    }

    Log(kError, "Unexpected fallthrough after VMLAUNCH\n");

    // memory_manager->Free()
    return;
}
