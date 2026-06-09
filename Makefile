CC = gcc
CXX = g++
AS = gcc
LD = ld

CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-pic -fno-stack-protector -Ikernel/include -nostdlib
CXXFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fno-pie -fno-pic -fno-stack-protector -Ikernel/include -Isdk/framework/include -Isdk/api/include -Isdk/gui/include -nostdlib
ASFLAGS = -m32 -c
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib -no-pie

# Source files
C_SOURCES = $(wildcard kernel/src/*.c)
CPP_SOURCES = $(wildcard sdk/framework/src/*.cpp) $(wildcard sdk/api/src/*.cpp) $(wildcard sdk/gui/src/*.cpp)
AS_SOURCES = boot/boot.s

# Object files
OBJ = boot/boot.o \
      $(C_SOURCES:.c=.o) \
      $(CPP_SOURCES:.cpp=.o)

# Target files
BIN = ritos.bin
ISO = ritos.iso

all: $(BIN) $(ISO)

$(BIN): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

boot/boot.o: boot/boot.s
	$(AS) $(ASFLAGS) $< -o $@

$(ISO): $(BIN)
	mkdir -p iso_root/boot/grub
	cp $(BIN) iso_root/boot/
	cp boot/grub.cfg iso_root/boot/grub/
	grub-mkrescue -o $(ISO) iso_root
	rm -rf iso_root

clean:
	rm -f $(OBJ) $(BIN) $(ISO)

.PHONY: all clean
