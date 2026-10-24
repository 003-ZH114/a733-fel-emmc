ENTRY(main)
SECTIONS
{
  . = 0x57000;
  .text : { *(.start) *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
  .ARM.attributes : { *(.ARM.attributes) }
}
