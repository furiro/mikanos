TARGET = libver.so
OBJS = libver.o


.PHONY: all libver
all: $(TARGET)
libver: $(TARGET)


$(TARGET): $(OBJS) Makefile
	clang++ --target=x86_64-elf -nostdlib -nostdlib++ -nodefaultlibs -shared $< -Wl,--hash-style=sysv -o $@

%.o: %.cpp Makefile
	clang++ --target=x86_64-elf -ffreestanding -fPIC -fno-exceptions -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables -c $< -o $@


