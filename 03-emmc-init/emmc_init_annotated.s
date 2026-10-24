/* 带注释的 emmc_init 汇编（由 arm-none-eabi-objdump -d -O0 生成）
 * 机器码与 bin/fel.bin 的 -O2 版本不同；这里用于逐条说明初始化流程。
 * 地址基于链接地址 0x57000。 */


fel_o0_attr.elf:     file format elf32-littlearm


Disassembly of section .text:

00057894 <emmc_init>:

/* ================= emmc_init: A733 eMMC 初始化 ================= */
   57894:	e92d4800 	push	{fp, lr}
   57898:	e28db004 	add	fp, sp, #4
   5789c:	e24dd058 	sub	sp, sp, #88	@ 0x58
   578a0:	e50b0058 	str	r0, [fp, #-88]	@ 0xffffffa8
   578a4:	f57ff04f 	dsb	sy

/* --- 1. PIO V3：PC0-PC7 复用为 sdc2 (mux=3) ---------------------
 * 地址 0x02000180 = 0xf33fff33；PC2/3/4/7 未用写 0xf
 */
   578a8:	e3a03d06 	mov	r3, #384	@ 0x180
   578ac:	e3403200 	movt	r3, #512	@ 0x200
   578b0:	e30f2f33 	movw	r2, #65331	@ 0xff33
   578b4:	e34f233f 	movt	r2, #62271	@ 0xf33f
   578b8:	e5832000 	str	r2, [r3]
   578bc:	f57ff04f 	dsb	sy

/* 1. PIO：PC8-PC15 (0x02000184) = 0x333f3333 (PC12 未用=0xf) */
   578c0:	e3a03f61 	mov	r3, #388	@ 0x184
   578c4:	e3403200 	movt	r3, #512	@ 0x200
   578c8:	e3032333 	movw	r2, #13107	@ 0x3333
   578cc:	e343233f 	movt	r2, #13119	@ 0x333f
   578d0:	e5832000 	str	r2, [r3]
   578d4:	f57ff04f 	dsb	sy

/* 1. PIO：PC16 (0x02000188) = 3 */
   578d8:	e3a03f62 	mov	r3, #392	@ 0x188
   578dc:	e3403200 	movt	r3, #512	@ 0x200
   578e0:	e3a02003 	mov	r2, #3
   578e4:	e5832000 	str	r2, [r3]
   578e8:	f57ff04f 	dsb	sy

/* 1. PIO DRV0 (0x020001a0)：PC0-PC7 驱动强度 0x11111111 */
   578ec:	e3a03e1a 	mov	r3, #416	@ 0x1a0
   578f0:	e3403200 	movt	r3, #512	@ 0x200
   578f4:	e3012111 	movw	r2, #4369	@ 0x1111
   578f8:	e3412111 	movt	r2, #4369	@ 0x1111
   578fc:	e5832000 	str	r2, [r3]
   57900:	f57ff04f 	dsb	sy

/* 1. PIO DRV1 (0x020001a4)：PC8-PC15 */
   57904:	e3a03f69 	mov	r3, #420	@ 0x1a4
   57908:	e3403200 	movt	r3, #512	@ 0x200
   5790c:	e3012111 	movw	r2, #4369	@ 0x1111
   57910:	e3412111 	movt	r2, #4369	@ 0x1111
   57914:	e5832000 	str	r2, [r3]
   57918:	f57ff04f 	dsb	sy

/* 1. PIO DRV2 (0x020001a8)：PC16-PC23 */
   5791c:	e3a03f6a 	mov	r3, #424	@ 0x1a8
   57920:	e3403200 	movt	r3, #512	@ 0x200
   57924:	e3012111 	movw	r2, #4369	@ 0x1111
   57928:	e3412111 	movt	r2, #4369	@ 0x1111
   5792c:	e5832000 	str	r2, [r3]
   57930:	f57ff04f 	dsb	sy

/* 1. PIO PULL (0x020001b0)：PC bank 上下拉 = 0x54555946 */
   57934:	e3a03e1b 	mov	r3, #432	@ 0x1b0
   57938:	e3403200 	movt	r3, #512	@ 0x200
   5793c:	e3052946 	movw	r2, #22854	@ 0x5946
   57940:	e3452455 	movt	r2, #21589	@ 0x5455
   57944:	e5832000 	str	r2, [r3]
   57948:	f57ff04f 	dsb	sy

/* --- 2. CCU SMHC2 时钟 (0x02002d20) = 0x80000000 ----------------
 * bit31=gate on；mux[26:24]=0 (sys24M)；M/P 分频=0 -> 24MHz
 */
   5794c:	e3023d20 	movw	r3, #11552	@ 0x2d20
   57950:	e3403200 	movt	r3, #512	@ 0x200
   57954:	e3a02102 	mov	r2, #-2147483648	@ 0x80000000
   57958:	e5832000 	str	r2, [r3]
   5795c:	f57ff04f 	dsb	sy

/* --- 3. CCU SMHC2 门控/复位 (0x02002d2c) -----------------------
 * bit16=reset，bit0=gate；先清 reset
 */
   57960:	e3023d2c 	movw	r3, #11564	@ 0x2d2c
   57964:	e3403200 	movt	r3, #512	@ 0x200
   57968:	e5933000 	ldr	r3, [r3]
   5796c:	e50b3010 	str	r3, [fp, #-16]
   57970:	f57ff05f 	dmb	sy
   57974:	e51b2010 	ldr	r2, [fp, #-16]
   57978:	e3023d2c 	movw	r3, #11564	@ 0x2d2c
   5797c:	e3403200 	movt	r3, #512	@ 0x200
   57980:	e3c22801 	bic	r2, r2, #65536	@ 0x10000
   57984:	e5832000 	str	r2, [r3]
   57988:	f57ff04f 	dsb	sy

/* 3. 清 bit0：gate off */
   5798c:	e3023d2c 	movw	r3, #11564	@ 0x2d2c
   57990:	e3403200 	movt	r3, #512	@ 0x200
   57994:	e5933000 	ldr	r3, [r3]
   57998:	e50b3014 	str	r3, [fp, #-20]	@ 0xffffffec
   5799c:	f57ff05f 	dmb	sy
   579a0:	e51b2014 	ldr	r2, [fp, #-20]	@ 0xffffffec
   579a4:	e3023d2c 	movw	r3, #11564	@ 0x2d2c
   579a8:	e3403200 	movt	r3, #512	@ 0x200
   579ac:	e3c22001 	bic	r2, r2, #1
   579b0:	e5832000 	str	r2, [r3]

/* 3. udelay(10) */
   579b4:	e3a0000a 	mov	r0, #10
   579b8:	ebfffd98 	bl	57020 <udelay>
   579bc:	f57ff04f 	dsb	sy

/* 3. 置 bit0：gate on */
   579c0:	e3023d2c 	movw	r3, #11564	@ 0x2d2c
   579c4:	e3403200 	movt	r3, #512	@ 0x200
   579c8:	e5933000 	ldr	r3, [r3]
   579cc:	e50b3018 	str	r3, [fp, #-24]	@ 0xffffffe8
   579d0:	f57ff05f 	dmb	sy
   579d4:	e51b2018 	ldr	r2, [fp, #-24]	@ 0xffffffe8
   579d8:	e3023d2c 	movw	r3, #11564	@ 0x2d2c
   579dc:	e3403200 	movt	r3, #512	@ 0x200
   579e0:	e3822001 	orr	r2, r2, #1
   579e4:	e5832000 	str	r2, [r3]
   579e8:	f57ff04f 	dsb	sy

/* 3. 置 bit16：deassert reset */
   579ec:	e3023d2c 	movw	r3, #11564	@ 0x2d2c
   579f0:	e3403200 	movt	r3, #512	@ 0x200
   579f4:	e5933000 	ldr	r3, [r3]
   579f8:	e50b301c 	str	r3, [fp, #-28]	@ 0xffffffe4
   579fc:	f57ff05f 	dmb	sy
   57a00:	e51b201c 	ldr	r2, [fp, #-28]	@ 0xffffffe4
   57a04:	e3023d2c 	movw	r3, #11564	@ 0x2d2c
   57a08:	e3403200 	movt	r3, #512	@ 0x200
   57a0c:	e3822801 	orr	r2, r2, #65536	@ 0x10000
   57a10:	e5832000 	str	r2, [r3]

/* 3. udelay(100) */
   57a14:	e3a00064 	mov	r0, #100	@ 0x64
   57a18:	ebfffd80 	bl	57020 <udelay>

/* --- 4. SMHC 控制器软复位 (GCTRL=0x04022000) -------------------
 * 写 7：bit0 软复位 + bit1 FIFO + bit2 DMA
 */
   57a1c:	f57ff04f 	dsb	sy
   57a20:	e3a03a02 	mov	r3, #8192	@ 0x2000
   57a24:	e3403402 	movt	r3, #1026	@ 0x402
   57a28:	e3a02007 	mov	r2, #7
   57a2c:	e5832000 	str	r2, [r3]
   57a30:	e3a03000 	mov	r3, #0
   57a34:	e50b3008 	str	r3, [fp, #-8]
   57a38:	ea00000b 	b	57a6c <emmc_init+0x1d8>

/* 4. 轮询 GCTRL，等 bit2:0 清零 */
   57a3c:	e3a03a02 	mov	r3, #8192	@ 0x2000
   57a40:	e3403402 	movt	r3, #1026	@ 0x402
   57a44:	e5933000 	ldr	r3, [r3]
   57a48:	e50b3020 	str	r3, [fp, #-32]	@ 0xffffffe0
   57a4c:	f57ff05f 	dmb	sy
   57a50:	e51b3020 	ldr	r3, [fp, #-32]	@ 0xffffffe0
   57a54:	e2033007 	and	r3, r3, #7
   57a58:	e3530000 	cmp	r3, #0
   57a5c:	0a000008 	beq	57a84 <emmc_init+0x1f0>
   57a60:	e51b3008 	ldr	r3, [fp, #-8]
   57a64:	e2833001 	add	r3, r3, #1
   57a68:	e50b3008 	str	r3, [fp, #-8]
   57a6c:	e51b2008 	ldr	r2, [fp, #-8]
   57a70:	e308369f 	movw	r3, #34463	@ 0x869f
   57a74:	e3403001 	movt	r3, #1
   57a78:	e1520003 	cmp	r2, r3
   57a7c:	9affffee 	bls	57a3c <emmc_init+0x1a8>
   57a80:	ea000000 	b	57a88 <emmc_init+0x1f4>
   57a84:	e320f000 	nop	{0}

/* 4. SMHC_WIDTH (0x0402200c) = 0：强制 1-bit ---------------------
 * BROM 可能留在 8-bit；软复位不会清 WIDTH
 */
   57a88:	f57ff04f 	dsb	sy
   57a8c:	e302300c 	movw	r3, #8204	@ 0x200c
   57a90:	e3403402 	movt	r3, #1026	@ 0x402
   57a94:	e3a02000 	mov	r2, #0
   57a98:	e5832000 	str	r2, [r3]

/* 4. S(0x54)=3：csdc，CRC status detect */
   57a9c:	f57ff04f 	dsb	sy
   57aa0:	e3023054 	movw	r3, #8276	@ 0x2054
   57aa4:	e3403402 	movt	r3, #1026	@ 0x402
   57aa8:	e3a02003 	mov	r2, #3
   57aac:	e5832000 	str	r2, [r3]
   57ab0:	f57ff04f 	dsb	sy

/* 4. S(0x50)=0xdeb：dbgc 调试使能 */
   57ab4:	e3023050 	movw	r3, #8272	@ 0x2050
   57ab8:	e3403402 	movt	r3, #1026	@ 0x402
   57abc:	e3002deb 	movw	r2, #3563	@ 0xdeb
   57ac0:	e5832000 	str	r2, [r3]
   57ac4:	f57ff04f 	dsb	sy

/* 4. S(0x100)=0x00020005：thldc，卡阈值 512B */
   57ac8:	e3a03c21 	mov	r3, #8448	@ 0x2100
   57acc:	e3403402 	movt	r3, #1026	@ 0x402
   57ad0:	e3a02005 	mov	r2, #5
   57ad4:	e3402200 	movt	r2, #512	@ 0x200
   57ad8:	e5832000 	str	r2, [r3]
   57adc:	f57ff04f 	dsb	sy

/* 4. S(0x144)=0x80：samp_dl，采样延迟链使能 */
   57ae0:	e3023144 	movw	r3, #8516	@ 0x2144
   57ae4:	e3403402 	movt	r3, #1026	@ 0x402
   57ae8:	e3a02080 	mov	r2, #128	@ 0x80
   57aec:	e5832000 	str	r2, [r3]
   57af0:	f57ff04f 	dsb	sy

/* --- 5. 初始时钟 400kHz (CLKCR=0x04022004) ---------------------
 * 0x8000001d：div=29 + MASK_D0；24MHz/(2*(29+1))=400kHz
 */
   57af4:	e3023004 	movw	r3, #8196	@ 0x2004
   57af8:	e3403402 	movt	r3, #1026	@ 0x402
   57afc:	e3a02176 	mov	r2, #-2147483619	@ 0x8000001d
   57b00:	e5832000 	str	r2, [r3]

/* 5. 更新时钟 */
   57b04:	ebfffe03 	bl	57318 <smhc_update_clk>
   57b08:	e1a03000 	mov	r3, r0
   57b0c:	e3530000 	cmp	r3, #0
   57b10:	0a000001 	beq	57b1c <emmc_init+0x288>
   57b14:	e3e03000 	mvn	r3, #0
   57b18:	ea000127 	b	57fbc <emmc_init+0x728>

/* 5. +CLK_EN：0x8001001d */
   57b1c:	f57ff04f 	dsb	sy
   57b20:	e3023004 	movw	r3, #8196	@ 0x2004
   57b24:	e3403402 	movt	r3, #1026	@ 0x402
   57b28:	e3a0201d 	mov	r2, #29
   57b2c:	e3482001 	movt	r2, #32769	@ 0x8001
   57b30:	e5832000 	str	r2, [r3]

/* 5. 更新时钟 */
   57b34:	ebfffdf7 	bl	57318 <smhc_update_clk>
   57b38:	e1a03000 	mov	r3, r0
   57b3c:	e3530000 	cmp	r3, #0
   57b40:	0a000001 	beq	57b4c <emmc_init+0x2b8>
   57b44:	e3e03000 	mvn	r3, #0
   57b48:	ea00011b 	b	57fbc <emmc_init+0x728>

/* 5. 清 MASK_D0 */
   57b4c:	f57ff04f 	dsb	sy
   57b50:	e3023004 	movw	r3, #8196	@ 0x2004
   57b54:	e3403402 	movt	r3, #1026	@ 0x402
   57b58:	e5933000 	ldr	r3, [r3]
   57b5c:	e50b3024 	str	r3, [fp, #-36]	@ 0xffffffdc
   57b60:	f57ff05f 	dmb	sy
   57b64:	e51b2024 	ldr	r2, [fp, #-36]	@ 0xffffffdc
   57b68:	e3023004 	movw	r3, #8196	@ 0x2004
   57b6c:	e3403402 	movt	r3, #1026	@ 0x402
   57b70:	e3c22102 	bic	r2, r2, #-2147483648	@ 0x80000000
   57b74:	e5832000 	str	r2, [r3]

/* 5. 更新时钟 */
   57b78:	ebfffde6 	bl	57318 <smhc_update_clk>
   57b7c:	e1a03000 	mov	r3, r0
   57b80:	e3530000 	cmp	r3, #0
   57b84:	0a000001 	beq	57b90 <emmc_init+0x2fc>
   57b88:	e3e03000 	mvn	r3, #0
   57b8c:	ea00010a 	b	57fbc <emmc_init+0x728>

/* --- 6. 超时 / 中断 ------------------------------------------- */
   57b90:	f57ff04f 	dsb	sy
   57b94:	e3023008 	movw	r3, #8200	@ 0x2008
   57b98:	e3403402 	movt	r3, #1026	@ 0x402
   57b9c:	e3e02000 	mvn	r2, #0
   57ba0:	e5832000 	str	r2, [r3]
   57ba4:	f57ff04f 	dsb	sy

/* 6. IMASK=0：关闭中断，使用轮询 */
   57ba8:	e3023030 	movw	r3, #8240	@ 0x2030
   57bac:	e3403402 	movt	r3, #1026	@ 0x402
   57bb0:	e3a02000 	mov	r2, #0
   57bb4:	e5832000 	str	r2, [r3]
   57bb8:	f57ff04f 	dsb	sy

/* --- 7. 硬复位 + 80 个初始化时钟 ------------------------------- */
   57bbc:	e3023078 	movw	r3, #8312	@ 0x2078
   57bc0:	e3403402 	movt	r3, #1026	@ 0x402
   57bc4:	e3a02000 	mov	r2, #0
   57bc8:	e5832000 	str	r2, [r3]
   57bcc:	e3a00ffa 	mov	r0, #1000	@ 0x3e8
   57bd0:	ebfffd12 	bl	57020 <udelay>

/* 7. udelay(1000) */
   57bd4:	f57ff04f 	dsb	sy

/* 7. HWRST=1 释放复位 */
   57bd8:	e3023078 	movw	r3, #8312	@ 0x2078
   57bdc:	e3403402 	movt	r3, #1026	@ 0x402
   57be0:	e3a02001 	mov	r2, #1
   57be4:	e5832000 	str	r2, [r3]
   57be8:	e3a00ffa 	mov	r0, #1000	@ 0x3e8
   57bec:	ebfffd0b 	bl	57020 <udelay>

/* 7. SEND_INIT_SEQ：CMD=0x80008000 (START|SEND_INIT_SEQ) */
   57bf0:	f57ff04f 	dsb	sy
   57bf4:	e3023018 	movw	r3, #8216	@ 0x2018
   57bf8:	e3403402 	movt	r3, #1026	@ 0x402
   57bfc:	e3a02902 	mov	r2, #32768	@ 0x8000
   57c00:	e3482000 	movt	r2, #32768	@ 0x8000
   57c04:	e5832000 	str	r2, [r3]
   57c08:	e3a03000 	mov	r3, #0
   57c0c:	e50b3008 	str	r3, [fp, #-8]
   57c10:	ea00000a 	b	57c40 <emmc_init+0x3ac>

/* 7. 轮询 CMD.START 清零 */
   57c14:	e3023018 	movw	r3, #8216	@ 0x2018
   57c18:	e3403402 	movt	r3, #1026	@ 0x402
   57c1c:	e5933000 	ldr	r3, [r3]
   57c20:	e50b3028 	str	r3, [fp, #-40]	@ 0xffffffd8
   57c24:	f57ff05f 	dmb	sy
   57c28:	e51b3028 	ldr	r3, [fp, #-40]	@ 0xffffffd8
   57c2c:	e3530000 	cmp	r3, #0
   57c30:	aa000008 	bge	57c58 <emmc_init+0x3c4>
   57c34:	e51b3008 	ldr	r3, [fp, #-8]
   57c38:	e2833001 	add	r3, r3, #1
   57c3c:	e50b3008 	str	r3, [fp, #-8]
   57c40:	e51b2008 	ldr	r2, [fp, #-8]
   57c44:	e308369f 	movw	r3, #34463	@ 0x869f
   57c48:	e3403001 	movt	r3, #1
   57c4c:	e1520003 	cmp	r2, r3
   57c50:	9affffef 	bls	57c14 <emmc_init+0x380>
   57c54:	ea000000 	b	57c5c <emmc_init+0x3c8>
   57c58:	e320f000 	nop	{0}

/* 7. udelay(1000) */
   57c5c:	e3a00ffa 	mov	r0, #1000	@ 0x3e8
   57c60:	ebfffcee 	bl	57020 <udelay>

/* --- 8. 卡片初始化 -------------------------------------------- */
   57c64:	e3a00001 	mov	r0, #1
   57c68:	ebfffd01 	bl	57074 <set_state>

/* 8. CMD0: GO_IDLE_STATE */
   57c6c:	e3a03000 	mov	r3, #0
   57c70:	e3a02902 	mov	r2, #32768	@ 0x8000
   57c74:	e3a01000 	mov	r1, #0
   57c78:	e3a00000 	mov	r0, #0
   57c7c:	ebfffd3b 	bl	57170 <smhc_cmd>
   57c80:	e3a03000 	mov	r3, #0
   57c84:	e50b3008 	str	r3, [fp, #-8]
   57c88:	ea000010 	b	57cd0 <emmc_init+0x43c>

/* 8. CMD1: SEND_OP_COND，轮询 OCR.BUSY 最多 1000 次 */
   57c8c:	e24b3050 	sub	r3, fp, #80	@ 0x50
   57c90:	e3a02040 	mov	r2, #64	@ 0x40
   57c94:	e3081080 	movw	r1, #32896	@ 0x8080
   57c98:	e34410ff 	movt	r1, #16639	@ 0x40ff
   57c9c:	e3a00001 	mov	r0, #1
   57ca0:	ebfffd32 	bl	57170 <smhc_cmd>
   57ca4:	e1a03000 	mov	r3, r0
   57ca8:	e3530000 	cmp	r3, #0
   57cac:	1a000002 	bne	57cbc <emmc_init+0x428>
   57cb0:	e51b3050 	ldr	r3, [fp, #-80]	@ 0xffffffb0
   57cb4:	e3530000 	cmp	r3, #0
   57cb8:	ba000008 	blt	57ce0 <emmc_init+0x44c>
   57cbc:	e3a00ffa 	mov	r0, #1000	@ 0x3e8
   57cc0:	ebfffcd6 	bl	57020 <udelay>
   57cc4:	e51b3008 	ldr	r3, [fp, #-8]
   57cc8:	e2833001 	add	r3, r3, #1
   57ccc:	e50b3008 	str	r3, [fp, #-8]
   57cd0:	e51b3008 	ldr	r3, [fp, #-8]
   57cd4:	e3530ffa 	cmp	r3, #1000	@ 0x3e8
   57cd8:	3affffeb 	bcc	57c8c <emmc_init+0x3f8>
   57cdc:	ea000000 	b	57ce4 <emmc_init+0x450>
   57ce0:	e320f000 	nop	{0}
   57ce4:	e51b3008 	ldr	r3, [fp, #-8]
   57ce8:	e3530ffa 	cmp	r3, #1000	@ 0x3e8
   57cec:	3a000001 	bcc	57cf8 <emmc_init+0x464>
   57cf0:	e3e03001 	mvn	r3, #1
   57cf4:	ea0000b0 	b	57fbc <emmc_init+0x728>

/* 8. CMD2: ALL_SEND_CID，随后读 RESP0-3 得到 CID */
   57cf8:	e3a03000 	mov	r3, #0
   57cfc:	e3a02d07 	mov	r2, #448	@ 0x1c0
   57d00:	e3a01000 	mov	r1, #0
   57d04:	e3a00002 	mov	r0, #2
   57d08:	ebfffd18 	bl	57170 <smhc_cmd>
   57d0c:	e51b3058 	ldr	r3, [fp, #-88]	@ 0xffffffa8
   57d10:	e3530000 	cmp	r3, #0
   57d14:	0a000022 	beq	57da4 <emmc_init+0x510>
   57d18:	e3023020 	movw	r3, #8224	@ 0x2020
   57d1c:	e3403402 	movt	r3, #1026	@ 0x402
   57d20:	e5933000 	ldr	r3, [r3]
   57d24:	e50b302c 	str	r3, [fp, #-44]	@ 0xffffffd4
   57d28:	f57ff05f 	dmb	sy
   57d2c:	e51b202c 	ldr	r2, [fp, #-44]	@ 0xffffffd4
   57d30:	e51b3058 	ldr	r3, [fp, #-88]	@ 0xffffffa8
   57d34:	e5832000 	str	r2, [r3]
   57d38:	e3023024 	movw	r3, #8228	@ 0x2024
   57d3c:	e3403402 	movt	r3, #1026	@ 0x402
   57d40:	e5933000 	ldr	r3, [r3]
   57d44:	e50b3030 	str	r3, [fp, #-48]	@ 0xffffffd0
   57d48:	f57ff05f 	dmb	sy
   57d4c:	e51b2030 	ldr	r2, [fp, #-48]	@ 0xffffffd0
   57d50:	e51b3058 	ldr	r3, [fp, #-88]	@ 0xffffffa8
   57d54:	e2833004 	add	r3, r3, #4
   57d58:	e5832000 	str	r2, [r3]
   57d5c:	e3023028 	movw	r3, #8232	@ 0x2028
   57d60:	e3403402 	movt	r3, #1026	@ 0x402
   57d64:	e5933000 	ldr	r3, [r3]
   57d68:	e50b3034 	str	r3, [fp, #-52]	@ 0xffffffcc
   57d6c:	f57ff05f 	dmb	sy
   57d70:	e51b2034 	ldr	r2, [fp, #-52]	@ 0xffffffcc
   57d74:	e51b3058 	ldr	r3, [fp, #-88]	@ 0xffffffa8
   57d78:	e2833008 	add	r3, r3, #8
   57d7c:	e5832000 	str	r2, [r3]
   57d80:	e302302c 	movw	r3, #8236	@ 0x202c
   57d84:	e3403402 	movt	r3, #1026	@ 0x402
   57d88:	e5933000 	ldr	r3, [r3]
   57d8c:	e50b3038 	str	r3, [fp, #-56]	@ 0xffffffc8
   57d90:	f57ff05f 	dmb	sy
   57d94:	e51b2038 	ldr	r2, [fp, #-56]	@ 0xffffffc8
   57d98:	e51b3058 	ldr	r3, [fp, #-88]	@ 0xffffffa8
   57d9c:	e283300c 	add	r3, r3, #12
   57da0:	e5832000 	str	r2, [r3]

/* 8. CMD3: SET_RELATIVE_ADDR (RCA=1) */
   57da4:	e3a03000 	mov	r3, #0
   57da8:	e3a02d05 	mov	r2, #320	@ 0x140
   57dac:	e3a01801 	mov	r1, #65536	@ 0x10000
   57db0:	e3a00003 	mov	r0, #3
   57db4:	ebfffced 	bl	57170 <smhc_cmd>

/* 8. CMD7: SELECT_CARD */
   57db8:	e3a03000 	mov	r3, #0
   57dbc:	e3a02d05 	mov	r2, #320	@ 0x140
   57dc0:	e3a01801 	mov	r1, #65536	@ 0x10000
   57dc4:	e3a00007 	mov	r0, #7
   57dc8:	ebfffce8 	bl	57170 <smhc_cmd>
   57dcc:	e3a00002 	mov	r0, #2
   57dd0:	ebfffca7 	bl	57074 <set_state>

/* 8. CMD6: SWITCH，强制 1-bit SDR (0x03B70000) */
   57dd4:	e3a03000 	mov	r3, #0
   57dd8:	e3a02d05 	mov	r2, #320	@ 0x140
   57ddc:	e3a01000 	mov	r1, #0
   57de0:	e34013b7 	movt	r1, #951	@ 0x3b7
   57de4:	e3a00006 	mov	r0, #6
   57de8:	ebfffce0 	bl	57170 <smhc_cmd>
   57dec:	e3a03000 	mov	r3, #0
   57df0:	e50b3008 	str	r3, [fp, #-8]
   57df4:	ea00000b 	b	57e28 <emmc_init+0x594>
   57df8:	e302303c 	movw	r3, #8252	@ 0x203c
   57dfc:	e3403402 	movt	r3, #1026	@ 0x402
   57e00:	e5933000 	ldr	r3, [r3]
   57e04:	e50b303c 	str	r3, [fp, #-60]	@ 0xffffffc4
   57e08:	f57ff05f 	dmb	sy
   57e0c:	e51b303c 	ldr	r3, [fp, #-60]	@ 0xffffffc4
   57e10:	e2033c02 	and	r3, r3, #512	@ 0x200
   57e14:	e3530000 	cmp	r3, #0
   57e18:	0a000008 	beq	57e40 <emmc_init+0x5ac>
   57e1c:	e51b3008 	ldr	r3, [fp, #-8]
   57e20:	e2833001 	add	r3, r3, #1
   57e24:	e50b3008 	str	r3, [fp, #-8]
   57e28:	e51b2008 	ldr	r2, [fp, #-8]
   57e2c:	e30a311f 	movw	r3, #41247	@ 0xa11f
   57e30:	e3403007 	movt	r3, #7
   57e34:	e1520003 	cmp	r2, r3
   57e38:	9affffee 	bls	57df8 <emmc_init+0x564>
   57e3c:	ea000000 	b	57e44 <emmc_init+0x5b0>
   57e40:	e320f000 	nop	{0}
   57e44:	e3a00ffa 	mov	r0, #1000	@ 0x3e8
   57e48:	ebfffc74 	bl	57020 <udelay>

/* --- 9. 提速到 12MHz (div=0) ---------------------------------- */
   57e4c:	e3a03000 	mov	r3, #0
   57e50:	e50b300c 	str	r3, [fp, #-12]

/* 9. 先 MASK_D0 */
   57e54:	f57ff04f 	dsb	sy
   57e58:	e3023004 	movw	r3, #8196	@ 0x2004
   57e5c:	e3403402 	movt	r3, #1026	@ 0x402
   57e60:	e3a02102 	mov	r2, #-2147483648	@ 0x80000000
   57e64:	e5832000 	str	r2, [r3]
   57e68:	ebfffd2a 	bl	57318 <smhc_update_clk>
   57e6c:	e1a03000 	mov	r3, r0
   57e70:	e3530000 	cmp	r3, #0
   57e74:	1a000035 	bne	57f50 <emmc_init+0x6bc>

/* 9. div=0 + MASK_D0 */
   57e78:	f57ff04f 	dsb	sy
   57e7c:	e3023004 	movw	r3, #8196	@ 0x2004
   57e80:	e3403402 	movt	r3, #1026	@ 0x402
   57e84:	e3a02102 	mov	r2, #-2147483648	@ 0x80000000
   57e88:	e5832000 	str	r2, [r3]
   57e8c:	ebfffd21 	bl	57318 <smhc_update_clk>
   57e90:	e1a03000 	mov	r3, r0
   57e94:	e3530000 	cmp	r3, #0
   57e98:	1a00002c 	bne	57f50 <emmc_init+0x6bc>

/* 9. +CLK_EN */
   57e9c:	f57ff04f 	dsb	sy
   57ea0:	e3023004 	movw	r3, #8196	@ 0x2004
   57ea4:	e3403402 	movt	r3, #1026	@ 0x402
   57ea8:	e3a02000 	mov	r2, #0
   57eac:	e3482001 	movt	r2, #32769	@ 0x8001
   57eb0:	e5832000 	str	r2, [r3]
   57eb4:	ebfffd17 	bl	57318 <smhc_update_clk>
   57eb8:	e1a03000 	mov	r3, r0
   57ebc:	e3530000 	cmp	r3, #0
   57ec0:	1a000022 	bne	57f50 <emmc_init+0x6bc>

/* 9. 重新配置采样延迟 samp_dl */
   57ec4:	e3023144 	movw	r3, #8516	@ 0x2144
   57ec8:	e3403402 	movt	r3, #1026	@ 0x402
   57ecc:	e5933000 	ldr	r3, [r3]
   57ed0:	e50b3040 	str	r3, [fp, #-64]	@ 0xffffffc0
   57ed4:	f57ff05f 	dmb	sy
   57ed8:	e51b3040 	ldr	r3, [fp, #-64]	@ 0xffffffc0
   57edc:	e50b3044 	str	r3, [fp, #-68]	@ 0xffffffbc
   57ee0:	e51b3044 	ldr	r3, [fp, #-68]	@ 0xffffffbc
   57ee4:	e3c3303f 	bic	r3, r3, #63	@ 0x3f
   57ee8:	e50b3044 	str	r3, [fp, #-68]	@ 0xffffffbc
   57eec:	e51b3044 	ldr	r3, [fp, #-68]	@ 0xffffffbc
   57ef0:	e3833080 	orr	r3, r3, #128	@ 0x80
   57ef4:	e50b3044 	str	r3, [fp, #-68]	@ 0xffffffbc
   57ef8:	f57ff04f 	dsb	sy
   57efc:	e3023144 	movw	r3, #8516	@ 0x2144
   57f00:	e3403402 	movt	r3, #1026	@ 0x402
   57f04:	e51b2044 	ldr	r2, [fp, #-68]	@ 0xffffffbc
   57f08:	e5832000 	str	r2, [r3]
   57f0c:	f57ff04f 	dsb	sy
   57f10:	e3023004 	movw	r3, #8196	@ 0x2004
   57f14:	e3403402 	movt	r3, #1026	@ 0x402
   57f18:	e5933000 	ldr	r3, [r3]
   57f1c:	e50b3048 	str	r3, [fp, #-72]	@ 0xffffffb8
   57f20:	f57ff05f 	dmb	sy
   57f24:	e51b2048 	ldr	r2, [fp, #-72]	@ 0xffffffb8
   57f28:	e3023004 	movw	r3, #8196	@ 0x2004
   57f2c:	e3403402 	movt	r3, #1026	@ 0x402
   57f30:	e3c22102 	bic	r2, r2, #-2147483648	@ 0x80000000
   57f34:	e5832000 	str	r2, [r3]

/* 9. 成功：清 MASK_D0 */
   57f38:	ebfffcf6 	bl	57318 <smhc_update_clk>
   57f3c:	e1a03000 	mov	r3, r0
   57f40:	e3530000 	cmp	r3, #0
   57f44:	1a000001 	bne	57f50 <emmc_init+0x6bc>
   57f48:	e3a03001 	mov	r3, #1
   57f4c:	e50b300c 	str	r3, [fp, #-12]

/* 9. 失败：回退到 400kHz/1-bit */
   57f50:	e51b300c 	ldr	r3, [fp, #-12]
   57f54:	e3530000 	cmp	r3, #0
   57f58:	1a000016 	bne	57fb8 <emmc_init+0x724>
   57f5c:	f57ff04f 	dsb	sy
   57f60:	e302300c 	movw	r3, #8204	@ 0x200c
   57f64:	e3403402 	movt	r3, #1026	@ 0x402
   57f68:	e3a02000 	mov	r2, #0
   57f6c:	e5832000 	str	r2, [r3]
   57f70:	f57ff04f 	dsb	sy
   57f74:	e3023004 	movw	r3, #8196	@ 0x2004
   57f78:	e3403402 	movt	r3, #1026	@ 0x402
   57f7c:	e3a0201d 	mov	r2, #29
   57f80:	e5832000 	str	r2, [r3]
   57f84:	ebfffce3 	bl	57318 <smhc_update_clk>
   57f88:	f57ff04f 	dsb	sy
   57f8c:	e3023004 	movw	r3, #8196	@ 0x2004
   57f90:	e3403402 	movt	r3, #1026	@ 0x402
   57f94:	e5933000 	ldr	r3, [r3]
   57f98:	e50b304c 	str	r3, [fp, #-76]	@ 0xffffffb4
   57f9c:	f57ff05f 	dmb	sy
   57fa0:	e51b204c 	ldr	r2, [fp, #-76]	@ 0xffffffb4
   57fa4:	e3023004 	movw	r3, #8196	@ 0x2004
   57fa8:	e3403402 	movt	r3, #1026	@ 0x402
   57fac:	e3822801 	orr	r2, r2, #65536	@ 0x10000
   57fb0:	e5832000 	str	r2, [r3]
   57fb4:	ebfffcd7 	bl	57318 <smhc_update_clk>
   57fb8:	e3a03000 	mov	r3, #0
   57fbc:	e1a00003 	mov	r0, r3
   57fc0:	e24bd004 	sub	sp, fp, #4
   57fc4:	e8bd8800 	pop	{fp, pc}
