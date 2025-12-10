// size_t __tlsdesc_static(size_t *a)
// {
// 	return a[1];
// }
.global __tlsdesc_static
.hidden __tlsdesc_static
.type __tlsdesc_static,@function
__tlsdesc_static:
	.cfi_startproc
	ldr x0,[x0,#8]
	ret
	.cfi_endproc

.global __tlsdesc_dynamic
.type __tlsdesc_dynamic,@function
__tlsdesc_dynamic:
	.cfi_startproc
//FAST PATH
        stp x1,x2,[sp, #-16]! // save the 4 registers we will be using in fast path
        .cfi_def_cfa_offset 16
        .cfi_offset x1, 0
        .cfi_offset x2, 8
        stp x3,x4,[sp, #-16]!
        .cfi_def_cfa_offset 32
        .cfi_offset x3, 16
        .cfi_offset x4, 24

        ldr x0,[x0,#8]        // p - pointer to the module_and_offset struct passed as an argument to this function
        ldr x2,[x0]           // store p->module in x2
        mrs x1,tpidr_el0
        ldr x3,[x1,#8]        // point to the dtv address stored in thread_control_block (see arch-tls.hh)
        ldr x4,[x3]           // load last_index - 1st field in the dtv struct x3 points to
        cmp x4,x2             // compare last_index with p->module
        b.lt 1f               // execute slow path to setup tls for p->module IF last_index < p->module
        ldr x4,[x3,#8]        // load first - 2nd field in the dtv struct - address of 1st element of the _tls vector (see sched.hh)
        lsl x2,x2,#3          // multiply p->module by 8 to get correct index of an entry with address of this module tls block
        ldr x4,[x4,x2]        // load the address of this module tls block to x4
        cmp x4,#0             // test if this module tls block has been setup for this thread
        b.eq 1f               // execute slow path to setup tls for p->module IF x4 is 0
                              // --- at this point this module TLS block for this thread has been setup
2:      ldr x2,[x0,#8]        // load p->offset
        add x0,x4,x2          // add p->offset to this module TLS block address and store in x0
        sub x0,x0,x1          // subtract tpidr_el0 (stored in x1) per TLS descriptor contract and return it

        ldp x3,x4,[sp],#16
        .cfi_restore x3
        .cfi_restore x4
        .cfi_def_cfa_offset 16
        ldp x1,x2,[sp],#16
        .cfi_restore x1
        .cfi_restore x2
        .cfi_def_cfa_offset 0
        ret

//SLOW PATH
1:      .cfi_def_cfa_offset 32
        stp	x29, x30, [sp, #-48]!
        .cfi_def_cfa_offset 80
        .cfi_offset x29, 32
        .cfi_offset x30, 40
        mov     x29, sp
        .cfi_def_cfa_register x29

        //Save all regular registers on the stack
        stp x0, xzr, [sp, #-16]!
        .cfi_offset x0, -16

        stp x1, x2, [sp, #-16]!
        .cfi_offset x1, -32
        .cfi_offset x2, -24
        stp x3, x4, [sp, #-16]!
        .cfi_offset x3, -48
        .cfi_offset x4, -40
        stp x5, x6, [sp, #-16]!
        .cfi_offset x5, -64
        .cfi_offset x6, -56
        stp x7, x8, [sp, #-16]!
        .cfi_offset x7, -80
        .cfi_offset x8, -72
        stp x9, x10, [sp, #-16]!
        .cfi_offset x9, -96
        .cfi_offset x10, -88
        stp x11, x12, [sp, #-16]!
        .cfi_offset x11, -112
        .cfi_offset x12, -104
        stp x13, x14, [sp, #-16]!
        .cfi_offset x13, -128
        .cfi_offset x14, -120
        stp x15, x16, [sp, #-16]!
        .cfi_offset x15, -144
        .cfi_offset x16, -136
        stp x17, x18, [sp, #-16]!
        .cfi_offset x17, -160
        .cfi_offset x18, -152
        stp x19, x20, [sp, #-16]!
        .cfi_offset x19, -176
        .cfi_offset x20, -168
        stp x21, x22, [sp, #-16]!
        .cfi_offset x21, -192
        .cfi_offset x22, -184
        stp x23, x24, [sp, #-16]!
        .cfi_offset x23, -208
        .cfi_offset x24, -200
        stp x25, x26, [sp, #-16]!
        .cfi_offset x25, -224
        .cfi_offset x26, -216
        stp x27, x28, [sp, #-16]!
        .cfi_offset x27, -240
        .cfi_offset x28, -232

        //The __tls_dynamic_setup will save all FPU registers as well
        bl __tls_dynamic_setup

        ldp x27, x28, [sp], #16
        .cfi_restore x27
        .cfi_restore x28
        ldp x25, x26, [sp], #16
        .cfi_restore x25
        .cfi_restore x26
        ldp x23, x24, [sp], #16
        .cfi_restore x23
        .cfi_restore x24
        ldp x21, x22, [sp], #16
        .cfi_restore x21
        .cfi_restore x22
        ldp x19, x20, [sp], #16
        .cfi_restore x19
        .cfi_restore x20
        ldp x17, x18, [sp], #16
        .cfi_restore x17
        .cfi_restore x18
        ldp x15, x16, [sp], #16
        .cfi_restore x15
        .cfi_restore x16
        ldp x13, x14, [sp], #16
        .cfi_restore x13
        .cfi_restore x14
        ldp x11, x12, [sp], #16
        .cfi_restore x11
        .cfi_restore x12
        ldp x9, x10, [sp], #16
        .cfi_restore x9
        .cfi_restore x10
        ldp x7, x8, [sp], #16
        .cfi_restore x7
        .cfi_restore x8
        ldp x5, x6, [sp], #16
        .cfi_restore x5
        .cfi_restore x6
        ldp x3, x4, [sp], #16
        .cfi_restore x3
        .cfi_restore x4
        ldp x1, x2, [sp], #16
        .cfi_restore x1
        .cfi_restore x2

        mov x4, x0            // put address of newly allocated TLS block into x4
        ldp x0, xzr, [sp], #16
        .cfi_restore x0

        ldp x29, x30, [sp], #48
        .cfi_restore x29
        .cfi_restore x30
        .cfi_def_cfa sp, 32

        b 2b
        .cfi_endproc
