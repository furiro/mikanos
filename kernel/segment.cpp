#include "segment.hpp"

#include "asmfunc.h"
#include "interrupt.hpp"
#include "logger.hpp"
#include "memory_manager.hpp"
#include "x86_descriptor.hpp"

namespace {
  std::array<SegmentDescriptor, 7> gdt;
  std::array<uint32_t, 26> tss;

  static_assert((kTSS >> 3) + 1 < gdt.size());

  void SetTSS(int index, uint64_t value) {
    tss[index]     = value & 0xffffffff;
    tss[index + 1] = value >> 32;
  }

  uint64_t AllocateStackArea(int num_4kframes) {
    auto [ stk, err ] = memory_manager->Allocate(num_4kframes);
    if (err) {
      Log(kError, "failed to allocate stack area: %s\n", err.Name());
      exit(1);
    }
    return reinterpret_cast<uint64_t>(stk.Frame()) + num_4kframes * 4096;
  }
}

uint64_t GetSegmentBaseGdtOnly(DescriptorTablePtr gdtr, uint16_t selector) {
    Log(kInfo, "GetSegmentBaseGdtOnly: selector=%04x\n", selector);
    if ((selector & ~0x7) == 0) {
        return 0;
    }

    // GDT only
    if (selector & 0x4) {
        return 0;
    }

    const uint16_t index = selector >> 3;
    const uint64_t offset = static_cast<uint64_t>(index) * 8;

    if (offset + sizeof(SegmentDescriptor) - 1 > gdtr.limit) {
        return 0;
    }

    const uint64_t desc_addr = gdtr.base + offset;
    const auto* desc = reinterpret_cast<const SegmentDescriptor*>(desc_addr);
    Log(kInfo, "Segment descriptor: %x\n", *reinterpret_cast<uint64_t*>(desc_addr));

    uint64_t base = 0;
    base |= static_cast<uint64_t>(desc->bits.base_low);
    base |= static_cast<uint64_t>(desc->bits.base_middle) << 16;
    base |= static_cast<uint64_t>(desc->bits.base_high) << 24;
    Log(kInfo, "base_low=%08x base_middle=%02x base_high=%02x\n",
        desc->bits.base_low, desc->bits.base_middle, desc->bits.base_high);

    const bool system_segment = (desc->bits.system_segment) == 0;
    const DescriptorType type = desc->bits.type;

    if (system_segment &&
        (type == DescriptorType::kLDT ||
         type == DescriptorType::kTSSAvailable ||
         type == DescriptorType::kTSSBusy)) {

        if (offset + sizeof(SystemSegmentDescriptor) - 1 > gdtr.limit) {
            return 0;
        }

        const auto* sys_desc =
            reinterpret_cast<const SystemSegmentDescriptor*>(desc_addr);
        base |= static_cast<uint64_t>(sys_desc->base_upper) << 32;
        Log(kInfo, "System segment: base_upper=%08x\n", sys_desc->base_upper);
    }

    return base;
}

uint64_t GetSegmentLimitGdtOnly(DescriptorTablePtr gdtr, uint16_t selector) {
    if ((selector & ~0x7) == 0) {
        return 0;
    }

    // GDT only
    if (selector & 0x4) {
        return 0;
    }

    const uint16_t index = selector >> 3;
    const uint64_t offset = static_cast<uint64_t>(index) * 8;

    if (offset + sizeof(SegmentDescriptor) - 1 > gdtr.limit) {
        return 0;
    }

    const uint64_t desc_addr = gdtr.base + offset;
    const auto* desc = reinterpret_cast<const SegmentDescriptor*>(desc_addr);

    uint32_t limit = 0;
    limit |= static_cast<uint32_t>(desc->bits.limit_low);
    limit |= static_cast<uint32_t>(desc->bits.limit_high) << 16;

    if (desc->bits.granularity) {
        limit = (limit << 12) | 0xFFF;
    }

    return limit;
}

uint32_t GetSegmentAccessRightsGdtOnly(DescriptorTablePtr gdtr, uint16_t selector) {
    // Null selector
    if ((selector & ~0x7) == 0) {
        return 1U << 16;  // unusable
    }

    // GDT only
    if (selector & 0x4) {
        return 1U << 16;  // unusable
    }

    const uint16_t index = selector >> 3;
    const uint64_t offset = static_cast<uint64_t>(index) * 8;

    if (offset + sizeof(SegmentDescriptor) - 1 > gdtr.limit) {
        Log(kError, "GetSegmentAccessRightsGdtOnly: selector=%04x out of limit\n", selector);
        return 1U << 16;  // unusable
    }

    const uint64_t desc_addr = gdtr.base + offset;
    const auto* desc = reinterpret_cast<const SegmentDescriptor*>(desc_addr);

    uint32_t ar = 0;

    // Access byte -> bits 8..15
    ar |= static_cast<uint32_t>(desc->bits.type) & 0xF;                               // bits 8..11 -> AR[0:3]
    Log(kInfo, "GetSegmentAccessRightsGdtOnly: type=%02x AR=%08x\n", desc->bits.type, ar);
    ar |= static_cast<uint32_t>((desc->bits.system_segment) & 1U) << 4;                // bit 12 (S) -> AR[4]
    Log(kInfo, "GetSegmentAccessRightsGdtOnly: system_segment=%u AR=%08x\n", desc->bits.system_segment, ar);
    ar |= static_cast<uint32_t>((desc->bits.descriptor_privilege_level) & 0x03) << 5;    // bits 13..14 -> AR[5:6]
    Log(kInfo, "GetSegmentAccessRightsGdtOnly: DPL=%u AR=%08x\n", desc->bits.descriptor_privilege_level, ar);
    ar |= static_cast<uint32_t>((desc->bits.present) & 1U) << 7;                       // bit 15 (P) -> AR[7]
    Log(kInfo, "GetSegmentAccessRightsGdtOnly: present=%u AR=%08x\n", desc->bits.present, ar);

    // Flags -> bits 20..23
    ar |= static_cast<uint32_t>((desc->bits.available) & 1U) << 12;                // AVL -> AR[12]
    ar |= static_cast<uint32_t>((desc->bits.long_mode) & 1U) << 13;                // L -> AR[13]
    ar |= static_cast<uint32_t>((desc->bits.default_operation_size) & 1U) << 14;   // D/B -> AR[14]
    ar |= static_cast<uint32_t>((desc->bits.granularity) & 1U) << 15;              // G -> AR[15]

    // Present=0 の場合は unusable 扱いにしておくと VMX では扱いやすい
    if (desc->bits.present == 0) {
        Log(kError, "GetSegmentAccessRightsGdtOnly: selector=%04x is not present\n", selector);
        ar |= 1U << 16;
    }

    Log(kInfo, "GetSegmentAccessRightsGdtOnly: selector=%04x AR=%08x\n", selector, ar);
    return ar;
}

void SetCodeSegment(SegmentDescriptor& desc,
                    DescriptorType type,
                    unsigned int descriptor_privilege_level,
                    uint32_t base,
                    uint32_t limit) {
  desc.data = 0;

  desc.bits.base_low = base & 0xffffu;
  desc.bits.base_middle = (base >> 16) & 0xffu;
  desc.bits.base_high = (base >> 24) & 0xffu;

  desc.bits.limit_low = limit & 0xffffu;
  desc.bits.limit_high = (limit >> 16) & 0xfu;

  desc.bits.type = type;
  desc.bits.system_segment = 1; // 1: code & data segment
  desc.bits.descriptor_privilege_level = descriptor_privilege_level;
  desc.bits.present = 1;
  desc.bits.available = 0;
  desc.bits.long_mode = 1;
  desc.bits.default_operation_size = 0; // should be 0 when long_mode == 1
  desc.bits.granularity = 1;
}

void SetDataSegment(SegmentDescriptor& desc,
                    DescriptorType type,
                    unsigned int descriptor_privilege_level,
                    uint32_t base,
                    uint32_t limit) {
  SetCodeSegment(desc, type, descriptor_privilege_level, base, limit);
  desc.bits.long_mode = 0;
  desc.bits.default_operation_size = 1; // 32-bit stack segment
}

void SetSystemSegment(SegmentDescriptor& desc,
                      DescriptorType type,
                      unsigned int descriptor_privilege_level,
                      uint32_t base,
                      uint32_t limit) {
  SetCodeSegment(desc, type, descriptor_privilege_level, base, limit);
  desc.bits.system_segment = 0;
  desc.bits.granularity = 0; // system segment では limit はバイト単位
  desc.bits.long_mode = 0;
}

void SetupSegments() {
  gdt[0].data = 0;
  SetCodeSegment(gdt[1], DescriptorType::kExecuteRead, 0, 0, 0xfffff);
  SetDataSegment(gdt[2], DescriptorType::kReadWrite, 0, 0, 0xfffff);
  SetDataSegment(gdt[3], DescriptorType::kReadWrite, 3, 0, 0xfffff);
  SetCodeSegment(gdt[4], DescriptorType::kExecuteRead, 3, 0, 0xfffff);
  LoadGDT(sizeof(gdt) - 1, reinterpret_cast<uintptr_t>(&gdt[0]));
}

void InitializeSegmentation() {
  SetupSegments();

  SetDSAll(kKernelDS);
  SetCSSS(kKernelCS, kKernelSS);
}

void InitializeTSS() {
  SetTSS(1, AllocateStackArea(8));
  SetTSS(7 + 2 * kISTForTimer, AllocateStackArea(8));

  uint64_t tss_addr = reinterpret_cast<uint64_t>(&tss[0]);
  SetSystemSegment(gdt[kTSS >> 3], DescriptorType::kTSSAvailable, 0,
                   tss_addr & 0xffffffff, sizeof(tss)-1);
  gdt[(kTSS >> 3) + 1].data = tss_addr >> 32;

  LoadTR(kTSS);
}
