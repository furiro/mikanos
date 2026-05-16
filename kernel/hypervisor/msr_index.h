#ifndef HYPERVISOR_MSR_INDEX_H
#define HYPERVISOR_MSR_INDEX_H
// Reffered to https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
// 2.5 CONTROL REGISTERS
#define CR0_PG BIT(31)
#define CR0_NE BIT(5)
#define CR0_PE BIT(0)
#define CR4_VMXE BIT(13)



#define MSR_IA32_FS_BASE                  0xC0000100
#define MSR_IA32_GS_BASE                  0xC0000101
#define MSR_IA32_SYSENTER_CS              0x174
#define MSR_IA32_SYSENTER_ESP             0x175
#define MSR_IA32_SYSENTER_EIP             0x176

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


struct VmxMsrEntry {
    uint32_t msr_index;
    uint32_t reserved;
    uint64_t value;
};

#endif