/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <osv/uefi.hh>
#include <osv/debug.hh>
#include <osv/boot.hh>
#include <osv/mmu.hh>
#include <osv/mempool.hh>
#include "arch-setup.hh"
#include "processor.hh"

using namespace osv::uefi;

// Global UEFI boot information
static uefi_boot_info_t uefi_boot_info;
static bool uefi_boot_detected = false;

// UEFI GUIDs
const efi_guid_t EFI_ACPI_20_TABLE_GUID = {
    0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}
};

const efi_guid_t EFI_ACPI_TABLE_GUID = {
    0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}
};

const efi_guid_t EFI_SMBIOS_TABLE_GUID = {
    0xeb9d2d31, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}
};

const efi_guid_t EFI_SMBIOS3_TABLE_GUID = {
    0xf2fd1544, 0x9794, 0x4a2c, {0x99, 0x2e, 0xe5, 0xbb, 0xcf, 0x20, 0xe3, 0x94}
};

namespace osv {
namespace uefi {

bool is_uefi_boot()
{
    return uefi_boot_detected;
}

static bool guid_equal(const efi_guid_t& a, const efi_guid_t& b)
{
    return a.data1 == b.data1 && a.data2 == b.data2 && a.data3 == b.data3 &&
           memcmp(a.data4, b.data4, 8) == 0;
}

void* uefi_find_config_table(const efi_guid_t& guid)
{
    if (!uefi_boot_detected || !uefi_boot_info.system_table) {
        return nullptr;
    }

    efi_system_table_t* st = uefi_boot_info.system_table;
    for (efi_uintn_t i = 0; i < st->number_of_table_entries; i++) {
        if (guid_equal(st->configuration_table[i].vendor_guid, guid)) {
            return st->configuration_table[i].vendor_table;
        }
    }
    return nullptr;
}

static efi_status_t get_memory_map()
{
    efi_boot_services_t* bs = uefi_boot_info.system_table->boot_services;
    efi_status_t status;
    
    // First call to get the size
    uefi_boot_info.memory_map_size = 0;
    status = bs->get_memory_map(&uefi_boot_info.memory_map_size, nullptr,
                                &uefi_boot_info.memory_map_key,
                                &uefi_boot_info.descriptor_size,
                                &uefi_boot_info.descriptor_version);
    
    if (status != EFI_BUFFER_TOO_SMALL) {
        debug_early("UEFI: Failed to get memory map size\n");
        return status;
    }
    
    // Add some extra space for new allocations
    uefi_boot_info.memory_map_size += 2 * uefi_boot_info.descriptor_size;
    
    // Allocate memory for the map
    status = bs->allocate_pool(EfiLoaderData, uefi_boot_info.memory_map_size,
                               (void**)&uefi_boot_info.memory_map);
    if (status != EFI_SUCCESS) {
        debug_early("UEFI: Failed to allocate memory for memory map\n");
        return status;
    }
    
    // Get the actual memory map
    status = bs->get_memory_map(&uefi_boot_info.memory_map_size,
                                uefi_boot_info.memory_map,
                                &uefi_boot_info.memory_map_key,
                                &uefi_boot_info.descriptor_size,
                                &uefi_boot_info.descriptor_version);
    
    if (status != EFI_SUCCESS) {
        debug_early("UEFI: Failed to get memory map\n");
        bs->free_pool(uefi_boot_info.memory_map);
        uefi_boot_info.memory_map = nullptr;
        return status;
    }
    
    return EFI_SUCCESS;
}

void uefi_setup_memory_map()
{
    if (!uefi_boot_detected) {
        return;
    }
    
    debug_early("UEFI: Setting up memory map\n");
    
    // Convert UEFI memory map to OSv format
    efi_memory_descriptor_t* desc = uefi_boot_info.memory_map;
    efi_uintn_t entries = uefi_boot_info.memory_map_size / uefi_boot_info.descriptor_size;
    
    for (efi_uintn_t i = 0; i < entries; i++) {
        u64 start = desc->physical_start;
        u64 size = desc->number_of_pages * 4096; // EFI pages are 4KB
        
        // Only add conventional memory to the memory pool
        if (desc->type == EfiConventionalMemory) {
            debug_early_u64("UEFI: Adding memory region: start=", start);
            debug_early_u64(" size=", size);
            debug_early("\n");
            
            // Add to OSv memory pool
            memory::phys_mem_size += size;
            // Note: Actual memory pool setup will be done later in arch_setup_free_memory()
        }
        
        // Move to next descriptor
        desc = (efi_memory_descriptor_t*)((u8*)desc + uefi_boot_info.descriptor_size);
    }
}

efi_status_t uefi_exit_boot_services()
{
    if (!uefi_boot_detected) {
        return EFI_UNSUPPORTED;
    }
    
    debug_early("UEFI: Exiting boot services\n");
    
    efi_boot_services_t* bs = uefi_boot_info.system_table->boot_services;
    efi_status_t status = bs->exit_boot_services(uefi_boot_info.image_handle,
                                                 uefi_boot_info.memory_map_key);
    
    if (status != EFI_SUCCESS) {
        debug_early("UEFI: Failed to exit boot services, retrying...\n");
        
        // Memory map might have changed, get it again
        if (uefi_boot_info.memory_map) {
            bs->free_pool(uefi_boot_info.memory_map);
        }
        
        status = get_memory_map();
        if (status != EFI_SUCCESS) {
            return status;
        }
        
        // Try again
        status = bs->exit_boot_services(uefi_boot_info.image_handle,
                                        uefi_boot_info.memory_map_key);
    }
    
    if (status == EFI_SUCCESS) {
        debug_early("UEFI: Boot services exited successfully\n");
    } else {
        debug_early("UEFI: Failed to exit boot services\n");
    }
    
    return status;
}

efi_status_t uefi_init(efi_handle_t image_handle, efi_system_table_t* system_table)
{
    debug_early("UEFI: Initializing UEFI boot\n");
    
    // Mark that we're booting via UEFI
    uefi_boot_detected = true;
    
    // Store boot information
    uefi_boot_info.image_handle = image_handle;
    uefi_boot_info.system_table = system_table;
    
    // Find configuration tables
    uefi_boot_info.acpi_table = uefi_find_config_table(EFI_ACPI_20_TABLE_GUID);
    if (!uefi_boot_info.acpi_table) {
        uefi_boot_info.acpi_table = uefi_find_config_table(EFI_ACPI_TABLE_GUID);
    }
    
    uefi_boot_info.smbios3_table = uefi_find_config_table(EFI_SMBIOS3_TABLE_GUID);
    uefi_boot_info.smbios_table = uefi_find_config_table(EFI_SMBIOS_TABLE_GUID);
    
    debug_early("UEFI: Found ACPI table: ");
    debug_early_u64("", (u64)uefi_boot_info.acpi_table);
    debug_early("\n");
    
    // Get memory map
    efi_status_t status = get_memory_map();
    if (status != EFI_SUCCESS) {
        debug_early("UEFI: Failed to get memory map\n");
        return status;
    }
    
    debug_early("UEFI: Initialization complete\n");
    return EFI_SUCCESS;
}

} // namespace uefi
} // namespace osv

// Forward declarations
extern "C" {
    void premain();
    int main(int argc, char** argv);
    extern int __loader_argc;
    extern char** __loader_argv;
}

// UEFI Application Entry Point
// This is the main entry point for UEFI boot
extern "C" efi_status_t efi_main(efi_handle_t image_handle, efi_system_table_t* system_table)
{
    // Initialize UEFI boot
    efi_status_t status = uefi_init(image_handle, system_table);
    if (status != EFI_SUCCESS) {
        return status;
    }
    
    // Set up memory mapping from UEFI
    uefi_setup_memory_map();
    
    // Exit UEFI boot services before starting OSv kernel
    status = uefi_exit_boot_services();
    if (status != EFI_SUCCESS) {
        return status;
    }
    
    // Clear BSS section
    extern char _bss_start[], _bss_end[];
    memset(_bss_start, 0, _bss_end - _bss_start);
    
    // Set up initial stack and call OSv initialization
    // Note: We're already in 64-bit mode when UEFI calls us
    
    // Call OSv premain initialization
    premain();
    
    // Call OSv main
    main(__loader_argc, __loader_argv);
    
    // Should never reach here
    return EFI_SUCCESS;
}