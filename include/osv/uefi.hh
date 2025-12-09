/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef OSV_UEFI_HH
#define OSV_UEFI_HH

#include <osv/types.h>

namespace osv {
namespace uefi {

// UEFI basic types
typedef u64 efi_status_t;
typedef void* efi_handle_t;
typedef u64 efi_uintn_t;
typedef u16 efi_char16_t;
typedef u8 efi_boolean_t;

// UEFI Status codes
#define EFI_SUCCESS                     0
#define EFI_LOAD_ERROR                  0x8000000000000001ULL
#define EFI_INVALID_PARAMETER           0x8000000000000002ULL
#define EFI_UNSUPPORTED                 0x8000000000000003ULL
#define EFI_BAD_BUFFER_SIZE             0x8000000000000004ULL
#define EFI_BUFFER_TOO_SMALL            0x8000000000000005ULL
#define EFI_NOT_READY                   0x8000000000000006ULL
#define EFI_DEVICE_ERROR                0x8000000000000007ULL
#define EFI_WRITE_PROTECTED             0x8000000000000008ULL
#define EFI_OUT_OF_RESOURCES            0x8000000000000009ULL
#define EFI_NOT_FOUND                   0x800000000000000EULL

// UEFI GUID structure
struct efi_guid_t {
    u32 data1;
    u16 data2;
    u16 data3;
    u8 data4[8];
} __attribute__((packed));

// UEFI Time structure
struct efi_time_t {
    u16 year;
    u8 month;
    u8 day;
    u8 hour;
    u8 minute;
    u8 second;
    u8 pad1;
    u32 nanosecond;
    s16 time_zone;
    u8 daylight;
    u8 pad2;
} __attribute__((packed));

// UEFI Memory types
enum efi_memory_type_t {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiMaxMemoryType
};

// UEFI Memory descriptor
struct efi_memory_descriptor_t {
    u32 type;
    u32 pad;
    u64 physical_start;
    u64 virtual_start;
    u64 number_of_pages;
    u64 attribute;
} __attribute__((packed));

// UEFI Memory attributes
#define EFI_MEMORY_UC               0x0000000000000001ULL
#define EFI_MEMORY_WC               0x0000000000000002ULL
#define EFI_MEMORY_WT               0x0000000000000004ULL
#define EFI_MEMORY_WB               0x0000000000000008ULL
#define EFI_MEMORY_UCE              0x0000000000000010ULL
#define EFI_MEMORY_WP               0x0000000000001000ULL
#define EFI_MEMORY_RP               0x0000000000002000ULL
#define EFI_MEMORY_XP               0x0000000000004000ULL
#define EFI_MEMORY_NV               0x0000000000008000ULL
#define EFI_MEMORY_MORE_RELIABLE    0x0000000000010000ULL
#define EFI_MEMORY_RO               0x0000000000020000ULL
#define EFI_MEMORY_RUNTIME          0x8000000000000000ULL

// UEFI Table header
struct efi_table_header_t {
    u64 signature;
    u32 revision;
    u32 header_size;
    u32 crc32;
    u32 reserved;
} __attribute__((packed));

// Forward declarations for service tables
struct efi_boot_services_t;
struct efi_runtime_services_t;
struct efi_configuration_table_t;

// UEFI System Table
struct efi_system_table_t {
    efi_table_header_t hdr;
    efi_char16_t* firmware_vendor;
    u32 firmware_revision;
    u32 pad1;
    efi_handle_t console_in_handle;
    void* con_in;
    efi_handle_t console_out_handle;
    void* con_out;
    efi_handle_t standard_error_handle;
    void* std_err;
    efi_runtime_services_t* runtime_services;
    efi_boot_services_t* boot_services;
    efi_uintn_t number_of_table_entries;
    efi_configuration_table_t* configuration_table;
} __attribute__((packed));

// UEFI Configuration Table
struct efi_configuration_table_t {
    efi_guid_t vendor_guid;
    void* vendor_table;
} __attribute__((packed));

// UEFI Boot Services (simplified)
struct efi_boot_services_t {
    efi_table_header_t hdr;
    
    // Task Priority Services
    void* raise_tpl;
    void* restore_tpl;
    
    // Memory Services
    efi_status_t (*allocate_pages)(u32 type, u32 memory_type, efi_uintn_t pages, u64* memory);
    efi_status_t (*free_pages)(u64 memory, efi_uintn_t pages);
    efi_status_t (*get_memory_map)(efi_uintn_t* memory_map_size, efi_memory_descriptor_t* memory_map,
                                   efi_uintn_t* map_key, efi_uintn_t* descriptor_size,
                                   u32* descriptor_version);
    efi_status_t (*allocate_pool)(u32 pool_type, efi_uintn_t size, void** buffer);
    efi_status_t (*free_pool)(void* buffer);
    
    // Event & Timer Services
    void* create_event;
    void* set_timer;
    void* wait_for_event;
    void* signal_event;
    void* close_event;
    void* check_event;
    
    // Protocol Handler Services
    void* install_protocol_interface;
    void* reinstall_protocol_interface;
    void* uninstall_protocol_interface;
    void* handle_protocol;
    void* reserved;
    void* register_protocol_notify;
    void* locate_handle;
    void* locate_device_path;
    void* install_configuration_table;
    
    // Image Services
    void* load_image;
    void* start_image;
    efi_status_t (*exit)(efi_handle_t image_handle, efi_status_t exit_status,
                         efi_uintn_t exit_data_size, efi_char16_t* exit_data);
    void* unload_image;
    efi_status_t (*exit_boot_services)(efi_handle_t image_handle, efi_uintn_t map_key);
    
    // Miscellaneous Services
    void* get_next_monotonic_count;
    void* stall;
    void* set_watchdog_timer;
    
    // DriverSupport Services
    void* connect_controller;
    void* disconnect_controller;
    
    // Open and Close Protocol Services
    void* open_protocol;
    void* close_protocol;
    void* open_protocol_information;
    
    // Library Services
    void* protocols_per_handle;
    void* locate_handle_buffer;
    void* locate_protocol;
    void* install_multiple_protocol_interfaces;
    void* uninstall_multiple_protocol_interfaces;
    
    // 32-bit CRC Services
    void* calculate_crc32;
    
    // Miscellaneous Services
    void* copy_mem;
    void* set_mem;
    void* create_event_ex;
} __attribute__((packed));

// UEFI Runtime Services (simplified)
struct efi_runtime_services_t {
    efi_table_header_t hdr;
    
    // Time Services
    efi_status_t (*get_time)(efi_time_t* time, void* capabilities);
    efi_status_t (*set_time)(efi_time_t* time);
    efi_status_t (*get_wakeup_time)(efi_boolean_t* enabled, efi_boolean_t* pending, efi_time_t* time);
    efi_status_t (*set_wakeup_time)(efi_boolean_t enable, efi_time_t* time);
    
    // Virtual Memory Services
    efi_status_t (*set_virtual_address_map)(efi_uintn_t memory_map_size, efi_uintn_t descriptor_size,
                                            u32 descriptor_version, efi_memory_descriptor_t* virtual_map);
    efi_status_t (*convert_pointer)(efi_uintn_t debug_disposition, void** address);
    
    // Variable Services
    efi_status_t (*get_variable)(efi_char16_t* variable_name, efi_guid_t* vendor_guid,
                                 u32* attributes, efi_uintn_t* data_size, void* data);
    efi_status_t (*get_next_variable_name)(efi_uintn_t* variable_name_size, efi_char16_t* variable_name,
                                           efi_guid_t* vendor_guid);
    efi_status_t (*set_variable)(efi_char16_t* variable_name, efi_guid_t* vendor_guid,
                                 u32 attributes, efi_uintn_t data_size, void* data);
    
    // Miscellaneous Services
    efi_status_t (*get_next_high_mono_count)(u32* high_count);
    efi_status_t (*reset_system)(u32 reset_type, efi_status_t reset_status,
                                 efi_uintn_t data_size, void* reset_data);
    
    // UEFI 2.0 Capsule Services
    void* update_capsule;
    void* query_capsule_capabilities;
    
    // Miscellaneous UEFI 2.0 Service
    void* query_variable_info;
} __attribute__((packed));

// Common UEFI GUIDs
extern const efi_guid_t EFI_ACPI_20_TABLE_GUID;
extern const efi_guid_t EFI_ACPI_TABLE_GUID;
extern const efi_guid_t EFI_SMBIOS_TABLE_GUID;
extern const efi_guid_t EFI_SMBIOS3_TABLE_GUID;

// UEFI boot information structure for OSv
struct uefi_boot_info_t {
    efi_system_table_t* system_table;
    efi_handle_t image_handle;
    efi_memory_descriptor_t* memory_map;
    efi_uintn_t memory_map_size;
    efi_uintn_t memory_map_key;
    efi_uintn_t descriptor_size;
    u32 descriptor_version;
    void* acpi_table;
    void* smbios_table;
    void* smbios3_table;
} __attribute__((packed));

// Function declarations
bool is_uefi_boot();
efi_status_t uefi_init(efi_handle_t image_handle, efi_system_table_t* system_table);
void uefi_setup_memory_map();
void* uefi_find_config_table(const efi_guid_t& guid);
efi_status_t uefi_exit_boot_services();

} // namespace uefi
} // namespace osv

#endif // OSV_UEFI_HH