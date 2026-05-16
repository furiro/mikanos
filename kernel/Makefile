TARGET = kernel.elf
OBJS_KERNEL = main.o graphics.o mouse.o font.o hankaku.o newlib_support.o console.o \
       pci.o asmfunc.o libcxx_support.o logger.o interrupt.o segment.o paging.o memory_manager.o \
       window.o layer.o timer.o frame_buffer.o acpi.o keyboard.o task.o terminal.o \
       fat.o syscall.o file.o \
       usb/memory.o usb/device.o usb/xhci/ring.o usb/xhci/trb.o usb/xhci/xhci.o \
       usb/xhci/port.o usb/xhci/device.o usb/xhci/devmgr.o usb/xhci/registers.o \
       usb/classdriver/base.o usb/classdriver/hid.o usb/classdriver/keyboard.o \
       usb/classdriver/mouse.o usb/classdriver/cdc.o \
	   hypervisor/hypervisor.o hypervisor/asms.o hypervisor/vmexit.o hypervisor/vmexit_msr.o\
	   crypto/verify.o crypto/monocypher-4.0.2/src/monocypher.o crypto/monocypher-4.0.2/src/monocypher-ed25519.o

OBJS_HYPERVISOR = hypervisor/hypervisor.o hypervisor/asms.o hypervisor/vmexit.o hypervisor/vmexit_msr.o \
		main.o graphics.o mouse.o font.o hankaku.o newlib_support.o console.o \
       pci.o asmfunc.o libcxx_support.o logger.o interrupt.o segment.o paging.o memory_manager.o \
       window.o layer.o timer.o frame_buffer.o acpi.o keyboard.o task.o terminal.o \
       fat.o syscall.o file.o \
       usb/memory.o usb/device.o usb/xhci/ring.o usb/xhci/trb.o usb/xhci/xhci.o \
       usb/xhci/port.o usb/xhci/device.o usb/xhci/devmgr.o usb/xhci/registers.o \
       usb/classdriver/base.o usb/classdriver/hid.o usb/classdriver/keyboard.o \
       usb/classdriver/mouse.o usb/classdriver/cdc.o \
	   crypto/verify.o crypto/monocypher-4.0.2/src/monocypher.o crypto/monocypher-4.0.2/src/monocypher-ed25519.o


DEPENDS = $(join $(dir $(OBJS_KERNEL)),$(addprefix .,$(notdir $(OBJS_KERNEL:.o=.d))))

CPPFLAGS_KERNEL = $(CPPFLAGS) -I.
CFLAGS_KERNEL   = $(CFLAGS) -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone -fshort-wchar
CXXFLAGS_KERNEL = $(CXXFLAGS) -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone -fshort-wchar \
            -fno-exceptions -fno-rtti -std=c++17
LDFLAGS_KERNEL  = $(LDFLAGS) --entry KernelMain -z norelro --image-base 0x100000 --static
LDFLAGS_HYPERVISOR = $(LDFLAGS) --entry HypervisorMain -z norelro --image-base 0xC00000 --static


.PHONY: all
all: $(TARGET) hypervisor.elf

.PHONY: clean
clean:
	rm -rf *.o

hypervisor.elf: $(OBJS_HYPERVISOR) Makefile
	ld.lld $(LDFLAGS_HYPERVISOR) -o hypervisor.elf $(OBJS_HYPERVISOR) -lc -lc++ -lc++abi -lm \
	-lfreetype

kernel.elf: $(OBJS_KERNEL) Makefile
	ld.lld $(LDFLAGS_KERNEL) -o kernel.elf $(OBJS_KERNEL) -lc -lc++ -lc++abi -lm \
	-lfreetype

%.o: %.cpp Makefile
	clang++ $(CPPFLAGS_KERNEL) $(CXXFLAGS_KERNEL) -c $< -o $@

.%.d: %.cpp
	clang++ $(CPPFLAGS_KERNEL) $(CXXFLAGS_KERNEL) -MM $< > $@
	$(eval OBJ = $(<:.cpp=.o))
	sed --in-place 's|$(notdir $(OBJ))|$(OBJ)|' $@

%.o: %.c Makefile
	clang $(CPPFLAGS_KERNEL) $(CFLAGS_KERNEL) -c $< -o $@

.%.d: %.c
	clang $(CPPFLAGS_KERNEL) $(CFLAGS_KERNEL) -MM $< > $@
	$(eval OBJ = $(<:.c=.o))
	sed --in-place 's|$(notdir $(OBJ))|$(OBJ)|' $@

%.o: %.asm Makefile
	nasm -f elf64 -o $@ $<

hankaku.bin: hankaku.txt
	../tools/makefont.py -o $@ $<

hankaku.o: hankaku.bin
	objcopy -I binary -O elf64-x86-64 -B i386:x86-64 $< $@

.%.d: %.bin
	touch $@

.PHONY: depends
depends:
	$(MAKE) $(DEPENDS)

-include $(DEPENDS)
