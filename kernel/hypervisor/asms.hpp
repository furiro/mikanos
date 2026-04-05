#include <stdint.h>
#include "hypervisor.hpp"

uint64_t read_cr4(void);
void write_cr4(uint64_t v);
uint64_t read_cr0(void);
void write_cr0(uint64_t v);
void cpuid(uint32_t leaf, uint32_t subleaf,
           uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx);
uint64_t rdmsr(uint32_t msr);
void wrmsr(uint32_t msr, uint64_t value);
VmxonResult vmxon(uint64_t vmxon_pa);





