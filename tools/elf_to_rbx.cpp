// Packages the linker's ELF output into RBX, RitOS's native executable
// format (see rit/rbx_format.h). The OS only ever loads .rbx files; ELF
// exists solely as the intermediate the host toolchain happens to produce.
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdint>

#include <rit/rbx_format.h>

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

int main(int argc, char* argv[]) {
    uint16_t rbx_type = RBX_TYPE_MODULE;
    std::string icon_file;
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--type=program")      rbx_type = RBX_TYPE_PROGRAM;
        else if (a == "--type=module")  rbx_type = RBX_TYPE_MODULE;
        else if (a.rfind("--icon=", 0) == 0) icon_file = a.substr(7);
        else args.push_back(a);
    }
    if (args.size() != 2) {
        std::cerr << "Usage: " << argv[0] << " [--type=module|program] [--icon=icon.argb] <input.elf> <output.rbx>" << std::endl;
        return 1;
    }

    std::vector<uint8_t> icon_data;
    if (!icon_file.empty()) {
        std::ifstream icon_in(icon_file, std::ios::binary);
        if (!icon_in.is_open()) {
            std::cerr << "Error: Could not open icon file " << icon_file << std::endl;
            return 1;
        }
        icon_data.assign(std::istreambuf_iterator<char>(icon_in),
                         std::istreambuf_iterator<char>());
        if (icon_data.size() != RBX_ICON_BYTES) {
            std::cerr << "Error: icon must be exactly " << RBX_ICON_BYTES
                      << " bytes (32x32 ARGB), got " << icon_data.size() << std::endl;
            return 1;
        }
    }

    std::string input_file = args[0];
    std::string output_file = args[1];

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
        if (phdr.p_type != 1) continue; // PT_LOAD only
        // ld emits a segment that maps nothing but the ELF/program headers
        // themselves; that's toolchain bookkeeping, not program image, and
        // has no place in an .rbx.
        uint32_t hdr_region_end = header.e_phoff + (uint32_t)header.e_phnum * header.e_phentsize;
        if (phdr.p_offset + phdr.p_filesz <= hdr_region_end) continue;
        load_segments.push_back(phdr);
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

    // Display name: output basename without the .rbx suffix
    std::string name = output_file;
    size_t slash = name.find_last_of('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    if (name.size() > 4 && name.substr(name.size() - 4) == ".rbx")
        name = name.substr(0, name.size() - 4);

    rbx_header_t rbx_hdr;
    std::memset(&rbx_hdr, 0, sizeof(rbx_hdr));
    std::memcpy(rbx_hdr.magic, RBX_MAGIC, 4);
    rbx_hdr.version     = RBX_VERSION;
    rbx_hdr.arch        = RBX_ARCH_X86;
    rbx_hdr.type        = rbx_type;
    rbx_hdr.header_size = sizeof(rbx_hdr);
    rbx_hdr.load_addr   = min_vaddr;
    rbx_hdr.entry_point = header.e_entry;
    rbx_hdr.code_size   = code_size;
    rbx_hdr.bss_size    = bss_size;
    rbx_hdr.checksum    = rbx_checksum(mem_image.data(), code_size);
    if (!icon_data.empty()) {
        rbx_hdr.icon_offset = sizeof(rbx_hdr) + code_size;
        rbx_hdr.icon_size   = (uint32_t)icon_data.size();
    }
    std::strncpy(rbx_hdr.name, name.c_str(), sizeof(rbx_hdr.name) - 1);

    std::ofstream out(output_file, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Error: Could not open output file " << output_file << std::endl;
        return 1;
    }

    out.write(reinterpret_cast<const char*>(&rbx_hdr), sizeof(rbx_hdr));
    out.write(reinterpret_cast<const char*>(mem_image.data()), code_size);
    if (!icon_data.empty())
        out.write(reinterpret_cast<const char*>(icon_data.data()), icon_data.size());
    out.close();

    std::cout << "Packaged " << output_file << " (RBX v" << RBX_VERSION << ", "
              << (rbx_type == RBX_TYPE_PROGRAM ? "program" : "module") << ")" << std::endl;
    std::cout << "  Load Address: 0x" << std::hex << min_vaddr << std::endl;
    std::cout << "  Entry Point:  0x" << std::hex << header.e_entry << std::endl;
    std::cout << "  Code Size:    " << std::dec << code_size << " bytes" << std::endl;
    std::cout << "  BSS Size:     " << std::dec << bss_size << " bytes" << std::endl;

    return 0;
}
