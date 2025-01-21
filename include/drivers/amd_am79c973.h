#ifndef __MYOS__DRIVERS__AMD_AM79C973_H              // Header guard to prevent multiple inclusions of this file
#define __MYOS__DRIVERS__AMD_AM79C973_H

#include <common/types.h>                            // Include fundamental type definitions (uint8_t, uint32_t, etc.)
#include <drivers/driver.h>                          // Include base driver class definitions
#include <hardwarecommunication/pci.h>               // Include PCI-related definitions (Peripheral Component Interconnect)
#include <hardwarecommunication/interrupts.h>        // Include interrupt-related definitions
#include <hardwarecommunication/port.h>              // Include I/O port abstractions

namespace myos
{
    namespace drivers
    {
        // Forward declare the amd_am79c973 class so it can be referenced by RawDataHandler
        class amd_am79c973;

        // This class provides a mechanism to handle "raw data" (network packets or similar data)
        // received by the amd_am79c973 driver. The RawDataHandler is associated with a backend
        // (the amd_am79c973 device) for sending and receiving data.
        class RawDataHandler
        {
        protected:
            // Points to the amd_am79c973 device that uses this handler
            amd_am79c973* backend;
        public:
            // Constructor initializes the handler with a specific amd_am79c973 device
            RawDataHandler(amd_am79c973* backend);
            
            // Destructor cleans up resources if necessary (currently empty)
            ~RawDataHandler();
            
            // This method is called when raw data has been received.
            // buffer: the start of the received data
            // size: size of the data in bytes
            // Returns a boolean value indicating if the data was handled successfully
            virtual bool OnRawDataReceived(common::uint8_t* buffer, common::uint32_t size);

            // Sends raw data to the network via the associated amd_am79c973 device.
            // buffer: the data to send
            // size: size of the data in bytes
            void Send(common::uint8_t* buffer, common::uint32_t size);
        };
        
        
        // The amd_am79c973 class is a driver for the AMD AM79C973 network interface controller (NIC).
        // It extends the generic Driver class and InterruptHandler to handle network I/O and hardware interrupts.
        class amd_am79c973 : public Driver, public hardwarecommunication::InterruptHandler
        {
            // The InitializationBlock is a data structure used by the AMD AM79C973 NIC to configure
            // various settings. It is often placed in memory in a specific format and accessed by the NIC.
            // The __attribute__((packed)) ensures the compiler does not add alignment padding between fields.
            struct InitializationBlock
            {
                common::uint16_t mode;                 // Stores mode settings (e.g., enabling/disabling certain features)
                unsigned reserved1 : 4;                // Reserved bits (part of low-level hardware specification)
                unsigned numSendBuffers : 4;           // Number of buffers used for sending packets
                unsigned reserved2 : 4;                // Reserved bits
                unsigned numRecvBuffers : 4;           // Number of buffers used for receiving packets
                common::uint64_t physicalAddress : 48; // MAC address of the NIC (48-bit hardware address)
                common::uint16_t reserved3;            // Reserved field
                common::uint64_t logicalAddress;        // Logical address (e.g., IP address in some contexts)
                common::uint32_t recvBufferDescrAddress;// Starting address of the receive buffer descriptors
                common::uint32_t sendBufferDescrAddress;// Starting address of the send buffer descriptors
            } __attribute__((packed));
            
            // Each BufferDescriptor describes one buffer in memory where data (packets) can be stored.
            // The descriptor contains information about the buffer's location, size, and state.
            // The __attribute__((packed)) directive ensures no extra padding is added.
            struct BufferDescriptor
            {
                common::uint32_t address;              // Physical address of the buffer
                common::uint32_t flags;                // Flags indicating status, ownership, and other properties
                common::uint32_t flags2;               // Additional flags or extended status information
                common::uint32_t avail;                // Available space or metadata for driver/HW usage
            } __attribute__((packed));
            
            // The following ports are used to communicate with the AM79C973 NIC at the I/O level.
            // Each port corresponds to a specific I/O address that the device expects to read/write.
            hardwarecommunication::Port16Bit MACAddress0Port;          // Access to MAC address low word
            hardwarecommunication::Port16Bit MACAddress2Port;          // Access to MAC address middle word
            hardwarecommunication::Port16Bit MACAddress4Port;          // Access to MAC address high word
            hardwarecommunication::Port16Bit registerDataPort;         // Data port for register reads/writes
            hardwarecommunication::Port16Bit registerAddressPort;      // Address port to specify which register to access
            hardwarecommunication::Port16Bit resetPort;                // Port used to reset the NIC
            hardwarecommunication::Port16Bit busControlRegisterDataPort;// Port for bus control register access
            
            // The InitializationBlock used to configure the device on startup
            InitializationBlock initBlock;
            
            // Array of buffer descriptors for sending data, and some memory to hold them.
            // The 2048+15 size aligns the memory to a boundary suitable for the device (32-bit or 16-bit alignment).
            BufferDescriptor* sendBufferDescr;
            common::uint8_t sendBufferDescrMemory[2048+15];      // Memory for storing send buffer descriptors
            common::uint8_t sendBuffers[2*1024+15][8];           // Actual buffers used for packet data
            common::uint8_t currentSendBuffer;                   // Index pointing to the current buffer for sending

            // Array of buffer descriptors for receiving data, and some memory to hold them.
            BufferDescriptor* recvBufferDescr;
            common::uint8_t recvBufferDescrMemory[2048+15];      // Memory for storing receive buffer descriptors
            common::uint8_t recvBuffers[2*1024+15][8];           // Actual buffers used for packet data
            common::uint8_t currentRecvBuffer;                   // Index pointing to the current buffer for receiving
            
            // Handler to process raw data received by this network driver
            RawDataHandler* handler;
            
        public:
            // Constructor that initializes ports, the interrupt manager, and configures the device based on
            // PCI information passed via the dev descriptor (PeripheralComponentInterconnectDeviceDescriptor).
            amd_am79c973(myos::hardwarecommunication::PeripheralComponentInterconnectDeviceDescriptor *dev,
                         myos::hardwarecommunication::InterruptManager* interrupts);
            
            // Destructor for cleanup if needed (currently empty).
            ~amd_am79c973();
            
            // Activates the device (e.g., enables DMA, sets up buffers, configures interrupts).
            void Activate();
            
            // Resets the device to a known state and returns an integer representing the status.
            int Reset();
            
            // This method is called by the interrupt manager when the device triggers an interrupt.
            // It handles packet sending/receiving or device-specific events.
            // esp: the current stack pointer (used to return the new stack pointer if modified).
            // Returns the (possibly) updated stack pointer after handling the interrupt.
            common::uint32_t HandleInterrupt(common::uint32_t esp);
            
            // Sends a packet of data across the network using the AMD NIC.
            // buffer: pointer to the data to be sent
            // count: number of bytes in the buffer
            void Send(common::uint8_t* buffer, int count);

            // Called internally (and by interrupts) to handle incoming packets. Processes them,
            // then hands them to the RawDataHandler for further handling if available.
            void Receive();
            
            // Assigns a RawDataHandler to this device, allowing external code to handle incoming data.
            void SetHandler(RawDataHandler* handler);
            
            // Returns the 48-bit MAC address of this network interface as a 64-bit value (with upper bits unused).
            common::uint64_t GetMACAddress();
            
            // Sets the IP address for this network interface (if needed).
            void SetIPAddress(common::uint32_t);
            
            // Retrieves the currently assigned IP address from this network interface.
            common::uint32_t GetIPAddress();
        };
    }
}

#endif  // End of header guard
