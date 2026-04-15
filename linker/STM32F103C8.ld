/**
 * @file  STM32F103C8.ld
 * @brief Linker script for STM32F103C8 (64 KB flash, 20 KB SRAM)
 *
 * Memory map:
 *   FLASH  0x08000000 – 0x0800FFFF  (64 KB)
 *   SRAM   0x20000000 – 0x20004FFF  (20 KB)
 *
 * Sections placed in order:
 *   .isr_vector  → interrupt vector table (must start at flash origin)
 *   .text        → executable code
 *   .rodata      → read-only constants
 *   .data        → initialised variables (loaded from flash, copied to RAM)
 *   .bss         → zero-initialised variables (in RAM)
 *   ._stack      → main stack pointer at top of RAM
 */

/* Entry point — Reset_Handler is defined in the startup file */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

/* Minimum stack and heap sizes — the linker will error if they don't fit */
_min_stack_size = 0x400;  /* 1 KB */
_min_heap_size  = 0x200;  /* 512 B */

SECTIONS
{
    /* --- Interrupt vector table ------------------------------------------ */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    /* --- Executable code ------------------------------------------------- */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)         /* ARM/Thumb interworking glue */
        *(.glue_7t)

        KEEP(*(.init))
        KEEP(*(.fini))

        . = ALIGN(4);
        _etext = .;
    } >FLASH

    /* --- Read-only data -------------------------------------------------- */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    /* --- ARM exception-handling tables (may be empty) -------------------- */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } >FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    /* --- Pre-init / init / fini arrays ----------------------------------- */
    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } >FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } >FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } >FLASH

    /* Flash-resident load address of .data */
    _sidata = LOADADDR(.data);

    /* --- Initialised data (copied from flash to SRAM at startup) --------- */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >SRAM AT>FLASH

    /* --- Zero-initialised data ------------------------------------------- */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >SRAM

    /* --- Heap (grows up) and stack (grows down) check -------------------- */
    ._heap :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _min_heap_size;
        . = ALIGN(8);
        _eheap = .;
    } >SRAM

    ._stack :
    {
        . = ALIGN(8);
        . = . + _min_stack_size;
        . = ALIGN(8);
    } >SRAM

    /* Top of stack = end of SRAM (stack grows downward) */
    _estack = ORIGIN(SRAM) + LENGTH(SRAM);

    /* Discard debug-related sections to keep the binary small */
    /DISCARD/ :
    {
        libc.a(*)
        libm.a(*)
        libgcc.a(*)
    }
}
