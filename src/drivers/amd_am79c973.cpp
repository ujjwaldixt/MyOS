#include <drivers/amd_am79c973.h>

/*
 * Namespace usage for clarity: 
 *  - myos: main OS namespace
 *  - common: contains fundamental types (uint8_t, uint32_t, etc.)
 *  - drivers: contains driver-related classes (e.g., amd_am79c973)
 *  - hardwarecommunication: contains hardware I/O and interrupt-related classes
 */
using namespace myos;
using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;


/*
 * ----------------------------------
 * RawDataHandler Class Definitions
 * ----------------------------------
 */

/*
 * Constructor:
 *  - Associates this RawDataHandler with a specific amd_am79c973 device.
 *  - Sets this handler in the NIC driver, so the driver knows to forward raw data to this handler.
 */
RawDataHandler::RawDataHandler(amd_am79c973* backend)
{
    this->backend = backend;
    backend->SetHandler(this);
}

/*
 * Destructor:
 *  - Detaches this handler from the NIC driver by setting the driver’s handler to null (0).
 */
RawDataHandler::~RawDataHandler()
{
    backend->SetHandler(0);
}
            
/*
 * OnRawDataReceived:
 *  - Called by the NIC driver whenever raw data is received.
 *  - Returns a boolean indicating if the data was processed (default false here).
 *  - Derived or extended handlers can override this to process incoming packets.
 */
bool RawDataHandler::OnRawDataReceived(uint8_t* buffer, uint32_t size)
{
    return false;
}

/*
 * Send:
 *  - Forwards data to the NIC driver for transmission.
 *  - The size parameter is the length of the buffer in bytes.
 */
void RawDataHandler::Send(uint8_t* buffer, uint32_t size)
{
    backend->Send(buffer, size);
}


/*
 * External C-style functions (likely from other parts of the OS):
 *  - printf: prints a string to the console.
 *  - printfHex: prints a byte in hexadecimal format.
 */
extern void printf(char*);
extern void printfHex(uint8_t);


/*
 * ----------------------------------
 * amd_am79c973 Class Definitions
 * ----------------------------------
 */

/*
 * Constructor:
 *  - Initializes the amd_am79c973 driver based on information in the PCI descriptor 'dev',
 *    and sets up interrupt handling via 'interrupts'.
 *  - Maps I/O ports used by the NIC (MACAddress0Port, registerDataPort, etc.).
 *  - Reads and composes the MAC address from hardware.
 *  - Sets up the Initialization Block (initBlock) and configures send/receive buffers.
 */
amd_am79c973::amd_am79c973(PeripheralComponentInterconnectDeviceDescriptor *dev,
                           InterruptManager* interrupts)
:   Driver(),
    InterruptHandler(interrupts, dev->interrupt + interrupts->HardwareInterruptOffset()),
    MACAddress0Port(dev->portBase),
    MACAddress2Port(dev->portBase + 0x02),
    MACAddress4Port(dev->portBase + 0x04),
    registerDataPort(dev->portBase + 0x10),
    registerAddressPort(dev->portBase + 0x12),
    resetPort(dev->portBase + 0x14),
    busControlRegisterDataPort(dev->portBase + 0x16)
{
    this->handler = 0;               // No handler initially set
    currentSendBuffer = 0;
    currentRecvBuffer = 0;
    
    // Read the MAC address from the hardware ports
    uint64_t MAC0 = MACAddress0Port.Read() % 256;   // Lower byte of MACAddress0
    uint64_t MAC1 = MACAddress0Port.Read() / 256;   // Upper byte of MACAddress0
    uint64_t MAC2 = MACAddress2Port.Read() % 256;   // Lower byte of MACAddress2
    uint64_t MAC3 = MACAddress2Port.Read() / 256;   // Upper byte of MACAddress2
    uint64_t MAC4 = MACAddress4Port.Read() % 256;   // Lower byte of MACAddress4
    uint64_t MAC5 = MACAddress4Port.Read() / 256;   // Upper byte of MACAddress4
    
    // Combine into a 48-bit MAC (stored in a 64-bit container)
    uint64_t MAC = (MAC5 << 40)
                 | (MAC4 << 32)
                 | (MAC3 << 24)
                 | (MAC2 << 16)
                 | (MAC1 << 8)
                 | (MAC0);
    
    // Set 32-bit mode via the bus control register (Register #20)
    registerAddressPort.Write(20);
    busControlRegisterDataPort.Write(0x102);
    
    // Stop/reset the card (Register #0, write 0x04 = STOP)
    registerAddressPort.Write(0);
    registerDataPort.Write(0x04);
    
    // Prepare the Initialization Block
    initBlock.mode       = 0x0000;   // Normal mode (non-promiscuous)
    initBlock.reserved1  = 0;
    initBlock.numSendBuffers = 3;    // Log2(# of send buffers) => 2^3 = 8
    initBlock.reserved2  = 0;
    initBlock.numRecvBuffers = 3;    // Log2(# of recv buffers) => 2^3 = 8
    initBlock.physicalAddress = MAC; // Store MAC
    initBlock.reserved3  = 0;
    initBlock.logicalAddress = 0;    // No IP set yet (will be updated later)
    
    // Align and store the addresses of send/receive descriptor arrays
    sendBufferDescr = (BufferDescriptor*)(
        (((uint32_t)&sendBufferDescrMemory[0]) + 15) & ~((uint32_t)0xF)
    );
    initBlock.sendBufferDescrAddress = (uint32_t)sendBufferDescr;
    
    recvBufferDescr = (BufferDescriptor*)(
        (((uint32_t)&recvBufferDescrMemory[0]) + 15) & ~((uint32_t)0xF)
    );
    initBlock.recvBufferDescrAddress = (uint32_t)recvBufferDescr;
    
    // Initialize each descriptor
    for(uint8_t i = 0; i < 8; i++)
    {
        // Set address (aligned) for send buffers
        sendBufferDescr[i].address = 
            ((((uint32_t)&sendBuffers[i]) + 15 ) & ~(uint32_t)0xF);

        // Descriptor flags: size, ownership bit, etc.
        sendBufferDescr[i].flags  = 0x7FF | 0xF000;    // 0xF000 => owned by driver, 7FF => buffer size
        sendBufferDescr[i].flags2 = 0;
        sendBufferDescr[i].avail  = 0;
        
        // Set address (aligned) for receive buffers
        recvBufferDescr[i].address = 
            ((((uint32_t)&recvBuffers[i]) + 15 ) & ~(uint32_t)0xF);

        // 0xF7FF => buffer size (2047 bytes?), 0x80000000 => owned by card
        recvBufferDescr[i].flags = 0xF7FF | 0x80000000;
        recvBufferDescr[i].flags2 = 0;
        // This appears to be a minor bug: the line below uses 'sendBufferDescr[i].avail' instead of 'recvBufferDescr[i].avail'
        sendBufferDescr[i].avail = 0;  
    }
    
    // Store the lower 16 bits of initBlock address in register #1
    registerAddressPort.Write(1);
    registerDataPort.Write( (uint32_t)(&initBlock) & 0xFFFF );

    // Store the upper 16 bits of initBlock address in register #2
    registerAddressPort.Write(2);
    registerDataPort.Write( ((uint32_t)(&initBlock) >> 16) & 0xFFFF );
}

/*
 * Destructor: typically does nothing specific here.
 */
amd_am79c973::~amd_am79c973()
{
}
            

/*
 * Activate:
 *  - Performs final steps to enable the card, such as turning on interrupts (Init Done) and the start command.
 *  - Bits 0x41 and 0x42 are written to Register #0 to issue START & INIT commands, etc.
 */
void amd_am79c973::Activate()
{
    // Issue START command (write 0x41 to reg #0)
    registerAddressPort.Write(0);
    registerDataPort.Write(0x41);

    // Enable interrupts by setting bits in register #4
    registerAddressPort.Write(4);
    uint32_t temp = registerDataPort.Read();
    registerAddressPort.Write(4);
    registerDataPort.Write(temp | 0xC00);
    
    // Issue INIT command (write 0x42 to reg #0)
    registerAddressPort.Write(0);
    registerDataPort.Write(0x42);
}

/*
 * Reset:
 *  - Triggers a reset via resetPort by reading and then writing 0.
 *  - Returns an arbitrary integer (10), possibly used as a delay or status code.
 */
int amd_am79c973::Reset()
{
    resetPort.Read();
    resetPort.Write(0);
    return 10;
}



/*
 * HandleInterrupt:
 *  - Invoked by the interrupt manager when the AMD NIC triggers an interrupt.
 *  - Reads the status register (#0), checks various error/status bits, and acknowledges them.
 *  - Calls Receive() if data is available or logs collisions/missed frames/etc.
 *  - Returns the stack pointer (esp) unchanged in this implementation.
 */
uint32_t amd_am79c973::HandleInterrupt(common::uint32_t esp)
{
    // Read status
    registerAddressPort.Write(0);
    uint32_t temp = registerDataPort.Read();
    
    if((temp & 0x8000) == 0x8000)
        printf("AMD am79c973 ERROR\n");
    if((temp & 0x2000) == 0x2000)
        printf("AMD am79c973 COLLISION ERROR\n");
    if((temp & 0x1000) == 0x1000)
        printf("AMD am79c973 MISSED FRAME\n");
    if((temp & 0x0800) == 0x0800)
        printf("AMD am79c973 MEMORY ERROR\n");
    if((temp & 0x0400) == 0x0400)
        Receive();
    if((temp & 0x0200) == 0x0200)
        printf(" SENT");

    // Acknowledge interrupt by writing back the status bits
    registerAddressPort.Write(0);
    registerDataPort.Write(temp);
    
    if((temp & 0x0100) == 0x0100)
        printf("AMD am79c973 INIT DONE\n");
    
    return esp;
}

       
/*
 * Send:
 *  - Places a packet into the next available send buffer, then signals the NIC to start transmission.
 *  - size: Number of bytes in the packet (max 1518 for Ethernet).
 *  - currentSendBuffer is used to track which descriptor to use.
 *  - The contents of 'buffer' are copied backward into the descriptor's buffer memory.
 */
void amd_am79c973::Send(uint8_t* buffer, int size)
{
    int sendDescriptor = currentSendBuffer;
    currentSendBuffer = (currentSendBuffer + 1) % 8;
    
    if(size > 1518)
        size = 1518;
    
    // Copy payload into the buffer, from end to start
    for(uint8_t *src = buffer + size -1,
                *dst = (uint8_t*)(sendBufferDescr[sendDescriptor].address + size -1);
                src >= buffer; src--, dst--)
    {
        *dst = *src;
    }
        
    printf("\nSEND: ");
    // Print partial contents of the packet (from byte 34 up to 64 or 'size')
    for(int i = 14+20; i < (size>64?64:size); i++)
    {
        printfHex(buffer[i]);
        printf(" ");
    }
    
    // Mark descriptor as ready to send, owned by NIC, with the size set
    sendBufferDescr[sendDescriptor].avail = 0;
    sendBufferDescr[sendDescriptor].flags2 = 0;
    sendBufferDescr[sendDescriptor].flags = 0x8300F000
                                          | ((uint16_t)((-size) & 0xFFF));
                                          
    // Write to register #0 to notify the NIC to transmit (bit 4 = transmit demand)
    registerAddressPort.Write(0);
    registerDataPort.Write(0x48);
}

/*
 * Receive:
 *  - Processes all receive buffers that are marked as complete by the NIC (ownership bit cleared).
 *  - For each valid packet (flags & 0x03000000 == 0x03000000 implies good packet),
 *    prints partial data, then calls the handler->OnRawDataReceived if set.
 *  - Resets the descriptor ownership bit (0x80000000) for the NIC to reuse.
 */
void amd_am79c973::Receive()
{
    printf("\nRECV: ");
    
    // Loop through all receive buffers until we find one still owned by the NIC (bit 31 set)
    for(; (recvBufferDescr[currentRecvBuffer].flags & 0x80000000) == 0;
        currentRecvBuffer = (currentRecvBuffer + 1) % 8)
    {
        // If it's a valid packet (not an error frame, etc.)
        if(!(recvBufferDescr[currentRecvBuffer].flags & 0x40000000)  // no error
         && (recvBufferDescr[currentRecvBuffer].flags & 0x03000000) == 0x03000000) // 0x03 => packet received OK
        {
            uint32_t size = recvBufferDescr[currentRecvBuffer].flags & 0xFFF; // Lower 12 bits = packet length
            if(size > 64)
                size -= 4;  // Remove checksum if size > 64
            
            // Pointer to the received data
            uint8_t* buffer = (uint8_t*)(recvBufferDescr[currentRecvBuffer].address);

            // Print partial contents of the received packet
            for(int i = 14+20; i < (size>64?64:size); i++)
            {
                printfHex(buffer[i]);
                printf(" ");
            }

            // If we have a handler and it returns true, we echo the packet back (for testing)
            if(handler != 0)
                if(handler->OnRawDataReceived(buffer, size))
                    Send(buffer, size);
        }
        
        // Reset descriptor ownership to the NIC and restore buffer length flags
        recvBufferDescr[currentRecvBuffer].flags2 = 0;
        recvBufferDescr[currentRecvBuffer].flags = 0x8000F7FF;
    }
}

/*
 * SetHandler:
 *  - Allows an external RawDataHandler to be assigned for handling received packets.
 */
void amd_am79c973::SetHandler(RawDataHandler* handler)
{
    this->handler = handler;
}

/*
 * GetMACAddress:
 *  - Returns the 48-bit MAC address stored in the initBlock.
 */
uint64_t amd_am79c973::GetMACAddress()
{
    return initBlock.physicalAddress;
}

/*
 * SetIPAddress:
 *  - Sets the logical IP address in the initBlock.
 *  - This is typically used for higher-layer protocols (ARP, IPv4) that query the NIC’s IP.
 */
void amd_am79c973::SetIPAddress(uint32_t ip)
{
    initBlock.logicalAddress = ip;
}

/*
 * GetIPAddress:
 *  - Returns the logical IP address stored in the initBlock.
 */
uint32_t amd_am79c973::GetIPAddress()
{
    return initBlock.logicalAddress;
}
