#ifndef __MYOS__HARDWARECOMMUNICATION__PCI_H               // Header guard to prevent multiple inclusions
#define __MYOS__HARDWARECOMMUNICATION__PCI_H

#include <hardwarecommunication/port.h>                    // Provides classes for port-based I/O
#include <drivers/driver.h>                                // Provides the Driver and DriverManager classes
#include <common/types.h>                                  // Common type aliases (e.g., uint8_t, uint32_t)
#include <hardwarecommunication/interrupts.h>              // For handling hardware interrupts
#include <memorymanagement.h>                              // Memory management functions

namespace myos
{
    namespace hardwarecommunication
    {
        /*
         * BaseAddressRegisterType:
         *  Distinguishes between a PCI BAR (Base Address Register) used for MemoryMapping
         *  versus one used for InputOutput (I/O) space.
         */
        enum BaseAddressRegisterType
        {
            MemoryMapping = 0,
            InputOutput = 1
        };
        
        
        /*
         * BaseAddressRegister:
         *  Represents the contents of a PCI BAR (Base Address Register). This class
         *  encapsulates the address, size, whether it's prefetchable, and the BAR type.
         *  
         *  prefetchable: indicates if the memory region can be cached/prefetched by the CPU.
         *  address: the base address of the device's memory or I/O range (depends on 'type').
         *  size: the size (in bytes) of the region.
         *  type: indicates whether this is an I/O BAR or memory-mapped BAR.
         */
        class BaseAddressRegister
        {
        public:
            bool prefetchable;
            myos::common::uint8_t* address;
            myos::common::uint32_t size;
            BaseAddressRegisterType type;
        };
        
        
        /*
         * PeripheralComponentInterconnectDeviceDescriptor:
         *  Contains information about a PCI device, including bus/device/function
         *  addresses, vendor/device IDs, class/subclass, revision, and so forth.
         *  
         *  portBase: base I/O port if the device is an I/O-mapped device.
         *  interrupt: interrupt line or IRQ associated with this device.
         */
        class PeripheralComponentInterconnectDeviceDescriptor
        {
        public:
            myos::common::uint32_t portBase;
            myos::common::uint32_t interrupt;
            
            myos::common::uint16_t bus;
            myos::common::uint16_t device;
            myos::common::uint16_t function;

            myos::common::uint16_t vendor_id;
            myos::common::uint16_t device_id;
            
            myos::common::uint8_t class_id;
            myos::common::uint8_t subclass_id;
            myos::common::uint8_t interface_id;

            myos::common::uint8_t revision;
            
            // Default constructor, initializes fields to default values.
            PeripheralComponentInterconnectDeviceDescriptor();
            
            // Destructor for cleanup if necessary (usually empty).
            ~PeripheralComponentInterconnectDeviceDescriptor();
        };


        /*
         * PeripheralComponentInterconnectController:
         *  Manages scanning and interacting with PCI devices on the system.
         *  Provides methods to read from and write to PCI configuration space,
         *  retrieve device descriptors, and identify base address registers (BARs).
         *  
         *  dataPort & commandPort: used to communicate with the PCI configuration space 
         *  (commonly at 0xCF8 (command) and 0xCFC (data)).
         */
        class PeripheralComponentInterconnectController
        {
            // Ports for accessing PCI configuration space:
            //   0xCF8 - commandPort
            //   0xCFC - dataPort
            Port32Bit dataPort;
            Port32Bit commandPort;
            
        public:
            // Constructor initializes the ports with the standard PCI config addresses.
            PeripheralComponentInterconnectController();
            ~PeripheralComponentInterconnectController();
            
            /*
             * Read:
             *  Reads a 32-bit value from the PCI configuration space for a specific
             *  bus/device/function and register offset.
             */
            myos::common::uint32_t Read(myos::common::uint16_t bus, 
                                        myos::common::uint16_t device, 
                                        myos::common::uint16_t function, 
                                        myos::common::uint32_t registeroffset);

            /*
             * Write:
             *  Writes a 32-bit value to the PCI configuration space for a specific
             *  bus/device/function and register offset.
             */
            void Write(myos::common::uint16_t bus, 
                       myos::common::uint16_t device, 
                       myos::common::uint16_t function, 
                       myos::common::uint32_t registeroffset, 
                       myos::common::uint32_t value);

            /*
             * DeviceHasFunctions:
             *  Checks if a device (specified by bus & device IDs) contains multiple functions.
             *  Some PCI devices can implement more than one function (e.g., multi-function cards).
             */
            bool DeviceHasFunctions(myos::common::uint16_t bus, myos::common::uint16_t device);
            
            /*
             * SelectDrivers:
             *  Iterates over all possible PCI devices and obtains the appropriate driver 
             *  for each device (via GetDriver). Then registers these drivers with the provided
             *  driverManager. Also takes an InterruptManager to handle interrupts from those devices.
             */
            void SelectDrivers(myos::drivers::DriverManager* driverManager, 
                               myos::hardwarecommunication::InterruptManager* interrupts);

            /*
             * GetDriver:
             *  Given a PCI device descriptor, returns a pointer to a Driver object
             *  capable of handling that device (e.g., a network card driver).
             *  If no suitable driver is found, returns a generic Driver.
             */
            myos::drivers::Driver* GetDriver(PeripheralComponentInterconnectDeviceDescriptor dev, 
                                             myos::hardwarecommunication::InterruptManager* interrupts);

            /*
             * GetDeviceDescriptor:
             *  Collects information (vendor ID, device ID, class, subclass, etc.) from PCI configuration space 
             *  to fill a PeripheralComponentInterconnectDeviceDescriptor for the specified bus/device/function.
             */
            PeripheralComponentInterconnectDeviceDescriptor GetDeviceDescriptor(myos::common::uint16_t bus, 
                                                                                myos::common::uint16_t device, 
                                                                                myos::common::uint16_t function);

            /*
             * GetBaseAddressRegister:
             *  Reads and interprets one of the Base Address Registers (BARs) from the PCI config space.
             *  Determines if it’s memory-mapped or I/O-based, its prefetchability, size, etc.
             */
            BaseAddressRegister GetBaseAddressRegister(myos::common::uint16_t bus, 
                                                       myos::common::uint16_t device, 
                                                       myos::common::uint16_t function, 
                                                       myos::common::uint16_t bar);
        };

    }
}

#endif // __MYOS__HARDWARECOMMUNICATION__PCI_H
