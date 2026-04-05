#include <stdint.h>
#include "hypervisor.hpp"

uint64_t read_cr4(void)
{
    uint64_t v;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(v));
    return v;
}

void write_cr4(uint64_t v)
{
    __asm__ volatile ("mov %0, %%cr4" :: "r"(v) : "memory");
}

uint64_t read_cr0(void)
{
    uint64_t v;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(v));
    return v;
}

void write_cr0(uint64_t v)
{
    __asm__ volatile ("mov %0, %%cr0" :: "r"(v) : "memory");
}

void cpuid(uint32_t leaf, uint32_t subleaf,
                         uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid"
                     : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(leaf), "c"(subleaf));
    if (eax) *eax = a;
    if (ebx) *ebx = b;
    if (ecx) *ecx = c;
    if (edx) *edx = d;
}

uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile (
        "rdmsr"
        : "=a"(lo), "=d"(hi)
        : "c"(msr)
        : /* no clobbers */
    );
    return ((uint64_t)hi << 32) | lo;
}

void wrmsr(uint32_t msr, uint64_t value)
{
    uint32_t lo = (uint32_t)(value & 0xFFFFFFFFu);
    uint32_t hi = (uint32_t)(value >> 32);

    __asm__ volatile (
        "wrmsr"
        :
        : "c"(msr), "a"(lo), "d"(hi)
        : "memory"
    );
}

VmxonResult vmxon(uint64_t vmxon_pa)
{
    VmxonResult r{};
    asm volatile(
        "vmxon %[pa]\n\t"
        "setc %[cf]\n\t"
        "setz %[zf]\n\t"
        : [cf] "=rm"(r.cf), [zf] "=rm"(r.zf)
        : [pa] "m"(vmxon_pa)
        : "cc", "memory"
    );
    return r;
}


