#include <hardwarecommunication/interrupts.h>

/*
 * Using namespaces for clarity:
 *   - myos: main OS namespace
 *   - myos::common: basic types and common utilities
 *   - myos::hardwarecommunication: classes related to hardware I/O, interrupts, etc.
 */
using namespace myos;
using namespace myos::common;
using namespace myos::hardwarecommunication;

// Forward declarations for printing functions, typically defined elsewhere.
extern void printf(char* str);
extern void printfHex(uint8_t);


/*
 * ----------------------------------------------------------------------------
 * InterruptHandler
 * ----------------------------------------------------------------------------
 *
 * Represents an individual handler for a specific interrupt vector. Subclasses
 * can override HandleInterrupt(...) to implement device-specific or exception-specific logic.
 */

/*
 * Constructor:
 *   - interruptManager: pointer to the owning InterruptManager
 *   - InterruptNumber: the specific IDT vector this handler will manage
 *
 * The constructor registers this handler in the manager's 'handlers' array at the given index.
 */
InterruptHandler::InterruptHandler(InterruptManager* interruptManager, uint8_t InterruptNumber)
{
    this->InterruptNumber = InterruptNumber;
    this->interruptManager = interruptManager;
    interruptManager->handlers[InterruptNumber] = this;
}

/*
 * Destructor:
 *  - If this handler is still the registered handler for its vector, remove it.
 */
InterruptHandler::~InterruptHandler()
{
    if(interruptManager->handlers[InterruptNumber] == this)
        interruptManager->handlers[InterruptNumber] = 0;
}

/*
 * HandleInterrupt:
 *  - Default implementation returns 'esp' unchanged. Subclasses can override to implement handling logic.
 */
uint32_t InterruptHandler::HandleInterrupt(uint32_t esp)
{
    return esp;
}




/*
 * ----------------------------------------------------------------------------
 * InterruptManager
 * ----------------------------------------------------------------------------
 *
 * Manages the global IDT (Interrupt Descriptor Table) for x86 protected mode,
 * sets up PIC (Programmable Interrupt Controller) remapping, and handles interrupt
 * dispatch to the correct InterruptHandler. Also integrates with a TaskManager
 * for multitasking if a timer interrupt occurs.
 */

/* The static array of IDT entries (256 possible vectors). */
InterruptManager::GateDescriptor InterruptManager::interruptDescriptorTable[256];

/* Global pointer to the currently active InterruptManager (only one can be active at a time). */
InterruptManager* InterruptManager::ActiveInterruptManager = 0;



/*
 * SetInterruptDescriptorTableEntry:
 *   - Configures one IDT entry with the given interrupt vector, code segment selector,
 *     handler function pointer, privilege level, and descriptor type.
 *
 * The GateDescriptor fields:
 *   - handlerAddressLowBits: low 16 bits of the function pointer.
 *   - handlerAddressHighBits: high 16 bits of the function pointer.
 *   - gdt_codeSegmentSelector: segment selector in the GDT that contains the code for this ISR.
 *   - access: indicates present bit, descriptor type (interrupt gate), DPL, etc.
 */
void InterruptManager::SetInterruptDescriptorTableEntry(uint8_t interrupt,
    uint16_t CodeSegment, void (*handler)(),
    uint8_t DescriptorPrivilegeLevel, uint8_t DescriptorType)
{
    interruptDescriptorTable[interrupt].handlerAddressLowBits = ((uint32_t)handler) & 0xFFFF;
    interruptDescriptorTable[interrupt].handlerAddressHighBits = (((uint32_t)handler) >> 16) & 0xFFFF;
    interruptDescriptorTable[interrupt].gdt_codeSegmentSelector = CodeSegment;

    const uint8_t IDT_DESC_PRESENT = 0x80;
    // access = 0x80 (present) | (DPL<<5) | descriptor type (e.g., 0xE for 32-bit interrupt gate)
    interruptDescriptorTable[interrupt].access
        = IDT_DESC_PRESENT
          | ((DescriptorPrivilegeLevel & 3) << 5)
          | DescriptorType;

    // reserved field must be zero
    interruptDescriptorTable[interrupt].reserved = 0;
}


/*
 * InterruptManager Constructor:
 *   - hardwareInterruptOffset: the base vector for remapped IRQ0..15 (e.g., 0x20).
 *   - globalDescriptorTable: used to get the code segment selector for IDT entries.
 *   - taskManager: for switching tasks on each timer interrupt (optional).
 *
 * Steps:
 *   1) Store references and set up PIC command/data ports.
 *   2) Initialize all IDT entries with InterruptIgnore (a do-nothing handler).
 *   3) Overwrite specific entries with real handlers for CPU exceptions (0..19),
 *      hardware IRQs (hardwareInterruptOffset + 0..15), and a special one at 0x80 (syscall).
 *   4) Remap the PIC from 0x08..0x0F to hardwareInterruptOffset..(hardwareInterruptOffset+7).
 *   5) Load the IDT with lidt instruction.
 */
InterruptManager::InterruptManager(uint16_t hardwareInterruptOffset,
                                   GlobalDescriptorTable* globalDescriptorTable,
                                   TaskManager* taskManager)
: programmableInterruptControllerMasterCommandPort(0x20),
  programmableInterruptControllerMasterDataPort(0x21),
  programmableInterruptControllerSlaveCommandPort(0xA0),
  programmableInterruptControllerSlaveDataPort(0xA1)
{
    this->taskManager = taskManager;
    this->hardwareInterruptOffset = hardwareInterruptOffset;

    uint32_t CodeSegment = globalDescriptorTable->CodeSegmentSelector();
    const uint8_t IDT_INTERRUPT_GATE = 0xE;

    // Set every entry in the IDT to InterruptIgnore
    for(uint8_t i = 255; i > 0; --i)
    {
        SetInterruptDescriptorTableEntry(i, CodeSegment, &InterruptIgnore, 0, IDT_INTERRUPT_GATE);
        handlers[i] = 0;
    }
    // also set vector 0 to InterruptIgnore
    SetInterruptDescriptorTableEntry(0, CodeSegment, &InterruptIgnore, 0, IDT_INTERRUPT_GATE);
    handlers[0] = 0;

    // CPU exceptions 0..19 (e.g., divide by zero, page fault, etc.)
    SetInterruptDescriptorTableEntry(0x00, CodeSegment, &HandleException0x00, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x01, CodeSegment, &HandleException0x01, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x02, CodeSegment, &HandleException0x02, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x03, CodeSegment, &HandleException0x03, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x04, CodeSegment, &HandleException0x04, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x05, CodeSegment, &HandleException0x05, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x06, CodeSegment, &HandleException0x06, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x07, CodeSegment, &HandleException0x07, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x08, CodeSegment, &HandleException0x08, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x09, CodeSegment, &HandleException0x09, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x0A, CodeSegment, &HandleException0x0A, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x0B, CodeSegment, &HandleException0x0B, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x0C, CodeSegment, &HandleException0x0C, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x0D, CodeSegment, &HandleException0x0D, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x0E, CodeSegment, &HandleException0x0E, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x0F, CodeSegment, &HandleException0x0F, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x10, CodeSegment, &HandleException0x10, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x11, CodeSegment, &HandleException0x11, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x12, CodeSegment, &HandleException0x12, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(0x13, CodeSegment, &HandleException0x13, 0, IDT_INTERRUPT_GATE);

    // Hardware IRQs mapped to hardwareInterruptOffset..hardwareInterruptOffset+15
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x00, CodeSegment, &HandleInterruptRequest0x00, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x01, CodeSegment, &HandleInterruptRequest0x01, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x02, CodeSegment, &HandleInterruptRequest0x02, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x03, CodeSegment, &HandleInterruptRequest0x03, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x04, CodeSegment, &HandleInterruptRequest0x04, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x05, CodeSegment, &HandleInterruptRequest0x05, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x06, CodeSegment, &HandleInterruptRequest0x06, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x07, CodeSegment, &HandleInterruptRequest0x07, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x08, CodeSegment, &HandleInterruptRequest0x08, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x09, CodeSegment, &HandleInterruptRequest0x09, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x0A, CodeSegment, &HandleInterruptRequest0x0A, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x0B, CodeSegment, &HandleInterruptRequest0x0B, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x0C, CodeSegment, &HandleInterruptRequest0x0C, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x0D, CodeSegment, &HandleInterruptRequest0x0D, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x0E, CodeSegment, &HandleInterruptRequest0x0E, 0, IDT_INTERRUPT_GATE);
    SetInterruptDescriptorTableEntry(hardwareInterruptOffset + 0x0F, CodeSegment, &HandleInterruptRequest0x0F, 0, IDT_INTERRUPT_GATE);

    // A separate software interrupt for syscalls at vector 0x80
    SetInterruptDescriptorTableEntry(0x80, CodeSegment, &HandleInterruptRequest0x80, 0, IDT_INTERRUPT_GATE);

    // Initialize PIC (Programmable Interrupt Controller) in cascade mode
    // 0x11 = start initialization
    programmableInterruptControllerMasterCommandPort.Write(0x11);
    programmableInterruptControllerSlaveCommandPort.Write(0x11);

    // Remap IRQ0..7 to hardwareInterruptOffset, IRQ8..15 to hardwareInterruptOffset+8
    programmableInterruptControllerMasterDataPort.Write(hardwareInterruptOffset);
    programmableInterruptControllerSlaveDataPort.Write(hardwareInterruptOffset + 8);

    // Set PIC wiring: master expects a slave on line 2, slave ID=2
    programmableInterruptControllerMasterDataPort.Write(0x04);
    programmableInterruptControllerSlaveDataPort.Write(0x02);

    // 0x01 = 8086 mode
    programmableInterruptControllerMasterDataPort.Write(0x01);
    programmableInterruptControllerSlaveDataPort.Write(0x01);

    // Mask all interrupts (0x00 = no mask -> all enabled)
    programmableInterruptControllerMasterDataPort.Write(0x00);
    programmableInterruptControllerSlaveDataPort.Write(0x00);

    // Load IDT by setting up the IDT pointer
    InterruptDescriptorTablePointer idt_pointer;
    idt_pointer.size = 256 * sizeof(GateDescriptor) - 1;
    idt_pointer.base = (uint32_t)interruptDescriptorTable;
    asm volatile("lidt %0" : : "m"(idt_pointer));
}

/*
 * Destructor:
 *  - Deactivates this interrupt manager if it's active.
 */
InterruptManager::~InterruptManager()
{
    Deactivate();
}

/*
 * HardwareInterruptOffset:
 *  - Returns the base offset where hardware IRQs are remapped.
 */
uint16_t InterruptManager::HardwareInterruptOffset()
{
    return hardwareInterruptOffset;
}

/*
 * Activate:
 *  - If there is already an active interrupt manager, deactivate it.
 *  - Mark this instance as the active manager.
 *  - Enable interrupts with 'sti'.
 */
void InterruptManager::Activate()
{
    if(ActiveInterruptManager != 0)
        ActiveInterruptManager->Deactivate();

    ActiveInterruptManager = this;
    asm("sti");
}

/*
 * Deactivate:
 *  - If this manager is active, set ActiveInterruptManager to null 
 *    and disable interrupts with 'cli'.
 */
void InterruptManager::Deactivate()
{
    if(ActiveInterruptManager == this)
    {
        ActiveInterruptManager = 0;
        asm("cli");
    }
}

/*
 * HandleInterrupt:
 *  - A static function called by the assembly stubs for each interrupt vector.
 *  - If there's an active manager, it delegates to DoHandleInterrupt; otherwise, returns esp.
 */
uint32_t InterruptManager::HandleInterrupt(uint8_t interrupt, uint32_t esp)
{
    if(ActiveInterruptManager != 0)
        return ActiveInterruptManager->DoHandleInterrupt(interrupt, esp);
    return esp;
}

/*
 * DoHandleInterrupt:
 *  - The core logic for handling an interrupt in the active manager context.
 *  - If a specific handler is registered, it calls that handler’s HandleInterrupt method.
 *  - If it's the timer interrupt (interrupt == hardwareInterruptOffset), 
 *    we invoke the TaskManager to switch tasks.
 *  - If it's a hardware IRQ, we send end-of-interrupt (EOI) to the PICs.
 */
uint32_t InterruptManager::DoHandleInterrupt(uint8_t interrupt, uint32_t esp)
{
    // Call the registered handler, if any
    if(handlers[interrupt] != 0)
    {
        esp = handlers[interrupt]->HandleInterrupt(esp);
    }
    else if(interrupt != hardwareInterruptOffset)
    {
        // For unhandled interrupts (excluding the timer), print a message
        printf("UNHANDLED INTERRUPT 0x");
        printfHex(interrupt);
    }
    
    // If this is the timer interrupt, schedule a new task if TaskManager is available
    if(interrupt == hardwareInterruptOffset)
    {
        esp = (uint32_t)taskManager->Schedule((CPUState*)esp);
    }

    // Acknowledge the PIC for hardware interrupts
    if(hardwareInterruptOffset <= interrupt && interrupt < hardwareInterruptOffset + 16)
    {
        // Send EOI (End Of Interrupt) to the master PIC
        programmableInterruptControllerMasterCommandPort.Write(0x20);
        // If it's an IRQ >= 8, also send EOI to the slave PIC
        if(hardwareInterruptOffset + 8 <= interrupt)
            programmableInterruptControllerSlaveCommandPort.Write(0x20);
    }

    return esp;
}
