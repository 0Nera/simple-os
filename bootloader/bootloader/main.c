
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../string/string.h"
#include "../tar/tar.h"
#include "../elf/elf.h"
#include "../multiboot/multiboot.h"

#include "../arch/i386/ata.h"
#include "../arch/i386/tty.h"

// The whole bootloader binary is assumed to take the first 16 sectors
// must be in sync of BOOTLOADER_MAX_SIZE in Makefile
#define BOOTLOADER_SECTORS 16
#define KERNEL_BOOT_IMG "/boot/simple_os.kernel"

// Defined in memory.asm
extern uint32_t ADDR_MMAP_ADDR; // address of the memory map structure
extern uint32_t ADDR_MMAP_COUNT; // count of the memory map entries

// Load a file from the tar file system
//
// LBA: 0-based Linear Block Address, 28bit LBA shall be between 0 to 0x0FFFFFFF
// filename: get a file with this name from the tar file system
// buffer: load the file found into this address
//
// return: file size of the loaded file
int tar_loopup_lazy(uint32_t LBA, char* filename, unsigned char* buffer) {
    uint32_t max_lba = get_total_28bit_sectors();
    if (LBA >= max_lba) {
        return TAR_ERR_LBA_GT_MAX_SECTOR;
    }
    read_sectors_ATA_28bit_PIO((uint16_t*)buffer, LBA, 1);
    int match = tar_match_filename(buffer, filename);
    if (match == TAR_ERR_NOT_USTAR) {
        return TAR_ERR_NOT_USTAR;
    } else {
        int filesize = tar_get_filesize(buffer);
        int size_in_sector = ((filesize + 511) / 512) + 1; // plus one for the meta sector
        if (match == TAR_ERR_FILE_NAME_NOT_MATCH) {
            return tar_loopup_lazy(LBA + size_in_sector, filename, buffer);
        } else {
            read_sectors_ATA_28bit_PIO((uint16_t*)buffer, LBA + 1, size_in_sector);
            return filesize;
        }
    }

}

void bootloader_main(void) {
    // printing starting at row 16
    uint8_t console_print_row = 16;

    // Init multiboot info structure and memory map info
    // Ref: https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
    // Store the Multiboot info structure at the end of conventional memory space 0x00007E00 - 0x0007FFFF
    multiboot_info_t* ptr_multiboot_info = (multiboot_info_t*)0x00080000 - sizeof(multiboot_info_t);
    ptr_multiboot_info->flags = 0b1000000; // Flag the memory layout info is available
    ptr_multiboot_info->mmap_length = (*(uint32_t*)ADDR_MMAP_COUNT) * sizeof(multiboot_memory_map_t);
    ptr_multiboot_info->mmap_addr = ADDR_MMAP_ADDR;

    // Load kernel from tar file system to this address
    char* kernel_buffer = (char*)0x01000000;
    int kernel_size = tar_loopup_lazy(BOOTLOADER_SECTORS, KERNEL_BOOT_IMG, (unsigned char*)kernel_buffer);
    print_str("Kernel Image Loaded at (Little Endian Hex):", console_print_row++, 0);
    print_memory_hex((char*)&kernel_buffer, sizeof(kernel_buffer), console_print_row++);
    print_str("Kernel size (Little Endian Hex):", console_print_row++, 0);
    print_memory_hex((char*)&kernel_size, sizeof(int), console_print_row++);
    // print_str("First 32 bytes of kernel:", console_print_row++, 0);
    // print_memory(kernel_buffer, 32, console_print_row++, 0);

    if (is_elf(kernel_buffer)) {
        // print_str("ELF Kernel Detected, now loading...", console_print_row++, 0);
        Elf32_Addr entry_pioint_physical = load_elf(kernel_buffer);
        // print_str("ELF Kernel first 16 bytes:", console_print_row++, 0);
        // print_memory_hex((char*) entry_pioint_physical, 16, console_print_row++);
        print_str("Jumping to ELF Kernel physical entry point (Little Endian Hex):", console_print_row++, 0);
        print_memory_hex((char*)&entry_pioint_physical, sizeof(entry_pioint_physical), console_print_row++);

        // Jump to the physical entry point of the loaded ELF kernel image 
        void* entry_point = (void*)entry_pioint_physical;

        // Multiboot convention: use ebx to store the pointer to the multiboot info structure
        print_str("Multiboot structure saved at (Little Endian Hex):", console_print_row++, 0);
        print_memory_hex((char*)&ptr_multiboot_info, sizeof(ptr_multiboot_info), console_print_row++);
        asm volatile("mov %0, %%ebx":: "r"(ptr_multiboot_info));

        goto *entry_point;
    }

}