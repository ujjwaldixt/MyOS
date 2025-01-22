.set IRQ_BASE, 0x20
# Define the base interrupt vector for hardware IRQs. Typically, the PIC is
# remapped so that IRQ0 (timer) is mapped to 0x20.

.section .text
# Begin the text (code) section.

.extern _ZN4myos21hardwarecommunication16InterruptManager15HandleInterruptEhj
# Declare an external symbol (C++ mangled name) for 
# myos::hardwarecommunication::InterruptManager::HandleInterrupt(uint8_t, uint32_t)
# This function will be called from the common interrupt bottom half.

# Macro: HandleException
# This macro creates an assembly function to handle a CPU exception.
# Each exception handler sets the "interruptnumber" byte to the given number
# and then jumps to a common "int_bottom" routine to do the actual handling.
.macro HandleException num
.global _ZN4myos21hardwarecommunication16InterruptManager19HandleException\num\()Ev
_ZN4myos21hardwarecommunication16InterruptManager19HandleException\num\()Ev:
    movb $\num, (interruptnumber)
    jmp int_bottom
.endm

# Macro: HandleInterruptRequest
# This macro creates a handler for a hardware interrupt (IRQ).
# It sets the "interruptnumber" byte to the IRQ number plus the IRQ_BASE offset,
# then pushes a 0 (dummy value for the error code parameter) onto the stack,
# and jumps to the shared int_bottom routine.
.macro HandleInterruptRequest num
.global _ZN4myos21hardwarecommunication16InterruptManager26HandleInterruptRequest\num\()Ev
_ZN4myos21hardwarecommunication16InterruptManager26HandleInterruptRequest\num\()Ev:
    movb $\num + IRQ_BASE, (interruptnumber)
    pushl $0
    jmp int_bottom
.endm

# Create exception handlers for exceptions 0x00 to 0x13.
HandleException 0x00
HandleException 0x01
HandleException 0x02
HandleException 0x03
HandleException 0x04
HandleException 0x05
HandleException 0x06
HandleException 0x07
HandleException 0x08
HandleException 0x09
HandleException 0x0A
HandleException 0x0B
HandleException 0x0C
HandleException 0x0D
HandleException 0x0E
HandleException 0x0F
HandleException 0x10
HandleException 0x11
HandleException 0x12
HandleException 0x13

# Create handlers for hardware interrupts (IRQs) for IRQs 0x00 to 0x0F.
HandleInterruptRequest 0x00
HandleInterruptRequest 0x01
HandleInterruptRequest 0x02
HandleInterruptRequest 0x03
HandleInterruptRequest 0x04
HandleInterruptRequest 0x05
HandleInterruptRequest 0x06
HandleInterruptRequest 0x07
HandleInterruptRequest 0x08
HandleInterruptRequest 0x09
HandleInterruptRequest 0x0A
HandleInterruptRequest 0x0B
HandleInterruptRequest 0x0C
HandleInterruptRequest 0x0D
HandleInterruptRequest 0x0E
HandleInterruptRequest 0x0F

# Create handler for a specific additional IRQ (vector 0x31).
HandleInterruptRequest 0x31

# Create handler for the syscall interrupt (vector 0x80).
HandleInterruptRequest 0x80

# The label "int_bottom" marks the common handler routine that is jumped to
# by both exception and IRQ macro-generated handlers.
int_bottom:

    # Save registers that need to be preserved across the call to the C++ handler.
    # (The code to save all registers using pusha is commented out, and here we save selected registers.)
    pushl %ebp
    pushl %edi
    pushl %esi

    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax

    # (Commented out: Loading of segment registers for ring0 if needed.)

    # Call the common C++ interrupt handling function.
    # We push the current stack pointer and the "interruptnumber" (which was set in the handler macro)
    # as arguments to the C++ function.
    pushl %esp                    # Push pointer to CPU state (stack pointer) for the C++ handler.
    push (interruptnumber)        # Push the interrupt number stored in memory at label 'interruptnumber'
    call _ZN4myos21hardwarecommunication16InterruptManager15HandleInterruptEhj
    # After the call, the C++ handler returns a new stack pointer in %eax.
    mov %eax, %esp                # Switch to the new stack (context switch if needed)

    # Restore registers in reverse order.
    popl %eax
    popl %ebx
    popl %ecx
    popl %edx

    popl %esi
    popl %edi
    popl %ebp

    # Clean up the parameter that was pushed before the call.
    add $4, %esp

.global _ZN4myos21hardwarecommunication16InterruptManager15InterruptIgnoreEv
_ZN4myos21hardwarecommunication16InterruptManager15InterruptIgnoreEv:
    iret
    # The InterruptIgnore function is a do-nothing handler. It simply returns
    # from the interrupt using the 'iret' instruction.

.section .data
    interruptnumber: .byte 0
    # This global data element holds the current interrupt number.
    # Each handler macro writes a specific value to this byte before calling int_bottom.
