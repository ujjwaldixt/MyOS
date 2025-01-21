#ifndef __MYOS__HARDWARECOMMUNICATION__INTERRUPTMANAGER_H    // Header guard to prevent multiple inclusions
#define __MYOS__HARDWARECOMMUNICATION__INTERRUPTMANAGER_H

#include <gdt.h>                                             // Global Descriptor Table definitions
#include <multitasking.h>                                    // TaskManager definitions
#include <common/types.h>                                    // Common type aliases (uint8_t, uint16_t, etc.)
#include <hardwarecommunication/port.h>                      // Definitions for Port I/O classes

namespace myos
{
    namespace hardwarecommunication
    {
        // Forward declaration of InterruptManager for use inside InterruptHandler.
        class InterruptManager;

        /*
         * InterruptHandler:
         *  Represents a generic handler for a particular interrupt (e.g., keyboard, mouse, etc.).
         *  Each derived class can override HandleInterrupt to provide custom handling logic.
         *  The interruptManager pointer lets the handler interact with the interrupt management system.
         */
        class InterruptHandler
        {
        protected:
            // The hardware interrupt number or IDT entry this handler will manage.
            myos::common::uint8_t InterruptNumber;

            // Reference back to the manager that controls overall interrupt functionality.
            InterruptManager* interruptManager;

            // Protected constructor called by derived classes to initialize handler associations.
            InterruptHandler(InterruptManager* interruptManager, myos::common::uint8_t InterruptNumber);

            // Protected destructor (interrupt handlers are typically freed when no longer needed).
            ~InterruptHandler();

        public:
            /*
             * HandleInterrupt:
             *   - 'esp' is the current stack pointer when the interrupt occurred.
             *   - Typically, we return the (potentially modified) stack pointer after handling.
             *   - Derived classes override this to implement device-specific interrupt logic.
             */
            virtual myos::common::uint32_t HandleInterrupt(myos::common::uint32_t esp);
        };


        /*
         * InterruptManager:
         *  Responsible for setting up the Interrupt Descriptor Table (IDT),
         *  remapping the programmable interrupt controllers (PICs),
         *  and managing the dispatch of hardware/software interrupts to appropriate handlers.
         *
         *  It also integrates with the TaskManager for context switching if needed.
         */
        class InterruptManager
        {
            // Allows the InterruptHandler class to access the private/protected members of InterruptManager.
            friend class InterruptHandler;

        protected:
            // Pointer to the currently active (enabled) InterruptManager. Used for global interrupt dispatch.
            static InterruptManager* ActiveInterruptManager;
            
            // An array of pointers to interrupt handlers, one for each possible interrupt (0..255).
            InterruptHandler* handlers[256];

            // Reference to the task manager, used for scheduling if an interrupt triggers task switching.
            TaskManager *taskManager;

            /*
             * GateDescriptor:
             *  Represents a single entry in the IDT (Interrupt Descriptor Table).
             *  Contains the handler function’s address, segment selector, and access flags.
             *  The 'packed' attribute prevents the compiler from adding padding bytes.
             */
            struct GateDescriptor
            {
                myos::common::uint16_t handlerAddressLowBits;     // Low 16 bits of the interrupt service routine address
                myos::common::uint16_t gdt_codeSegmentSelector;    // GDT segment selector for code (e.g., kernel code segment)
                myos::common::uint8_t  reserved;                   // Reserved (must be zero or set by system)
                myos::common::uint8_t  access;                     // Access flags (e.g., present bit, DPL, type)
                myos::common::uint16_t handlerAddressHighBits;     // High 16 bits of the ISR address
            } __attribute__((packed));
            
            // The actual IDT with 256 entries, one for each interrupt vector.
            static GateDescriptor interruptDescriptorTable[256];

            /*
             * InterruptDescriptorTablePointer:
             *  This structure is passed to the lidt (Load Interrupt Descriptor Table) instruction.
             *  It tells the CPU where the IDT is located and its size.
             */
            struct InterruptDescriptorTablePointer
            {
                myos::common::uint16_t size;    // Size of the IDT (in bytes) - 1
                myos::common::uint32_t base;    // Base address of the IDT
            } __attribute__((packed));

            // The offset where hardware interrupts start in the IDT (after remapping).
            myos::common::uint16_t hardwareInterruptOffset;

            /*
             * SetInterruptDescriptorTableEntry:
             *  Helper function to configure one entry in the IDT.
             *  Parameters define which interrupt vector, which code segment selector, the ISR,
             *  privilege level, and descriptor type (e.g., interrupt gate).
             */
            static void SetInterruptDescriptorTableEntry(myos::common::uint8_t interrupt,
                myos::common::uint16_t codeSegmentSelectorOffset, void (*handler)(),
                myos::common::uint8_t DescriptorPrivilegeLevel, myos::common::uint8_t DescriptorType);

            // A default handler that ignores interrupts (used for unhandled vectors).
            static void InterruptIgnore();

            // Various static handlers for hardware interrupts (IRQs) 0..15 and special ones like 0x31, 0x80.
            // Each corresponds to a hardware line on the PIC or a software interrupt.
            static void HandleInterruptRequest0x00();
            static void HandleInterruptRequest0x01();
            static void HandleInterruptRequest0x02();
            static void HandleInterruptRequest0x03();
            static void HandleInterruptRequest0x04();
            static void HandleInterruptRequest0x05();
            static void HandleInterruptRequest0x06();
            static void HandleInterruptRequest0x07();
            static void HandleInterruptRequest0x08();
            static void HandleInterruptRequest0x09();
            static void HandleInterruptRequest0x0A();
            static void HandleInterruptRequest0x0B();
            static void HandleInterruptRequest0x0C();
            static void HandleInterruptRequest0x0D();
            static void HandleInterruptRequest0x0E();
            static void HandleInterruptRequest0x0F();
            static void HandleInterruptRequest0x31();

            // Software interrupt for system calls (int 0x80).
            static void HandleInterruptRequest0x80();

            // Handlers for CPU exceptions/faults/traps (vectors 0..31). 
            // Each one corresponds to a specific processor exception (e.g., divide by zero, invalid opcode, page fault, etc.).
            static void HandleException0x00();
            static void HandleException0x01();
            static void HandleException0x02();
            static void HandleException0x03();
            static void HandleException0x04();
            static void HandleException0x05();
            static void HandleException0x06();
            static void HandleException0x07();
            static void HandleException0x08();
            static void HandleException0x09();
            static void HandleException0x0A();
            static void HandleException0x0B();
            static void HandleException0x0C();
            static void HandleException0x0D();
            static void HandleException0x0E();
            static void HandleException0x0F();
            static void HandleException0x10();
            static void HandleException0x11();
            static void HandleException0x12();
            static void HandleException0x13();

            /*
             * HandleInterrupt:
             *  Low-level static function called by assembly stubs to handle an interrupt.
             *  The 'interrupt' parameter is the IDT vector that fired.
             *  'esp' is the stack pointer at the time. Returns a possibly new stack pointer.
             */
            static myos::common::uint32_t HandleInterrupt(myos::common::uint8_t interrupt, myos::common::uint32_t esp);

            /*
             * DoHandleInterrupt:
             *  Called by HandleInterrupt once it identifies the correct InterruptManager (ActiveInterruptManager).
             *  Looks up the registered handler (if any) and calls its HandleInterrupt method.
             */
            myos::common::uint32_t DoHandleInterrupt(myos::common::uint8_t interrupt, myos::common::uint32_t esp);

            // Ports for communicating with the Programmable Interrupt Controller (PIC).
            Port8BitSlow programmableInterruptControllerMasterCommandPort;
            Port8BitSlow programmableInterruptControllerMasterDataPort;
            Port8BitSlow programmableInterruptControllerSlaveCommandPort;
            Port8BitSlow programmableInterruptControllerSlaveDataPort;

        public:
            /*
             * Constructor:
             *   - hardwareInterruptOffset: the index in the IDT where hardware interrupts (IRQs) should begin.
             *   - globalDescriptorTable: pointer to the system’s GDT, used for selector offsets.
             *   - taskManager: reference to handle context switching in response to interrupts (if needed).
             *  The constructor initializes the PIC, sets up the IDT, and configures all known ISR stubs.
             */
            InterruptManager(myos::common::uint16_t hardwareInterruptOffset, 
                             myos::GlobalDescriptorTable* globalDescriptorTable,
                             myos::TaskManager* taskManager);
            
            // Destructor: Disables interrupts and cleans up if necessary (often empty in low-level OS code).
            ~InterruptManager();

            // Returns the hardware interrupt offset (e.g., 0x20 for master PIC remap).
            myos::common::uint16_t HardwareInterruptOffset();

            /*
             * Activate:
             *  Loads the IDT (via lidt), enables interrupt handling, and sets this as the ActiveInterruptManager.
             */
            void Activate();

            /*
             * Deactivate:
             *  Disables this interrupt manager, typically by clearing ActiveInterruptManager 
             *  and possibly disabling interrupts if no other manager is active.
             */
            void Deactivate();
        };
        
    }
}

#endif // __MYOS__HARDWARECOMMUNICATION__INTERRUPTMANAGER_H
