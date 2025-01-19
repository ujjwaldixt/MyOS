#ifndef __MYOS__SYSCALLS_H
#define __MYOS__SYSCALLS_H

#include <common/types.h>
#include <hardwarecommunication/interrupts.h>
#include <multitasking.h>

namespace myos
{
    // The SyscallHandler class is responsible for handling system calls (syscalls) made by user programs.
    // Syscalls allow user-space programs to request services or resources from the operating system
    // in a controlled and secure manner. This class extends the InterruptHandler class to handle
    // system call interrupts specifically.
    class SyscallHandler : public hardwarecommunication::InterruptHandler
    {
    public:
        // Constructor: Registers the SyscallHandler with the InterruptManager.
        // - `interruptManager`: Pointer to the interrupt manager that handles system interrupts.
        // - `InterruptNumber`: The specific interrupt number reserved for system calls.
        // This ensures that system calls trigger this handler for processing.
        SyscallHandler(hardwarecommunication::InterruptManager* interruptManager, myos::common::uint8_t InterruptNumber);
        
        // Destructor: Cleans up resources used by the SyscallHandler.
        ~SyscallHandler();
        
        // Handles a system call interrupt.
        // - `esp`: The stack pointer of the interrupted process, which contains its context.
        // This method processes the syscall by reading parameters from the stack,
        // executing the requested system service, and returning the appropriate response.
        virtual myos::common::uint32_t HandleInterrupt(myos::common::uint32_t esp);
    };

}

#endif
