.set MAGIC, 0x1badb002                 # The Multiboot "magic number" required by Multiboot-compliant bootloaders
.set FLAGS, (1<<0 | 1<<1)              # Multiboot flags. Here we set:
                                       #   bit 0: align the modules on page boundaries
                                       #   bit 1: indicate we want information about memory layout
.set CHECKSUM, -(MAGIC + FLAGS)        # The checksum ensures (MAGIC + FLAGS + CHECKSUM) == 0 (32-bit wraparound).

##
# .multiboot section:
#   Contains the 12-byte Multiboot header that the bootloader reads to confirm
#   our kernel is indeed Multiboot-compliant. This header must appear early in the binary.
##
.section .multiboot
    .long MAGIC                        # 0x1badb002
    .long FLAGS                        # The flags set above
    .long CHECKSUM                     # Computed so that MAGIC + FLAGS + CHECKSUM == 0


##
# .text section:
#   The executable code for our loader. We import external functions from C/C++
#   (kernelMain and callConstructors) and define the "loader" entry point.
##
.section .text
.extern kernelMain
.extern callConstructors
.global loader


##
# loader:
#   The main entry point once the bootloader has loaded our kernel.
#   1) Set up the stack pointer (ESP) to 'kernel_stack' (defined below).
#   2) Call callConstructors (ensuring C++ global constructors are run).
#   3) Push %eax and %ebx (multiboot info and magic) onto the stack
#   4) Call kernelMain with those parameters
#   5) Halt in an infinite loop.
##
loader:
    mov $kernel_stack, %esp           # Initialize stack pointer to top of kernel stack
    call callConstructors             # Call global C++ constructors before kernelMain

    push %eax                         # Push 'multiboot_magic' onto the stack
    push %ebx                         # Push 'multiboot_structure' pointer onto the stack
    call kernelMain                   # Jump into the main C/C++ kernel function


##
# _stop:
#   Execution should never proceed here if the kernel runs indefinitely.
#   However, if kernelMain returns, we disable interrupts (cli) and halt the CPU.
#   Then we jump back to itself for an infinite loop.
##
_stop:
    cli                               # Clear interrupts
    hlt                               # Halt the CPU
    jmp _stop                         # Jump to itself (loop forever)


##
# .bss section:
#   Reserves memory for uninitialized data. 
#   Here, we allocate 2 MiB for our kernel stack and define the label kernel_stack
#   at the end of this reserved space.
##
.section .bss
.space 2*1024*1024                    # Reserve 2 MiB of uninitialized space
kernel_stack:
