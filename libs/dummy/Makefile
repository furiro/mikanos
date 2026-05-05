TARGET = dummy.so
OBJS = dummy.o


.PHONY: all dummy
all: $(TARGET)
dummy: $(TARGET)


$(TARGET): $(OBJS) Makefile
	clang++ --target=x86_64-elf -nostdlib -nostdlib++ -nodefaultlibs -shared $< -Wl,--hash-style=sysv -o $@
	python3 ../../kernel/crypto/sign.py $@ -k ../../kernel/crypto/private_key.bin

%.o: %.cpp Makefile
	clang++ --target=x86_64-elf -ffreestanding -fPIC -fno-exceptions -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables -c $< -o $@


