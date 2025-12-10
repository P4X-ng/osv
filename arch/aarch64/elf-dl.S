/*
 * Copyright (C) 2014 Huawei Technologies Duesseldorf GmbH
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "macros.S"

.text

.global __elf_resolve_pltgot
.hidden __elf_resolve_pltgot

// calling convention:
// ip0: &pltgot(2)
// ip1: &__elf_resolve_pltgot
// [sp, #8] lr
// [sp, #0] &PLTGOT[n]

__elf_resolve_pltgot:
.type __elf_resolve_pltgot, @function
        .cfi_startproc
        stp     x8, xzr, [sp, #-80]! // space for x0-x8
        .cfi_def_cfa_offset 80
        .cfi_offset x8, 0
        stp     x6, x7, [sp, #16]
        .cfi_offset x6, 16
        .cfi_offset x7, 24
        stp     x4, x5, [sp, #32]
        .cfi_offset x4, 32
        .cfi_offset x5, 40
        stp     x2, x3, [sp, #48]
        .cfi_offset x2, 48
        .cfi_offset x3, 56
        stp     x0, x1, [sp, #64]
        .cfi_offset x0, 64
        .cfi_offset x1, 72

        ldr     x0, [sp, #80]       // load &PLTGOT[n]
        ldr     x1, [x16, #-8]      // load object
        sub     x0, x0, x16         // &PLTGOT[n] - &PLTGOT[2]
        lsr     x0, x0, #3
        sub     x0, x0, #1          // x0 = index

        bl      elf_resolve_pltgot
        mov     x16, x0

	ldp	x0, x1, [sp, #64]
	ldp	x2, x3, [sp, #48]
	ldp	x4, x5, [sp, #32]
	ldp	x6, x7, [sp, #16]
	ldp	x8, xzr, [sp], #(80)
        .cfi_restore x8
        .cfi_restore x7
        .cfi_restore x6
        .cfi_restore x5
        .cfi_restore x4
        .cfi_restore x3
        .cfi_restore x2
        .cfi_restore x1
        .cfi_restore x0
        .cfi_def_cfa_offset 16

        ldp     x17, x30, [sp], #16
        .cfi_restore x17
        .cfi_restore x30
        .cfi_def_cfa_offset 0
        br      x16
        .cfi_endproc
