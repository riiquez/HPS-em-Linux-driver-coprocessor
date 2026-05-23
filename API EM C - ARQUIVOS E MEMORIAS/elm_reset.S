        .syntax unified
        .arch armv7-a
        .cpu cortex-a9

        .text
        .align 2

        .global elm_reset
        .type   elm_reset, %function

elm_reset:
        push    {r4, lr}

        @ r0 = ports*

        ldr     r4, [r0, #4]      @ ctrl

        @ ativa reset (bit 2 = 1)
        mov     r1, #4
        str     r1, [r4]

        @ desativa reset
        mov     r1, #0
        str     r1, [r4]

        mov     r0, #0

        pop     {r4, pc}

        .size elm_reset, .-elm_reset
