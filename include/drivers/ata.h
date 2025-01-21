#ifndef __MYOS__DRIVERS__ATA_H                      // Header guard to avoid multiple inclusions
#define __MYOS__DRIVERS__ATA_H

#include <common/types.h>                            // Provides integral type definitions (e.g. uint8_t, uint16_t, uint32_t)
#include <hardwarecommunication/interrupts.h>        // Required for interrupt handling in other related code
#include <hardwarecommunication/port.h>              // Provides Port I/O functionality for reading/writing to hardware ports

namespace myos
{
    namespace drivers
    {
        // The AdvancedTechnologyAttachment (ATA) class provides methods to interact with an
        // ATA (IDE) hard drive or a similar storage device. It allows for identifying the drive,
        // reading, writing, and flushing (finalizing cached writes).
        class AdvancedTechnologyAttachment
        {
        protected:
            // Indicates whether this object represents the master drive (true) or the slave drive (false).
            bool master;

            // The following Port objects handle read/write operations at specific I/O addresses.
            // These addresses are typically derived from the portBase provided during construction.
            hardwarecommunication::Port16Bit dataPort;     // 16-bit data port used for transferring data to/from the drive
            hardwarecommunication::Port8Bit errorPort;     // 8-bit port for reading error information from the drive
            hardwarecommunication::Port8Bit sectorCountPort; // 8-bit port for specifying the number of sectors to read/write
            hardwarecommunication::Port8Bit lbaLowPort;    // 8-bit port for the low byte of the LBA (Logical Block Address)
            hardwarecommunication::Port8Bit lbaMidPort;    // 8-bit port for the middle byte of the LBA
            hardwarecommunication::Port8Bit lbaHiPort;     // 8-bit port for the high byte of the LBA
            hardwarecommunication::Port8Bit devicePort;    // 8-bit port to select the device (master/slave) and set LBA bits
            hardwarecommunication::Port8Bit commandPort;   // 8-bit port for sending commands (e.g., read, write, identify)
            hardwarecommunication::Port8Bit controlPort;   // 8-bit port for controlling features like interrupts, resets, etc.

        public:
            // Constructor that takes a boolean indicating whether the drive is master or slave,
            // and a portBase that determines the range of I/O ports used to interact with the drive.
            // The ATA primary channel typically starts at 0x1F0, and the secondary channel at 0x170.
            AdvancedTechnologyAttachment(bool master, common::uint16_t portBase);
            
            // Destructor (currently does not have specific tasks).
            ~AdvancedTechnologyAttachment();
            
            // Issues the IDENTIFY command to the drive. This typically retrieves
            // drive parameters (e.g., number of cylinders, heads, sectors, etc.).
            void Identify();
            
            // Reads data from a 28-bit LBA (Logical Block Address) using the Read sectors (28-bit) method.
            // sectorNum: The sector number on the disk to read from.
            // count: The number of bytes to read (usually 512 for one sector).
            void Read28(common::uint32_t sectorNum, int count = 512);

            // Writes data to a 28-bit LBA on the disk.
            // sectorNum: The sector number on the disk to write to.
            // data: Pointer to the buffer containing the data to write.
            // count: The number of bytes to write (e.g., 512 for one sector).
            void Write28(common::uint32_t sectorNum, common::uint8_t* data, common::uint32_t count);
            
            // Issues the FLUSH command to ensure any data cached in the drive’s buffer is written to disk.
            void Flush();
        };
        
    }
}

#endif  // __MYOS__DRIVERS__ATA_H
