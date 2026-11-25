.syntax unified
.cpu cortex-m4
.thumb

.extern main

.global Reset_Handler
.global Default_Handler

.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object
g_pfnVectors:
    .word _estack
    .word Reset_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word Default_Handler
    .word 0
    .word 0
    .word Default_Handler
    .word SysTick_Handler
    .rept 67
    .word Default_Handler
    .endr
    .word OTG_FS_IRQHandler
    .rept 14
    .word Default_Handler
    .endr
.size g_pfnVectors, .-g_pfnVectors

.section .text.Reset_Handler,"ax",%progbits
.thumb
.thumb_func
.type Reset_Handler, %function
Reset_Handler:
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata

data_copy:
    cmp r1, r2
    bhs data_copy_done
    ldr r3, [r0], #4
    str r3, [r1], #4
    b data_copy
data_copy_done:

    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0

bss_zero:
    cmp r0, r1
    bhs bss_zero_done
    str r2, [r0], #4
    b bss_zero
bss_zero_done:

    bl main

loop_forever:
    b loop_forever
.size Reset_Handler, .-Reset_Handler

.weak SysTick_Handler
.thumb_set SysTick_Handler, Default_Handler
.weak OTG_FS_IRQHandler
.thumb_set OTG_FS_IRQHandler, Default_Handler

.section .text.Default_Handler,"ax",%progbits
.thumb
.thumb_func
.type Default_Handler, %function
Default_Handler:
    b Default_Handler
.size Default_Handler, .-Default_Handler
