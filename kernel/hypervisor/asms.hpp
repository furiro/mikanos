#include <stdint.h>
#include "hypervisor.hpp"
#include "../segment.hpp"

uint64_t read_cr4(void);
void write_cr4(uint64_t v);
uint64_t read_cr3(void);
void write_cr3(uint64_t v);
uint64_t read_cr0(void);
void write_cr0(uint64_t v);
uint64_t read_rsp(void);
void write_rsp(uint64_t v);
uint64_t read_rflags(void);
uint64_t read_cs(void);
uint64_t read_ss(void);
uint64_t read_ds(void);
uint64_t read_es(void);
uint64_t read_fs(void);
uint64_t read_gs(void);
uint64_t read_tr(void);
uint64_t read_ldtr(void);
uint64_t read_dr7(void);
void sgdt(DescriptorTablePtr* dt);
void sidt(DescriptorTablePtr* dt);

void cpuid(uint32_t leaf, uint32_t subleaf,
           uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx);
uint64_t rdmsr(uint32_t msr);
void wrmsr(uint32_t msr, uint64_t value);
VmxResult vmxon(uint64_t vmxon_pa);
extern "C" bool vmclear(uint64_t vmcs_pa);
extern "C" bool vmptrld(uint64_t vmcs_pa);
extern "C" VmxResult vmwrite(uint64_t field, uint64_t value);
extern "C" VmreadResult vmread(uint64_t field);
extern "C" VmxResult vmlaunch();





