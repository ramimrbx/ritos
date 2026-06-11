#include <kernel/storage.h>
#include <kernel/input_output.h>

/*
 * ATA PIO driver for the primary-bus master drive (polling, no interrupts -
 * matching the rest of this port). 28-bit LBA addressing, which covers
 * disks up to 128GB; plenty for now.
 */

#define ATA_PORT_DATA          0x1F0
#define ATA_PORT_ERROR         0x1F1
#define ATA_PORT_SECTOR_COUNT  0x1F2
#define ATA_PORT_LBA_LOW       0x1F3
#define ATA_PORT_LBA_MIDDLE    0x1F4
#define ATA_PORT_LBA_HIGH      0x1F5
#define ATA_PORT_DRIVE_SELECT  0x1F6
#define ATA_PORT_COMMAND       0x1F7
#define ATA_PORT_STATUS        0x1F7

#define ATA_STATUS_ERROR       0x01
#define ATA_STATUS_DATA_READY  0x08
#define ATA_STATUS_DRIVE_FAULT 0x20
#define ATA_STATUS_BUSY        0x80

#define ATA_COMMAND_READ_SECTORS  0x20
#define ATA_COMMAND_WRITE_SECTORS 0x30
#define ATA_COMMAND_FLUSH_CACHE   0xE7
#define ATA_COMMAND_IDENTIFY      0xEC

static int      g_present = 0;
static int      g_probed  = 0;
static uint32_t g_sector_count = 0;
static char     g_model[44];

/* Wait until BUSY clears. Returns the status byte, or -1 on timeout. */
static int ata_wait_not_busy(void) {
	for (uint32_t spin = 0; spin < 1000000; spin++) {
		uint8_t status = inb(ATA_PORT_STATUS);
		if (!(status & ATA_STATUS_BUSY)) return status;
	}
	return -1;
}

/* Wait for DATA_READY after a command. 1 on success. */
static int ata_wait_data(void) {
	for (uint32_t spin = 0; spin < 1000000; spin++) {
		uint8_t status = inb(ATA_PORT_STATUS);
		if (status & (ATA_STATUS_ERROR | ATA_STATUS_DRIVE_FAULT)) return 0;
		if (!(status & ATA_STATUS_BUSY) && (status & ATA_STATUS_DATA_READY)) return 1;
	}
	return 0;
}

void storage_initialize(void) {
	if (g_probed) return;
	g_probed = 1;
	g_model[0] = '\0';

	/* Select master drive, then IDENTIFY */
	outb(ATA_PORT_DRIVE_SELECT, 0xA0);
	outb(ATA_PORT_SECTOR_COUNT, 0);
	outb(ATA_PORT_LBA_LOW, 0);
	outb(ATA_PORT_LBA_MIDDLE, 0);
	outb(ATA_PORT_LBA_HIGH, 0);
	outb(ATA_PORT_COMMAND, ATA_COMMAND_IDENTIFY);

	uint8_t status = inb(ATA_PORT_STATUS);
	if (status == 0 || status == 0xFF) return; /* no drive on the bus */

	if (ata_wait_not_busy() < 0) return;
	/* Non-ATA devices set LBA registers to a signature; reject those */
	if (inb(ATA_PORT_LBA_MIDDLE) != 0 || inb(ATA_PORT_LBA_HIGH) != 0) return;
	if (!ata_wait_data()) return;

	uint16_t identify[256];
	for (int i = 0; i < 256; i++) identify[i] = inw(ATA_PORT_DATA);

	/* Model string lives in words 27..46, characters byte-swapped */
	int m = 0;
	for (int w = 27; w <= 46; w++) {
		g_model[m++] = (char)(identify[w] >> 8);
		g_model[m++] = (char)(identify[w] & 0xFF);
	}
	g_model[m] = '\0';
	while (m > 0 && g_model[m-1] == ' ') g_model[--m] = '\0';

	/* 28-bit LBA sector count in words 60..61 */
	g_sector_count = (uint32_t)identify[60] | ((uint32_t)identify[61] << 16);
	g_present = (g_sector_count > 0);
}

int storage_present(void) {
	storage_initialize();
	return g_present;
}

const char* storage_model(void) {
	storage_initialize();
	return g_model;
}

uint32_t storage_sector_count(void) {
	storage_initialize();
	return g_sector_count;
}

static int ata_setup_transfer(uint32_t sector, uint32_t count, uint8_t command) {
	if (ata_wait_not_busy() < 0) return 0;
	outb(ATA_PORT_DRIVE_SELECT, 0xE0 | ((sector >> 24) & 0x0F));
	outb(ATA_PORT_SECTOR_COUNT, (uint8_t)count);
	outb(ATA_PORT_LBA_LOW, (uint8_t)sector);
	outb(ATA_PORT_LBA_MIDDLE, (uint8_t)(sector >> 8));
	outb(ATA_PORT_LBA_HIGH, (uint8_t)(sector >> 16));
	outb(ATA_PORT_COMMAND, command);
	return 1;
}

int storage_read_sectors(uint32_t sector, uint32_t count, void* buffer) {
	if (!storage_present() || count == 0 || count > 255) return 0;
	if (sector + count > g_sector_count) return 0;
	if (!ata_setup_transfer(sector, count, ATA_COMMAND_READ_SECTORS)) return 0;

	uint16_t* out = (uint16_t*)buffer;
	for (uint32_t s = 0; s < count; s++) {
		if (!ata_wait_data()) return 0;
		for (int i = 0; i < 256; i++) *out++ = inw(ATA_PORT_DATA);
	}
	return 1;
}

int storage_write_sectors(uint32_t sector, uint32_t count, const void* buffer) {
	if (!storage_present() || count == 0 || count > 255) return 0;
	if (sector + count > g_sector_count) return 0;
	if (!ata_setup_transfer(sector, count, ATA_COMMAND_WRITE_SECTORS)) return 0;

	const uint16_t* in = (const uint16_t*)buffer;
	for (uint32_t s = 0; s < count; s++) {
		if (!ata_wait_data()) return 0;
		for (int i = 0; i < 256; i++) outw(ATA_PORT_DATA, *in++);
	}
	if (ata_wait_not_busy() < 0) return 0;
	outb(ATA_PORT_COMMAND, ATA_COMMAND_FLUSH_CACHE);
	if (ata_wait_not_busy() < 0) return 0;
	return 1;
}
