#include <syscalls.h>

/*
 * The namespaces:
 *   - myos: main OS namespace
 *   - myos::common: basic type definitions (uint8_t, uint32_t, etc.)
 *   - myos::hardwarecommunication: interrupt-related classes, port I/O, etc.
 */
using namespace myos;
using namespace myos::common;
using namespace myos::hardwarecommunication;


/*
 * SyscallHandler:
 *  A class to handle system calls (software interrupts) triggered by "int <InterruptNumber>".
 *  For example, user code might do "int 0x80" to invoke a system call.
 */

/*
 * Constructor:
 *   - interruptManager: pointer to the system’s interrupt manager
 *   - InterruptNumber: the base interrupt vector for syscalls, e.g., 0x80
 *
 * We pass 'InterruptNumber + interruptManager->HardwareInterruptOffset()'
 * to the InterruptHandler constructor because hardware interrupts and software
 * interrupts are often offset in the IDT. This ensures we register the correct
 * entry in the IDT for the syscall vector.
 */
SyscallHandler::SyscallHandler(InterruptManager* interruptManager, uint8_t InterruptNumber)
: InterruptHandler(interruptManager, InterruptNumber + interruptManager->HardwareInterruptOffset())
{
}

/*
 * Destructor: (currently empty)
 *  If resources needed to be released, that would happen here.
 */
SyscallHandler::~SyscallHandler()
{
}

/*
 * printf:
 *  Forward declaration of a basic kernel print function 
 *  (to write strings to the screen in text mode).
 */
extern void printf(char*);

/*
 * HandleInterrupt:
 *   - This method is called whenever a system call interrupt occurs (e.g., int 0x80).
 *   - 'esp' is the current stack pointer, which contains the CPUState snapshot.
 *   - We cast 'esp' to CPUState* to access registers (eax, ebx, etc.).
 *
 * The switch statement checks cpu->eax to identify which system call is requested.
 * For example:
 *   - case 4: treat it as a "print string" system call. The address of the string is in ebx.
 *
 * Returns the (potentially modified) stack pointer.
 */
uint32_t SyscallHandler::HandleInterrupt(uint32_t esp)
{
    // Interpret 'esp' as a pointer to the CPUState structure
    CPUState* cpu = (CPUState*)esp;
    
    // The system call number is placed in EAX by convention
    switch(cpu->eax)
    {
        case 4:
            // Syscall #4: print the string pointed to by EBX
            printf((char*)cpu->ebx);
            break;
            
        default:
            // Unhandled syscall number
            break;
    }

    // Return the stack pointer (unchanged in this basic example)
    return esp;
}
