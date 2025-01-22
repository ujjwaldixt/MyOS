#include <hardwarecommunication/pci.h>
#include <drivers/amd_am79c973.h>

/*
 * Using namespaces:
 *   - myos::common: provides fixed-width types (uint8_t, uint16_t, uint32_t, etc.)
 *   - myos::drivers: contains driver classes such as amd_am79c973.
 *   - myos::hardwarecommunication: provides low-level port I/O and interrupt support.
 */
using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;


/*
 * ----------------------------------------------------------------------------
 * PeripheralComponentInterconnectDeviceDescriptor Class
 * ----------------------------------------------------------------------------
 *
 * This class encapsulates the PCI device descriptor information. It holds
 * data about a device's bus, device, function numbers, vendor and device IDs,
 * class and subclass information, revision, interrupt line, and optionally
 * the base I/O port (portBase) if the device uses I/O-mapped registers.
 */

PeripheralComponentInterconnectDeviceDescriptor::PeripheralComponentInterconnectDeviceDescriptor()
{
    // Default constructor. Members remain uninitialized and should be set later.
}

PeripheralComponentInterconnectDeviceDescriptor::~PeripheralComponentInterconnectDeviceDescriptor()
{
    // Default destructor. No dynamic resources to free.
}


/*
 * ----------------------------------------------------------------------------
 * PeripheralComponentInterconnectController Class
 * ----------------------------------------------------------------------------
 *
 * This class provides methods to interact with the PCI configuration space.
 * It uses two port objects:
 *   - commandPort (0xCF8): used to specify the configuration address.
 *   - dataPort (0xCFC): used to read from/write to the configuration space.
 *
 * It includes functions for reading and writing to PCI configuration registers,
 * checking for multi-function devices, retrieving device descriptors, and
 * selecting and instantiating drivers for PCI devices.
 */

PeripheralComponentInterconnectController::PeripheralComponentInterconnectController()
: dataPort(0xCFC),
  commandPort(0xCF8)
{
    // The constructor initializes the configuration ports.
}

PeripheralComponentInterconnectController::~PeripheralComponentInterconnectController()
{
    // Destructor. No dynamic cleanup required.
}

/*
 * Read:
 *  - Reads a 32-bit value from the PCI configuration space for a given bus,
 *    device, function, and register offset.
 *  - The PCI configuration mechanism requires forming a special 32-bit address:
 *      Bit 31: enable bit (set to 1)
 *      Bits 23-16: bus number (8 bits)
 *      Bits 15-11: device number (5 bits)
 *      Bits 10-8: function number (3 bits)
 *      Bits 7-2: register number (registeroffset, aligned on 4-byte boundary)
 *      Bits 1-0: always 0 (due to 32-bit alignment)
 *  - After writing this address to commandPort, the dataPort is read.
 *  - The returned result is then shifted right by a multiple of 8, depending on
 *    registeroffset modulo 4, to align the desired 8/16/32-bit field.
 */
uint32_t PeripheralComponentInterconnectController::Read(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset)
{
    uint32_t id =
        0x1 << 31                              // Enable bit to indicate a configuration space access
        | ((bus & 0xFF) << 16)                   // Bus number: 8 bits
        | ((device & 0x1F) << 11)                // Device number: 5 bits
        | ((function & 0x07) << 8)               // Function number: 3 bits
        | (registeroffset & 0xFC);               // Register offset (must be aligned on 4 bytes; lower 2 bits zero)
    
    commandPort.Write(id);                      // Write the constructed address to the configuration space command port
    uint32_t result = dataPort.Read();          // Read the 32-bit result from the data port
    return result >> (8 * (registeroffset % 4));  // Adjust the result based on the offset within the 32-bit value
}

/*
 * Write:
 *  - Writes a 32-bit value to the PCI configuration space at the specified bus/device/function
 *    and register offset.
 *  - Constructs the configuration address similarly to the Read() function.
 *  - Writes the address to the commandPort, then writes the value to the dataPort.
 */
void PeripheralComponentInterconnectController::Write(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset, uint32_t value)
{
    uint32_t id =
        0x1 << 31                              // Enable bit
        | ((bus & 0xFF) << 16)
        | ((device & 0x1F) << 11)
        | ((function & 0x07) << 8)
        | (registeroffset & 0xFC);               // Aligned register offset
    commandPort.Write(id);
    dataPort.Write(value); 
}

/*
 * DeviceHasFunctions:
 *  - Checks if a PCI device supports multiple functions.
 *  - Reads the header type (at register offset 0x0E) and checks bit 7.
 *  - If bit 7 is set, then the device is a multi-function device.
 */
bool PeripheralComponentInterconnectController::DeviceHasFunctions(common::uint16_t bus, common::uint16_t device)
{
    return Read(bus, device, 0, 0x0E) & (1 << 7);
}


/*
 * SelectDrivers:
 *  - Scans the PCI bus for connected devices and attempts to select an appropriate driver for each.
 *  - For each bus (0-7) and for each device (0-31), it checks whether the device supports multiple functions.
 *  - For each function of the device, it retrieves the device descriptor.
 *  - If the vendor_id is 0x0000 or 0xFFFF, the slot is considered unused and is skipped.
 *  - For each Base Address Register (BAR) on the device (up to 6 BARs),
 *    if the BAR is valid and is of type InputOutput, the device's portBase is set to that address.
 *  - After processing, it attempts to instantiate a driver with GetDriver() and, if successful,
 *    adds it to the provided DriverManager.
 *  - Finally, prints out basic information (bus, device, function, vendor_id, device_id) for each device found.
 */
void PeripheralComponentInterconnectController::SelectDrivers(DriverManager* driverManager, myos::hardwarecommunication::InterruptManager* interrupts)
{
    for(int bus = 0; bus < 8; bus++)
    {
        for(int device = 0; device < 32; device++)
        {
            int numFunctions = DeviceHasFunctions(bus, device) ? 8 : 1;
            for(int function = 0; function < numFunctions; function++)
            {
                PeripheralComponentInterconnectDeviceDescriptor dev = GetDeviceDescriptor(bus, device, function);
                
                // Skip empty or invalid devices (vendor_id 0x0000 or 0xFFFF)
                if(dev.vendor_id == 0x0000 || dev.vendor_id == 0xFFFF)
                    continue;
                
                // Check each of the 6 possible Base Address Registers (BARs)
                for(int barNum = 0; barNum < 6; barNum++)
                {
                    BaseAddressRegister bar = GetBaseAddressRegister(bus, device, function, barNum);
                    // If the BAR indicates an I/O-mapped region and has a valid address,
                    // set the device's portBase to that address.
                    if(bar.address && (bar.type == InputOutput))
                        dev.portBase = (uint32_t)bar.address;
                }
                
                // Try to get a driver for the device, given its descriptor and the interrupt manager.
                Driver* driver = GetDriver(dev, interrupts);
                if(driver != 0)
                    driverManager->AddDriver(driver);

                // Print out basic PCI device information for debugging purposes.
                printf("PCI BUS ");
                printfHex(bus & 0xFF);
                
                printf(", DEVICE ");
                printfHex(device & 0xFF);

                printf(", FUNCTION ");
                printfHex(function & 0xFF);
                
                printf(" = VENDOR ");
                printfHex((dev.vendor_id & 0xFF00) >> 8);
                printfHex(dev.vendor_id & 0xFF);
                printf(", DEVICE ");
                printfHex((dev.device_id & 0xFF00) >> 8);
                printfHex(dev.device_id & 0xFF);
                printf("\n");
            }
        }
    }
}

/*
 * GetBaseAddressRegister:
 *  - Retrieves one of the device's Base Address Registers (BARs) from the PCI configuration space.
 *  - The BAR's value is read at offset 0x10 + (bar * 4).
 *  - The header type of the device (found at offset 0x0E) is used to determine the number of BARs present.
 *    Some devices have fewer than 6 BARs.
 *  - If the specified BAR index is invalid (>= maxBARs), an empty result is returned.
 *  - For I/O BARs, the address is the BAR value with the lower two bits masked off.
 *  - For memory-mapped BARs, additional processing would normally be done to interpret
 *    the BAR (such as determining 32-bit vs 64-bit addressing), but here it simply returns
 *    a default value.
 */
BaseAddressRegister PeripheralComponentInterconnectController::GetBaseAddressRegister(uint16_t bus, uint16_t device, uint16_t function, uint16_t bar)
{
    BaseAddressRegister result;
    
    // Get the header type (header type is in register 0x0E, masked by 0x7F to ignore multi-function flag)
    uint32_t headertype = Read(bus, device, function, 0x0E) & 0x7F;
    int maxBARs = 6 - (4 * headertype);  // Some devices (e.g., bridges) have fewer BARs.
    if(bar >= maxBARs)
        return result;
    
    uint32_t bar_value = Read(bus, device, function, 0x10 + 4 * bar);
    // Determine the BAR type: if bit 0 is set, it's an I/O BAR; otherwise, it's memory-mapped.
    result.type = (bar_value & 0x1) ? InputOutput : MemoryMapping;
    uint32_t temp;
    
    if(result.type == MemoryMapping)
    {
        // For memory-mapped BARs, additional interpretation of bits may be required
        // (e.g., handling 32-bit vs. 64-bit BARs), but here the code is minimal.
        switch((bar_value >> 1) & 0x3)
        {
            case 0: // 32 Bit Mode
            case 1: // 20 Bit Mode
            case 2: // 64 Bit Mode
                break;
        }
    }
    else // For I/O BARs:
    {
        // Mask off the lower two bits to get the base I/O address
        result.address = (uint8_t*)(bar_value & ~0x3);
        result.prefetchable = false;  // I/O regions are not prefetchable.
    }
    
    return result;
}

/*
 * GetDriver:
 *  - Based on the device descriptor, attempts to instantiate an appropriate driver.
 *  - For known vendors and devices, this function creates a new driver object.
 *
 *  Example:
 *    - For vendor 0x1022 (AMD) and device 0x2000, an instance of amd_am79c973 is allocated.
 *    - The driver is constructed using placement new on memory allocated by the active MemoryManager.
 *
 *  If a known driver cannot be created, additional checks (such as checking dev.class_id)
 *  can be performed (e.g., for VGA, network, etc.). Currently, only the AMD am79c973 is handled.
 */
Driver* PeripheralComponentInterconnectController::GetDriver(PeripheralComponentInterconnectDeviceDescriptor dev, InterruptManager* interrupts)
{
    Driver* driver = 0;
    switch(dev.vendor_id)
    {
        case 0x1022: // AMD
            switch(dev.device_id)
            {
                case 0x2000: // am79c973 network card
                    printf("AMD am79c973 ");
                    driver = (amd_am79c973*)MemoryManager::activeMemoryManager->malloc(sizeof(amd_am79c973));
                    if(driver != 0)
                        new (driver) amd_am79c973(&dev, interrupts); // Placement new: construct the driver in allocated memory
                    else
                        printf("instantiation failed");
                    return driver;
                    break;
            }
            break;

        case 0x8086: // Intel devices
            break;
    }
    
    // As an additional check, for devices classified as graphics (class_id 0x03)
    // and specifically VGA (subclass_id 0x00), you might instantiate a VGA driver.
    switch(dev.class_id)
    {
        case 0x03: // Graphics devices
            switch(dev.subclass_id)
            {
                case 0x00: // VGA-compliant device
                    printf("VGA ");
                    break;
            }
            break;
    }
    
    return driver;
}

/*
 * GetDeviceDescriptor:
 *  - Fills a PeripheralComponentInterconnectDeviceDescriptor with data from the PCI
 *    configuration space for the specified bus, device, and function.
 *  - Reads registers for vendor ID, device ID, revision, class codes, and the interrupt line.
 */
PeripheralComponentInterconnectDeviceDescriptor PeripheralComponentInterconnectController::GetDeviceDescriptor(uint16_t bus, uint16_t device, uint16_t function)
{
    PeripheralComponentInterconnectDeviceDescriptor result;
    
    result.bus = bus;
    result.device = device;
    result.function = function;
    
    // Vendor and Device ID are stored in the first 4 bytes
    result.vendor_id = Read(bus, device, function, 0x00);
    result.device_id = Read(bus, device, function, 0x02);

    // Class and Subclass information are stored in registers 0x0B, 0x0A, 0x09 respectively.
    result.class_id = Read(bus, device, function, 0x0B);
    result.subclass_id = Read(bus, device, function, 0x0A);
    result.interface_id = Read(bus, device, function, 0x09);

    // Revision ID is at register 0x08.
    result.revision = Read(bus, device, function, 0x08);
    // Interrupt line is stored at register 0x3C.
    result.interrupt = Read(bus, device, function, 0x3c);
    
    return result;
}
