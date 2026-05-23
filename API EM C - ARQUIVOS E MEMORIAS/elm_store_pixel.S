@
@ store_img:
@ formato: [20:13] pixel 8bit  | [12:3] indice  | [2:0] opc 000
@ data_out :
@   bits [3:0]   resultado inferencia (nao usamos aqui)
@   bit 4        fl_processor_done
@   bit 5        fl_processor_busy
@   bit 6        fl_error
@ este codigo vai ter mais comentarios pois foi a primeira instrução que montamos, exibiremos o que fomos montando pois a base segue para as outras instrucoes

        .syntax unified
        .arch armv7-a
        .cpu cortex-a9

        .equ MASK_DONE,   (1 << 4)
        .equ MASK_ERROR, (1 << 6)

        .text
        .align 2
        .global elm_store_img_pixel
        .type   elm_store_img_pixel, %function

elm_store_img_pixel:
        push    {r4-r8, lr}

        @ r0 ports*, r1 pixel, r2 index 
        ldr     r4, [r0, #0]          @ data_in
        ldr     r5, [r0, #4]          @ ctrl
        ldr     r6, [r0, #8]          @ data_out

        mov     r7, r1                @ copiar pixel para r7
        mov     r8, r2                @ copiar indice (0..783) para r8 ; veio do i do C em r2
        @ esse codigo de cima eh desnecessario so faz uma copia - nao sei se pode usar direto o registrador da funcao -- editar depois


        @ montando a palavra aqui:

        mov     r3, #0                @ opcode STORE_IMG nos [2:0]

        mov     r12, r7               @ campo pixel (valor 8-bit no fundo dos 32 bits de r12)
        lsl     r12, r12, #13         @ coloca esse valor na zona dos bits [20:13] da palavra - ou seja, adiciona 13 zeros a direita desse registrador ficando com pixel + 13 zeros
        orr     r3, r3, r12           @ r3 ja tem pixel bem posicionado + zeros no resto 

        mov     r12, r8               @ (indice 0..783) antes do shift
        lsl     r12, r12, #3          @ indice vai para zona [12:3] =
        orr     r3, r3, r12           @ une pixel + endereco na mesma palavra sem pisar um ao outro

        str     r3, [r4]

        mov     r12, #1
        str     r12, [r5]
        mov     r12, #0
        str     r12, [r5]

.Lpoll:
        ldr     r12, [r6]
        tst     r12, #MASK_ERROR
        bne     .Lhw_err
        tst     r12, #MASK_DONE
        beq     .Lpoll

        @ README Maik: clr_operation (bit 1 de ctrl) limpa done/error para o proximo passo
        mov     r12, #2
        str     r12, [r5]
        mov     r12, #0
        str     r12, [r5]

        mov     r0, #0
        pop     {r4-r8, pc}

.Lhw_err:
        mov     r0, #-2
        pop     {r4-r8, pc}

        .size   elm_store_img_pixel, .-elm_store_img_pixel

