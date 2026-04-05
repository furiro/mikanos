#include "hypervisor.hpp"
#include "asms.hpp"
#include "../memory_manager.hpp"
#include "../logger.hpp"
// #include "file.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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

VMX_REGIONS* ConfigureVmxonRegion() {
    uint64_t MsrVmxBasic    = rdmsr(MSR_IA32_VMX_BASIC);
    Log(kInfo, "MsrVmxBasic %08X%08X\n", MsrVmxBasic>>32, MsrVmxBasic);
    uint32_t revisions      = MsrVmxBasic & 0xFFFFFFFF;
    Log(kInfo, "revisions %08X\n", revisions);
    uint32_t VmcsSize       = (MsrVmxBasic & BITS(44, 32)) >> 32;
    Log(kInfo, "VmcsSize %08X\n", VmcsSize);
    size_t   num_frames     = (VmcsSize+0xfff)/0x1000;
    VMX_REGIONS* VmxonRegion;

    // num_frames = total size / page size
    auto frame = memory_manager->Allocate(num_frames);
    if (frame.error) {
        Log(kError, "malloc error\n");
        return nullptr;
    }

    VmxonRegion = reinterpret_cast<VMX_REGIONS*>(frame.value.Frame());
    memset(VmxonRegion, 0, num_frames * 0x1000);
    VmxonRegion->revisions = revisions;
    return VmxonRegion;
}

void HypervisorMain() {
    VMX_REGIONS* VmxonRegion;
    SetLogLevel(kInfo);

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

    ConfigureCr0();
    ConfigureCr4();

    VmxonRegion = ConfigureVmxonRegion();
    if (VmxonRegion == nullptr) {
        Log(kError, "Not Vmxon Region\n");
        return;
    }

    VmxonResult result = vmxon((uint64_t)VmxonRegion);
    if ((result.zf != 0) || (result.cf != 0)) {
        Log(kError, "VMXON Fail zf = %02X, cf = %02X\n", result.zf, result.cf);
    } 

    

    // memory_manager->Free()
    return;
}
