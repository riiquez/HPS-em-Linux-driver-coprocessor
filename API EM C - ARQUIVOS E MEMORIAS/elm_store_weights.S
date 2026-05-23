@
@ Busy (bit 5): fl_processor_busy so libera quando enable baixa (IDLE).

        .syntax unified
        .arch armv7-a
        .cpu cortex-a9

        .equ OPCODE_WEIGHT_ADDR,    1
        .equ OPCODE_WEIGHT_VALUE,   2
        .equ WIN_MAX_ENTRIES,       100352

        .equ MASK_DONE,   (1 << 4)
        .equ MASK_ERROR, (1 << 6)

        .text
        .align 2

@ --- int elm_weights_send_addr(elm_ports_t *ports, unsigned addr) ------------
        .global elm_weights_send_addr
        .type   elm_weights_send_addr, %function

elm_weights_send_addr:
        push    {r4-r8, lr}
        @ r0 ports*, r1 endereco fisico/indice dentro mem_win [0 .. 100351]
        ldr     r12, =WIN_MAX_ENTRIES
        cmp     r1, r12
        bhs     .Lbad_addr

        ldr     r4, [r0, #0]
        ldr     r5, [r0, #4]

        mov     r3, OPCODE_WEIGHT_ADDR          @ garante opcao baixa primeiro
        mov     r12, r1
        lsl     r12, r12, #3
        orr     r3, r3, r12

        str     r3, [r4]
        mov     r12, #1
        str     r12, [r5]
        mov     r12, #0
        str     r12, [r5]

        mov     r0, #0
        pop     {r4-r8, pc}

.Lbad_addr:
        mov     r0, #-3
        pop     {r4-r8, pc}

        .size   elm_weights_send_addr, .-elm_weights_send_addr

@ --- int elm_weights_send_value(elm_ports_t *ports, unsigned value_q4_12) ----
        .global elm_weights_send_value
        .type   elm_weights_send_value, %function

elm_weights_send_value:
        push    {r4-r8, lr}
        @ r0 ports*, r1 valor 16-bit (so usamos fundo dos 32-bit)
        ldr     r4, [r0, #0]
        ldr     r5, [r0, #4]
        ldr     r6, [r0, #8]

        mov     r3, OPCODE_WEIGHT_VALUE
        uxth    r12, r1                 @ garante apenas 16 bits
        lsl     r12, r12, #3
        orr     r3, r3, r12

        str     r3, [r4]
        mov     r12, #1
        str     r12, [r5]
        mov     r12, #0
        str     r12, [r5]

.Lpollw:
        ldr     r12, [r6]
        tst     r12, #MASK_ERROR
        bne     .Lhw_err_w
        tst     r12, #MASK_DONE
        beq     .Lpollw

        mov     r12, #2
        str     r12, [r5]
        mov     r12, #0
        str     r12, [r5]

        mov     r0, #0
        pop     {r4-r8, pc}

.Lhw_err_w:
        mov     r0, #-2
        pop     {r4-r8, pc}

        .size   elm_weights_send_value, .-elm_weights_send_value
