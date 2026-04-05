#ifndef HYPERVISOR_HPP
#define HYPERVISOR_HPP

#include <stdint.h>
void HypervisorMain();


#define BIT(n) (1ULL << (n))
#define BITS(high, low) \
    (((1ULL << ((high) - (low) + 1)) - 1) << (low))
#define VMX_BIT BIT(5)

typedef struct {
    uint32_t revisions;
    uint32_t abortIndicator;
    uint8_t  data;
} VMX_REGIONS;

typedef struct VmxonResult {
    uint8_t cf;
    uint8_t zf;
};

// Reffered to https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
// 2.5 CONTROL REGISTERS
#define CR0_PG BIT(31)
#define CR0_NE BIT(5)
#define CR0_PE BIT(0)
#define CR4_VMXE BIT(13)

// https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/msr-index.h
/* Referred to as IA32_FEATURE_CONTROL in Intel's SDM. */
// https://cdrdv2.intel.com/v1/dl/getContent/671269
#define MSR_IA32_FEAT_CTL		0x0000003a
#define FEAT_CTL_LOCKED				BIT(0)
#define FEAT_CTL_VMX_ENABLED_INSIDE_SMX		BIT(1)
#define FEAT_CTL_VMX_ENABLED_OUTSIDE_SMX	BIT(2)

/* Intel VT MSRs */
#define MSR_IA32_VMX_BASIC              0x00000480
#define MSR_IA32_VMX_PINBASED_CTLS      0x00000481
#define MSR_IA32_VMX_PROCBASED_CTLS     0x00000482
#define MSR_IA32_VMX_EXIT_CTLS          0x00000483
#define MSR_IA32_VMX_ENTRY_CTLS         0x00000484
#define MSR_IA32_VMX_MISC               0x00000485
#define MSR_IA32_VMX_CR0_FIXED0         0x00000486
#define MSR_IA32_VMX_CR0_FIXED1         0x00000487
#define MSR_IA32_VMX_CR4_FIXED0         0x00000488
#define MSR_IA32_VMX_CR4_FIXED1         0x00000489
#define MSR_IA32_VMX_VMCS_ENUM          0x0000048a
#define MSR_IA32_VMX_PROCBASED_CTLS2    0x0000048b
#define MSR_IA32_VMX_EPT_VPID_CAP       0x0000048c
#define MSR_IA32_VMX_TRUE_PINBASED_CTLS  0x0000048d
#define MSR_IA32_VMX_TRUE_PROCBASED_CTLS 0x0000048e
#define MSR_IA32_VMX_TRUE_EXIT_CTLS      0x0000048f
#define MSR_IA32_VMX_TRUE_ENTRY_CTLS     0x00000490
#define MSR_IA32_VMX_VMFUNC             0x00000491
#define MSR_IA32_VMX_PROCBASED_CTLS3	0x00000492

#define MSR_IA32_MCU_STAGING_MBOX_ADDR	0x000007a5


#endif // HYPERVISOR_HPP