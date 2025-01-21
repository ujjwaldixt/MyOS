ENTRY(loader)                  /* The entry point of our program is 'loader',
                                 which the linker will call the _start symbol. 
                                 This is where the CPU will jump when the bootloader
                                 transfers control to our kernel. */

OUTPUT_FORMAT(elf32-i386)     /* The binary is produced in the 32-bit ELF format. */
OUTPUT_ARCH(i386:i386)        /* The target architecture is 32-bit Intel 80386. */

SECTIONS
{
  . = 0x0100000;              /* Set the starting load address for the sections to 0x100000 
                                 (1 MB mark), common for kernel space when loaded by a 
                                 multiboot-compliant bootloader. */

  /* .text section:
   *  - Contains all executable code (.text*), the multiboot header (.multiboot),
   *    and any read-only data (.rodata).
   */
  .text :
  {
    *(.multiboot)             /* Place the multiboot header first so the bootloader can find it. */
    *(.text*)                 /* Include all text (code) sections from all object files. */
    *(.rodata)                /* Include read-only data (e.g. const strings). */
  }

  /* .data section:
   *  - Contains static/global variables that are initialized.
   *  - Also includes constructors through .init_array to handle C++ global initialization.
   */
  .data  :
  {
    start_ctors = .;          /* Label marking the beginning of the constructors list. */

    /* Keep the .init_array symbols so they are not stripped out. They contain pointers
     * to global constructors in C++ which we call at startup.
     */
    KEEP(*( .init_array ));
    KEEP(*(SORT_BY_INIT_PRIORITY( .init_array.* )));

    end_ctors = .;            /* Label marking the end of the constructors list. */

    *(.data)                  /* All normal .data sections (initialized variables). */
  }

  /* .bss section:
   *  - Contains uninitialized data (e.g. global variables set to 0). 
   *  - The linker zero-initializes this region at runtime.
   */
  .bss  :
  {
    *(.bss)
  }

  /* /DISCARD/ section:
   *  - Discards any sections we don't want to keep in the final binary.
   *  - Here, we discard the .fini_array (destructors) and .comment sections.
   *    We are not using .fini_array in this bare-metal OS scenario.
   */
  /DISCARD/ : { *(.fini_array*) *(.comment) }
}
