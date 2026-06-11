#include <kernel/power.h>
#include <kernel/io.h>
#include <stdint.h>
#include <stddef.h>

struct RSDPDescriptor {
	char Signature[8];
	uint8_t Checksum;
	char OEMID[6];
	uint8_t Revision;
	uint32_t RsdtAddress;
} __attribute__ ((packed));

struct ACPISDTHeader {
	char Signature[4];
	uint32_t Length;
	uint8_t Revision;
	uint8_t Checksum;
	char OEMID[6];
	char OEMTableID[8];
	uint32_t OEMRevision;
	uint32_t CreatorID;
	uint32_t CreatorRevision;
} __attribute__ ((packed));

struct FADT {
	struct ACPISDTHeader h;
	uint32_t FirmwareCtrl;
	uint32_t Dsdt;
	uint8_t Reserved;
	uint8_t PreferredPMProfile;
	uint16_t SCI_Interrupt;
	uint32_t SMI_CmdPort;
	uint8_t AcpiEnable;
	uint8_t AcpiDisable;
	uint8_t S4BiosReq;
	uint8_t PSTATE_Control;
	uint32_t PM1aEventBlock;
	uint32_t PM1bEventBlock;
	uint32_t PM1aControlBlock;
	uint32_t PM1bControlBlock;
} __attribute__ ((packed));

static uint8_t* find_rsdp(void) {
	// 1. Scan EBDA first
	uint32_t ebda_phys = (*(uint16_t*)0x40E) << 4;
	if (ebda_phys >= 0x80000 && ebda_phys < 0xA0000) {
		for (uint32_t addr = ebda_phys; addr < ebda_phys + 1024; addr += 16) {
			if (*(uint32_t*)addr == 0x20445352 && *(uint32_t*)(addr + 4) == 0x20525450) { // "RSD PTR "
				// Verify checksum
				uint8_t sum = 0;
				uint8_t* bytes = (uint8_t*)addr;
				for (int i = 0; i < 20; i++) {
					sum += bytes[i];
				}
				if (sum == 0) {
					return bytes;
				}
			}
		}
	}

	// 2. Scan BIOS ROM area 0xE0000 to 0xFFFFF
	for (uint32_t addr = 0xE0000; addr < 0x100000; addr += 16) {
		if (*(uint32_t*)addr == 0x20445352 && *(uint32_t*)(addr + 4) == 0x20525450) { // "RSD PTR "
			// Verify checksum
			uint8_t sum = 0;
			uint8_t* bytes = (uint8_t*)addr;
			for (int i = 0; i < 20; i++) {
				sum += bytes[i];
			}
			if (sum == 0) {
				return bytes;
			}
		}
	}

	return NULL;
}

static uint8_t* scan_s5(uint8_t* start, uint32_t length) {
	if (start == NULL || length < 8) return NULL;
	for (uint32_t i = 4; i < length - 4; i++) {
		if (start[i] == '_' && start[i+1] == 'S' && start[i+2] == '5' && start[i+3] == '_') {
			// Check if name prepended with NameOp (0x08)
			if (start[i-1] == 0x08 && start[i+4] == 0x12) {
				return &start[i];
			}
		}
	}
	return NULL;
}

void sys_reboot(void) {
	// 1. Reset CPU via PS/2 keyboard controller pulse
	// Poll 8042 keyboard controller
	for (int i = 0; i < 1000; i++) {
		uint8_t temp = inb(0x64);
		if ((temp & 0x02) == 0) {
			break;
		}
		// Read input buffer if full to clear it
		if ((temp & 0x01) != 0) {
			inb(0x60);
		}
	}
	outb(0x64, 0xFE);

	// 2. PCI Reset Register
	// Write 0x06 to 0xCF9 (Reset Control Register)
	// (Writing 0x02 then 0x06 is standard)
	outb(0xCF9, 0x02);
	outb(0xCF9, 0x06);

	// 3. Fallback: Force CPU reset via Triple Fault
	__asm__ volatile (
		"lidt %0\n\t"
		"int $3" 
		: 
		: "m"((uint16_t[3]){0, 0, 0})
	);

	// 4. Ultimate fallback: Hang CPU
	while (1) {
		__asm__ volatile("cli; hlt");
	}
}

void sys_shutdown(void) {
	// Try ACPI S5 shutdown on real hardware first
	uint8_t* rsdp_ptr = find_rsdp();
	if (rsdp_ptr != NULL) {
		struct RSDPDescriptor* rsdp = (struct RSDPDescriptor*)rsdp_ptr;
		struct ACPISDTHeader* rsdt = (struct ACPISDTHeader*)rsdp->RsdtAddress;
		if (rsdt != NULL && rsdt->Signature[0] == 'R' && rsdt->Signature[1] == 'S' && rsdt->Signature[2] == 'D' && rsdt->Signature[3] == 'T') {
			int entries = (rsdt->Length - sizeof(struct ACPISDTHeader)) / 4;
			uint32_t* other_tables = (uint32_t*)((uintptr_t)rsdt + sizeof(struct ACPISDTHeader));
			struct FADT* fadt = NULL;
			
			for (int i = 0; i < entries; i++) {
				struct ACPISDTHeader* h = (struct ACPISDTHeader*)other_tables[i];
				if (h != NULL && h->Signature[0] == 'F' && h->Signature[1] == 'A' && h->Signature[2] == 'C' && h->Signature[3] == 'P') {
					fadt = (struct FADT*)h;
					break;
				}
			}
			
			if (fadt != NULL) {
				// Enable ACPI
				if (fadt->SMI_CmdPort != 0 && fadt->AcpiEnable != 0) {
					outb(fadt->SMI_CmdPort, fadt->AcpiEnable);
					// Wait for ACPI to be enabled
					for (int i = 0; i < 300; i++) {
						if (inw(fadt->PM1aControlBlock) & 1) {
							break;
						}
						// small delay
						for (volatile int d = 0; d < 10000; d++);
					}
				}
				
				// Search for _S5 package in DSDT
				if (fadt->Dsdt != 0) {
					struct ACPISDTHeader* dsdt_h = (struct ACPISDTHeader*)fadt->Dsdt;
					uint8_t* s5_ptr = scan_s5((uint8_t*)fadt->Dsdt, dsdt_h->Length);
					if (s5_ptr != NULL) {
						uint8_t* s5_val_ptr = s5_ptr + 4; // skip "_S5_"
						if (*s5_val_ptr == 0x12) { // PackageOp
							s5_val_ptr++;
							// Skip Package Length bytes
							uint8_t pkg_len_bytes = (*s5_val_ptr >> 6) & 0x03;
							s5_val_ptr += (pkg_len_bytes + 1);
							
							uint8_t num_elements = *s5_val_ptr;
							s5_val_ptr++; // skip num_elements
							
							uint16_t slp_typa = 0;
							uint16_t slp_typb = 0;
							
							// First element: SLP_TYPa
							if (num_elements > 0) {
								if (*s5_val_ptr == 0x0A) {
									slp_typa = *(s5_val_ptr + 1);
									s5_val_ptr += 2;
								} else if (*s5_val_ptr == 0x0B) {
									slp_typa = *(uint16_t*)(s5_val_ptr + 1);
									s5_val_ptr += 3;
								} else if (*s5_val_ptr <= 0x08) {
									slp_typa = *s5_val_ptr;
									s5_val_ptr += 1;
								}
							}
							
							// Second element: SLP_TYPb
							if (num_elements > 1) {
								if (*s5_val_ptr == 0x0A) {
									slp_typb = *(s5_val_ptr + 1);
								} else if (*s5_val_ptr == 0x0B) {
									slp_typb = *(uint16_t*)(s5_val_ptr + 1);
								} else if (*s5_val_ptr <= 0x08) {
									slp_typb = *s5_val_ptr;
								}
							}
							
							uint16_t slp_en = 1 << 13;
							outw(fadt->PM1aControlBlock, (slp_typa << 10) | slp_en);
							if (fadt->PM1bControlBlock != 0) {
								outw(fadt->PM1bControlBlock, (slp_typb << 10) | slp_en);
							}
						}
					}
				}
			}
		}
	}

	// Emulator fallbacks
	// QEMU / Bochs (newer): write 0x2000 (SLP_EN | SLP_TYP) to PM1a_CNT
	outw(0x604, 0x2000);

	// QEMU / Bochs (older)
	outw(0xB004, 0x2000);

	// VirtualBox
	outw(0x4004, 0x3400);

	// Ultimate fallback: Hang CPU
	while (1) {
		__asm__ volatile("cli; hlt");
	}
}
