#void cpu::reschedule_from_interrupt(bool called_from_yield,
#                                    thread_runtime::duration preempt_after)
.global reschedule_from_interrupt
.hidden reschedule_from_interrupt
.type reschedule_from_interrupt, @function
reschedule_from_interrupt:
        .cfi_startproc
        stp     x29, x30, [sp, #-192]!
        .cfi_def_cfa_offset 192
        .cfi_offset x29, 0
        .cfi_offset x30, 8
        mov	x29, sp
        .cfi_def_cfa_register x29

        #Call cpu_schedule_next_thread() to determine next thread to switch to if any
        bl cpu_schedule_next_thread

	#The cpu_schedule_next_thread returns thread_switch_data in x0 and x1,
        #where x0 holds old_thread_state and x1 holds new_thread_state
        #If cpu_schedule_next_thread() returns thread_switch_data with null pointers, exit
        cmp x0, #0
        b.eq 2f

        #Store all regular callee-save registers on the old thread stack
        stp	x19, x20, [sp, #16]
        .cfi_offset x19, 16
        .cfi_offset x20, 24
        stp	x21, x22, [sp, #32]
        .cfi_offset x21, 32
        .cfi_offset x22, 40
        stp	x23, x24, [sp, #48]
        .cfi_offset x23, 48
        .cfi_offset x24, 56
        stp	x25, x26, [sp, #64]
        .cfi_offset x25, 64
        .cfi_offset x26, 72
        stp	x27, x28, [sp, #80]
        .cfi_offset x27, 80
        .cfi_offset x28, 88

        #Store all SIMD/FP callee-save registers on the old thread stack
        stp	d8, d9, [sp, #96]
        .cfi_offset d8, 96
        .cfi_offset d9, 104
        stp	d10, d11, [sp, #112]
        .cfi_offset d10, 112
        .cfi_offset d11, 120
        stp	d12, d13, [sp, #128]
        .cfi_offset d12, 128
        .cfi_offset d13, 136
        stp	d14, d15, [sp, #144]
        .cfi_offset d14, 144
        .cfi_offset d15, 152

        #Store FP Control Register with flags that control rounding, etc
        mrs     x2, fpcr
        str     x2, [sp, #160]

        #Switch to new thread
        ldr	x2, [x1, #32]     //Fetch _tcb of the new thread
        msr	tpidr_el0, x2     //Set thread pointer
        msr	tpidr_el1, x2     //Set thread pointer
        isb

        str     x29, [x0, #0]     //Save frame pointer of the old thread

        mrs     x2, spsel         //Fetch old thread stack selector
        msr     spsel, #1         //Select SP_ELx
        mov     x3, sp            //Fetch old thread stack pointer

        adr     x4, 1f            //Fetch old thread instruction point
        stp     x3, x4, [x0, #16] //Save old thread sp and pc

        msr     spsel, #0         //Select SP_EL0
        mov     x3, sp            //Fetch old thread exception stack pointer
        stp     x3, x2, [x0, #40] //Save old thread exception stack pointer and stack selector

        ldp     x29, x0, [x1, #0] //Set frame pointer of the new thread and this (x0) of the new thread
                                  //Please note that the pc may point to thread_main_c(thread*) which is
                                  //why we have to set x0 (1st argument) to new thread object
        ldp     x3, x4, [x1, #16] //Fetch new thread sp and pc

        msr     spsel, #1         //Select SP_ELx
        mov     sp, x3            //Restore new thread stack pointer

        ldp     x3, x2, [x1, #40] //Load new thread exception stack pointer and stack selector
        msr     spsel, #0         //Select SP_EL0
        mov     sp, x3            //Restore new thread exception stack pointer
        msr     spsel, x2         //Restore new thread stack selector (1-SP_ELx,0-SP_EL0)

        blr     x4                //Jump to the new thread pc

1:
        #Restore all regular callee-save registers from the new thread stack
        ldp     x19, x20, [sp, #16]
        ldp     x21, x22, [sp, #32]
        ldp     x23, x24, [sp, #48]
        ldp     x25, x26, [sp, #64]
        ldp     x27, x28, [sp, #80]

        #Restore all SIMD/FP callee-save registers from the new thread stack
        ldp	d8, d9, [sp, #96]
        ldp	d10, d11, [sp, #112]
        ldp	d12, d13, [sp, #128]
        ldp	d14, d15, [sp, #144]

        #Restore FP Control Register with flags that control rounding, etc
        ldr     x2, [sp, #160]
        msr     fpcr, x2

        #Call destroy_current_cpu_terminating_thread()
        bl destroy_current_cpu_terminating_thread

2:
        ldp     x29, x30, [sp], #192
        .cfi_restore x29
        .cfi_restore x30
        .cfi_def_cfa sp, 0
        ret
        .cfi_endproc
