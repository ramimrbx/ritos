#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdint>

struct Elf32_Header {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed));

struct Elf32_Phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed));

struct RbxHeader {
    char     magic[4];       // 'RBX\1'
    uint32_t load_addr;
    uint32_t entry_point;
    uint32_t code_size;
    uint32_t bss_size;
    uint32_t reserved;
    char     pad[40];        // Pad to 64 bytes total
} __attribute__((packed));

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.elf> <output.rbx>" << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = argv[2];

    std::ifstream in(input_file, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Error: Could not open input file " << input_file << std::endl;
        return 1;
    }

    std::vector<uint8_t> elf_data((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
    in.close();

    if (elf_data.size() < sizeof(Elf32_Header)) {
        std::cerr << "Error: File is too small to be a valid ELF file" << std::endl;
        return 1;
    }

    Elf32_Header header;
    std::memcpy(&header, elf_data.data(), sizeof(Elf32_Header));

    // Verify ELF magic
    if (header.e_ident[0] != 0x7F || header.e_ident[1] != 'E' ||
        header.e_ident[2] != 'L' || header.e_ident[3] != 'F') {
        std::cerr << "Error: Input is not a valid ELF file" << std::endl;
        return 1;
    }

    // Verify 32-bit
    if (header.e_ident[4] != 1) {
        std::cerr << "Error: Input must be a 32-bit ELF file" << std::endl;
        return 1;
    }

    std::vector<Elf32_Phdr> load_segments;
    for (int i = 0; i < header.e_phnum; ++i) {
        size_t off = header.e_phoff + i * header.e_phentsize;
        if (off + sizeof(Elf32_Phdr) > elf_data.size()) {
            std::cerr << "Error: Program header table out of bounds" << std::endl;
            return 1;
        }
        Elf32_Phdr phdr;
        std::memcpy(&phdr, &elf_data[off], sizeof(Elf32_Phdr));
        if (phdr.p_type == 1) { // PT_LOAD
            load_segments.push_back(phdr);
        }
    }

    if (load_segments.empty()) {
        std::cerr << "Error: No PT_LOAD segments found in ELF" << std::endl;
        return 1;
    }

    // Find the minimum load address and the total span
    uint32_t min_vaddr = load_segments[0].p_vaddr;
    uint32_t max_vaddr_end = load_segments[0].p_vaddr + load_segments[0].p_memsz;
    for (const auto& seg : load_segments) {
        min_vaddr = std::min(min_vaddr, seg.p_vaddr);
        max_vaddr_end = std::max(max_vaddr_end, seg.p_vaddr + seg.p_memsz);
    }
    uint32_t total_memsz = max_vaddr_end - min_vaddr;

    // Create a buffer for the memory image
    std::vector<uint8_t> mem_image(total_memsz, 0);

    // Copy filesz data from segments into the memory image
    uint32_t total_filesz = 0;
    for (const auto& seg : load_segments) {
        uint32_t dest_offset = seg.p_vaddr - min_vaddr;
        if (dest_offset + seg.p_filesz > total_memsz) {
            std::cerr << "Error: Segment memory write out of bounds" << std::endl;
            return 1;
        }
        if (seg.p_offset + seg.p_filesz > elf_data.size()) {
            std::cerr << "Error: Segment file offset out of bounds" << std::endl;
            return 1;
        }
        std::memcpy(&mem_image[dest_offset], &elf_data[seg.p_offset], seg.p_filesz);
        total_filesz = std::max(total_filesz, dest_offset + seg.p_filesz);
    }

    uint32_t bss_size = total_memsz - total_filesz;
    uint32_t code_size = total_filesz;

    // Build RBX header (64 bytes)
    RbxHeader rbx_hdr;
    std::memset(&rbx_hdr, 0, sizeof(RbxHeader));
    std::memcpy(rbx_hdr.magic, "RBX\x01", 4);
    rbx_hdr.load_addr = min_vaddr;
    rbx_hdr.entry_point = header.e_entry;
    rbx_hdr.code_size = code_size;
    rbx_hdr.bss_size = bss_size;

    std::ofstream out(output_file, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Error: Could not open output file " << output_file << std::endl;
        return 1;
    }

    out.write(reinterpret_cast<const char*>(&rbx_hdr), sizeof(RbxHeader));
    out.write(reinterpret_cast<const char*>(mem_image.data()), code_size);
    out.close();

    std::cout << "Successfully converted " << input_file << " to " << output_file << std::endl;
    std::cout << "  Load Address: 0x" << std::hex << min_vaddr << std::endl;
    std::cout << "  Entry Point:  0x" << std::hex << header.e_entry << std::endl;
    std::cout << "  Code Size:    " << std::dec << code_size << " bytes" << std::endl;
    std::cout << "  BSS Size:     " << std::dec << bss_size << " bytes" << std::endl;

    return 0;
}
