#include <stdint.h>
#include "hypervisor.hpp"
#include "../segment.hpp"
#include "../logger.hpp"

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

uint64_t read_cr3(void)
{
    uint64_t v;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(v));
    return v;
}

void write_cr3(uint64_t v)
{
    __asm__ volatile ("mov %0, %%cr3" :: "r"(v) : "memory");
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

uint64_t read_rsp(void)
{
    uint64_t v;
    __asm__ volatile ("mov %%rsp, %0" : "=r"(v));
    return v;
}

void write_rsp(uint64_t v)
{
    __asm__ volatile ("mov %0, %%rsp" :: "r"(v) : "memory");
}

uint64_t read_rflags(void)
{
    uint64_t v;
    __asm__ volatile ("pushfq; pop %0" : "=r"(v));
    return v;
}

uint64_t read_cs(void)
{
    uint16_t v;
    __asm__ volatile ("mov %%cs, %0" : "=r"(v));
    return v;
}

uint64_t read_ss(void)
{
    uint16_t v;
    __asm__ volatile ("mov %%ss, %0" : "=r"(v));
    return v;
}

uint64_t read_ds(void)
{
    uint16_t v;
    __asm__ volatile ("mov %%ds, %0" : "=r"(v));
    return v;
}

uint64_t read_es(void)
{
    uint16_t v;
    __asm__ volatile ("mov %%es, %0" : "=r"(v));
    return v;
}

uint64_t read_fs(void)
{
    uint16_t v;
    __asm__ volatile ("mov %%fs, %0" : "=r"(v));
    return v;
}

uint64_t read_gs(void)
{
    uint16_t v;
    __asm__ volatile ("mov %%gs, %0" : "=r"(v));
    return v;
}

uint64_t read_tr(void)
{
    uint16_t v;
    __asm__ volatile ("str %0" : "=r"(v));
    return v;
}

uint64_t read_ldtr(void)
{
    uint16_t v;
    __asm__ volatile ("sldt %0" : "=r"(v));
    return v;
}

uint64_t read_dr7() {
    uint64_t v;
    __asm__ volatile ("mov %%dr7, %0" : "=r"(v));
    return v;
}

void sgdt(DescriptorTablePtr* dt)
{
    __asm__ volatile ("sgdt %0" : "=m"(*dt));
}

void sidt(DescriptorTablePtr* dt)
{
    __asm__ volatile ("sidt %0" : "=m"(*dt));
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

VmxResult vmxon(uint64_t vmxon_pa)
{
    VmxResult r{};
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

extern "C" bool vmclear(uint64_t vmcs_pa) {
    unsigned char failed;

    asm volatile(
        "vmclear %[pa]\n\t"
        "setna %[failed]"
        : [failed] "=qm"(failed)
        : [pa] "m"(vmcs_pa)
        : "cc", "memory");

    return failed == 0;
}

extern "C" bool vmptrld(uint64_t vmcs_pa) {
    unsigned char failed;

    asm volatile(
        "vmptrld %[pa]\n\t"
        "setna %[failed]"
        : [failed] "=qm"(failed)
        : [pa] "m"(vmcs_pa)
        : "cc", "memory");

    return failed == 0;
}

extern "C" VmxResult vmwrite(uint64_t field, uint64_t value) {
    VmxResult r{};
    uint8_t cf = 0;
    uint8_t zf = 0;

    __asm__ __volatile__(
        "vmwrite %[value], %[field]\n\t"
        "setc %[cf]\n\t"
        "setz %[zf]\n\t"
        : [cf] "=r"(cf), [zf] "=r"(zf)
        : [field] "r"(field), [value] "r"(value)
        : "cc"
    );

    r.cf = cf;
    r.zf = zf;
    return r;
}

extern "C" VmreadResult vmread(uint64_t field) {
    VmreadResult r{};
    uint64_t value = 0;
    uint8_t cf = 0;
    uint8_t zf = 0;

    __asm__ __volatile__(
        "vmread %[field], %[value]\n\t"
        "setc %[cf]\n\t"
        "setz %[zf]\n\t"
        : [value] "=r"(value), [cf] "=r"(cf), [zf] "=r"(zf)
        : [field] "r"(field)
        : "cc"
    );

    r.value = value;
    r.cf = cf;
    r.zf = zf;
    return r;
}


extern "C" VmxResult vmlaunch() {
    Log(kInfo, "VMLAUNCH called\n");
    VmxResult r{};
    uint8_t cf = 0;
    uint8_t zf = 0;
    Log(kInfo, "Executing VMLAUNCH\n");

    __asm__ __volatile__(
        "vmlaunch\n\t"
        "setc %[cf]\n\t"
        "setz %[zf]\n\t"
        : [cf] "=r"(cf), [zf] "=r"(zf)
        :
        : "cc", "memory"
    );

    r.cf = cf;
    r.zf = zf;
    return r;
}




