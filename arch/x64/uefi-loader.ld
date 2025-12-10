/*
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

/* UEFI PE32+ linker script for OSv */

INCLUDE "loader_options.ld"
INCLUDE "libc/aliases.ld"

OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)

ENTRY(efi_main)

SECTIONS
{
    /* UEFI applications are loaded at arbitrary addresses by the firmware */
    . = 0x10000000;
    
    ImageBase = .;
    
    /* PE32+ headers will be added by objcopy */
    .text : ALIGN(4096) {
        _text = .;
        *(.text.efi_main)  /* UEFI entry point first */
        *(.text.hot .text.hot.*)
        *(.text.unlikely .text.*_unlikely)
        *(.text.fixup)
        *(.text.startup .text.startup.*)
        *(.text .text.*)
        _etext = .;
    }
    
    .reloc : ALIGN(4096) {
        *(.reloc)
    }
    
    .data : ALIGN(4096) {
        _data = .;
        *(.rodata*)
        *(.data)
        *(.data.*)
        *(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*)
        *(.data.rel.ro .data.rel.ro.* .gnu.linkonce.d.rel.ro.*)
        
        /* Constructor arrays */
        _init_array_start = .;
        *(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*))
        *(.init_array .ctors)
        KEEP(*(.init_array .init_array.* .ctors.* .ctors))
        _init_array_end = .;
        
        _edata = .;
    }
    
    .dynamic : ALIGN(4096) {
        *(.dynamic)
    }
    
    .rela : ALIGN(4096) {
        *(.rela.*)
    }
    
    /* Per-CPU data */
    .percpu : ALIGN(4096) {
        _percpu_start = .;
        *(.percpu)
        . = ALIGN(4096);
        _percpu_end = .;
    }
    
    .percpu_workers : ALIGN(4096) {
        _percpu_workers_start = .;
        *(.percpu_workers)
        _percpu_workers_end = .;
    }
    
    /* Thread-local storage */
    . = ALIGN(64);
    .tdata : {
        *(.tdata .tdata.* .gnu.linkonce.td.*)
    }
    
    .tbss : {
        *(.tbss .tbss.* .gnu.linkonce.tb.*)
        _pie_static_tls_start = .;
        . = . + APP_LOCAL_EXEC_TLS_SIZE;
        . = ALIGN(64);
        _pie_static_tls_end = .;
    }
    
    .tls_template_size = SIZEOF(.tdata) + SIZEOF(.tbss);
    
    .bss : ALIGN(4096) {
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        _bss_end = .;
    }
    
    . = ALIGN(64);
    tcb0 = .;
    . = . + .tls_template_size + 256;
    .edata = .;
    
    /* Exception handling */
    .eh_frame : ALIGN(4096) {
        *(.eh_frame)
        KEEP(*(.eh_frame))
    }
    
    .eh_frame_hdr : ALIGN(4096) {
        *(.eh_frame_hdr)
        KEEP(*(.eh_frame_hdr))
    }
    
    .gcc_except_table : ALIGN(4096) {
        *(.gcc_except_table)
        *(.gcc_except_table.*)
    }
    
    /* Tracepoint support */
    .tracepoint_patch_sites ALIGN(8) : {
        __tracepoint_patch_sites_start = .;
        *(.tracepoint_patch_sites)
        __tracepoint_patch_sites_end = .;
        KEEP(*(.tracepoint_patch_sites))
    }
    
    /* Fixup tables */
    . = ALIGN(8);
    .fixup : {
        fault_fixup_start = .;
        *(.fixup)
        fault_fixup_end = .;
        KEEP(*(.fixup))
    }
    
    .memcpy_decode : ALIGN(8) {
        memcpy_decode_start = .;
        *(.memcpy_decode)
        memcpy_decode_end = .;
        KEEP(*(.memcpy_decode))
    }
    
    /* Debug sections */
    .debug_info 0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_srcinfo 0 : { *(.debug_srcinfo) }
    .debug_sfnames 0 : { *(.debug_sfnames) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_frame 0 : { *(.debug_frame) }
    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_aranges 0 : { *(.debug_aranges) }
    .debug_ranges 0 : { *(.debug_ranges) }
    .debug_line 0 : { *(.debug_line) }
    .debug_loc 0 : { *(.debug_loc) }
    .debug_str 0 : { *(.debug_str) }
    .debug_macinfo 0 : { *(.debug_macinfo) }
    .debug_typenames 0 : { *(.debug_typenames) }
    .debug_varnames 0 : { *(.debug_varnames) }
    .debug_weaknames 0 : { *(.debug_weaknames) }
    .gdb_index 0 : { *(.gdb_index) }
    .comment : { *(.comment) }
    
    /* Discard unwanted sections */
    /DISCARD/ : {
        *(.note.gnu.property)
        *(.note.gnu.build-id)
        *(.note.ABI-tag)
    }
}