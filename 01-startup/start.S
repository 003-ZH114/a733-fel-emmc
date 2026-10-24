/*
 * A733 (sun60iw2p1) FEL payload startup -- assembly version
 *
 * Entry point is main(), called directly by the BROM FEL "exec" command
 * at the link address 0x57000 (see a733.ld).
 *
 * CRITICAL: the BROM's stack pointer must be preserved.  Overwriting it
 * kills the FEL USB handler and requires a power cycle to recover.
 */

    .syntax unified
    .arm
    .section .start, "ax", %progbits
    .global main
    .type   main, %function

main:
    mov     r4, sp                  /* save BROM stack pointer            */
    ldr     sp, =0x000A0000         /* private SRAM work stack, grows down */
    push    {r4, lr}                /* save BROM SP + return address      */
    bl      work                    /* C entry: clock + eMMC init         */
    pop     {r4, lr}
    mov     sp, r4                  /* restore BROM stack pointer         */
    bx      lr                      /* return to BROM FEL                 */

    .size   main, . - main
    .ltorg
