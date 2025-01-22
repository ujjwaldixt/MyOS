#include <drivers/ata.h>

/*
 * Use of namespaces:
 *   - myos is the root namespace for the operating system.
 *   - myos::common provides basic type definitions (uint8_t, uint16_t, etc.).
 *   - myos::drivers houses driver-related classes, including ATA driver here.
 */
using namespace myos;
using namespace myos::common;
using namespace myos::drivers;

/*
 * External functions for printing:
 *   - printf: prints a string.
 *   - printfHex: prints a byte in hexadecimal format.
 * These are commonly defined elsewhere in the OS (e.g., in a console or debugging library).
 */
extern void printf(char* str);
extern void printfHex(uint8_t);


/*
 * AdvancedTechnologyAttachment class implements an ATA (IDE) driver for reading/writing data 
 * from/to an ATA hard disk or similar storage device using 28-bit addressing.
 */
 
/*
 * Constructor:
 *   - master: boolean indicating if the drive is the master or slave on this ATA channel.
 *   - portBase: the base I/O port for the ATA channel (e.g., 0x1F0 for primary, 0x170 for secondary).
 *   It initializes I/O port objects for data, error, sector count, LBA registers, and command/control.
 */
AdvancedTechnologyAttachment::AdvancedTechnologyAttachment(bool master, common::uint16_t portBase)
:   dataPort(portBase),
    errorPort(portBase + 0x1),
    sectorCountPort(portBase + 0x2),
    lbaLowPort(portBase + 0x3),
    lbaMidPort(portBase + 0x4),
    lbaHiPort(portBase + 0x5),
    devicePort(portBase + 0x6),
    commandPort(portBase + 0x7),
    controlPort(portBase + 0x206)
{
    this->master = master;
}

/*
 * Destructor:
 *   - Currently empty, but could be used for shutting down the drive or freeing resources.
 */
AdvancedTechnologyAttachment::~AdvancedTechnologyAttachment()
{
}
            
/*
 * Identify:
 *   - Issues the 'IDENTIFY' command (0xEC) to the drive to retrieve information such as 
 *     model number, serial number, supported features, etc.
 *   - First, it selects the drive (master or slave), then issues the IDENTIFY command.
 *   - If the drive is present and supports IDENTIFY, we read 256 words (512 bytes) from the data port.
 *   - Each word is printed out as two characters (since the model/serial info is often stored as ASCII).
 *   - If there's an error (status=0x01), it prints "ERROR".
 */
void AdvancedTechnologyAttachment::Identify()
{
    // Select the drive (0xA0 for master, 0xB0 for slave) and reset any control flags
    devicePort.Write(master ? 0xA0 : 0xB0);
    controlPort.Write(0);
    
    // Read status to check if drive exists (0xFF = no drive)
    devicePort.Write(0xA0);
    uint8_t status = commandPort.Read();
    if(status == 0xFF)
        return;
    
    // Prepare the drive for IDENTIFY
    devicePort.Write(master ? 0xA0 : 0xB0);
    sectorCountPort.Write(0);
    lbaLowPort.Write(0);
    lbaMidPort.Write(0);
    lbaHiPort.Write(0);
    
    // Issue the IDENTIFY command (0xEC)
    commandPort.Write(0xEC);
    
    // If the drive returns 0x00, it's not present or doesn't respond
    status = commandPort.Read();
    if(status == 0x00)
        return;
    
    // Wait until BSY clears (bit 7 = 0) and DRQ sets or ERR sets (bit 0 = 1 means error).
    while(((status & 0x80) == 0x80)  // BSY = 1
       && ((status & 0x01) != 0x01)) // ERR = 0
    {
        status = commandPort.Read();
    }
        
    // If ERR is set (bit 0), print "ERROR"
    if(status & 0x01)
    {
        printf("ERROR");
        return;
    }
    
    // Read 256 words of drive info
    for(int i = 0; i < 256; i++)
    {
        uint16_t data = dataPort.Read();
        
        // Each word is 2 bytes; interpret them as ASCII.
        char text[3] = "  ";  // temporary buffer
        text[0] = (data >> 8) & 0xFF;  // high byte
        text[1] = data & 0xFF;        // low byte
        
        // Print the two characters
        printf(text);
    }
    printf("\n");
}

/*
 * Read28:
 *   - Reads a single 512-byte sector from the drive using 28-bit LBA.
 *   - sectorNum: The sector index (0..0x0FFFFFFF).
 *   - count: Number of bytes to read from that sector (<= 512).
 *   - This code reads from the data port in 16-bit words and prints them out as text.
 *   - The remainder of the 512-byte sector is read (and discarded) if 'count' < 512.
 */
void AdvancedTechnologyAttachment::Read28(common::uint32_t sectorNum, int count)
{
    // 28-bit LBA max
    if(sectorNum > 0x0FFFFFFF)
        return;
    
    // Select drive with LBA bits
    devicePort.Write( (master ? 0xE0 : 0xF0) 
                     | ((sectorNum & 0x0F000000) >> 24) );
    
    // Reset error register
    errorPort.Write(0);
    
    // We want to read 1 sector
    sectorCountPort.Write(1);
    
    // Write the low/mid/high parts of the LBA
    lbaLowPort.Write(  sectorNum & 0x000000FF );
    lbaMidPort.Write( (sectorNum & 0x0000FF00) >> 8 );
    lbaLowPort.Write( (sectorNum & 0x00FF0000) >> 16 );

    // Issue 'READ SECTORS' command (0x20)
    commandPort.Write(0x20);
    
    // Wait for the drive to be ready (BSY=0, ERR=0)
    uint8_t status = commandPort.Read();
    while(((status & 0x80) == 0x80)  // BSY=1
       && ((status & 0x01) != 0x01)) // ERR=0
    {
        status = commandPort.Read();
    }
        
    if(status & 0x01) // Error
    {
        printf("ERROR");
        return;
    }
    
    
    printf("Reading ATA Drive: ");
    
    // Read 'count' bytes (in words) from the data port
    for(int i = 0; i < count; i += 2)
    {
        uint16_t wdata = dataPort.Read();
        
        // Convert the 16-bit word into two characters
        char text[3] = "  ";
        text[0] = wdata & 0xFF;
        
        if(i+1 < count)
            text[1] = (wdata >> 8) & 0xFF;
        else
            text[1] = '\0'; // no second char if count is odd
        
        printf(text);
    }    
    
    // If we haven't read the full 512 bytes, we need to discard the remainder
    // so the drive's internal pointer is at the next sector boundary.
    for(int i = count + (count%2); i < 512; i += 2)
        dataPort.Read();
}

/*
 * Write28:
 *   - Writes a single 512-byte sector using 28-bit LBA.
 *   - sectorNum: The sector index to write.
 *   - data: Pointer to the bytes to be written.
 *   - count: Number of bytes to write (<= 512).
 *   - If less than 512 bytes are written, the remainder of the sector is filled with 0x00.
 */
void AdvancedTechnologyAttachment::Write28(common::uint32_t sectorNum, common::uint8_t* data, common::uint32_t count)
{
    // Validate LBA range and size
    if(sectorNum > 0x0FFFFFFF)
        return;
    if(count > 512)
        return;
    
    // Select drive and address
    devicePort.Write( (master ? 0xE0 : 0xF0)
                     | ((sectorNum & 0x0F000000) >> 24) );
                     
    errorPort.Write(0);
    sectorCountPort.Write(1);

    // LBA low/mid/high
    lbaLowPort.Write(  sectorNum & 0x000000FF );
    lbaMidPort.Write( (sectorNum & 0x0000FF00) >> 8 );
    lbaLowPort.Write( (sectorNum & 0x00FF0000) >> 16 );

    // Write command (0x30 = WRITE SECTORS)
    commandPort.Write(0x30);
    
    printf("Writing to ATA Drive: ");

    // Write the 'count' bytes, two bytes at a time
    for(int i = 0; i < (int)count; i += 2)
    {
        uint16_t wdata = data[i];
        if(i+1 < (int)count)
            wdata |= ((uint16_t)data[i+1]) << 8;
        
        dataPort.Write(wdata);
        
        // Print what we wrote
        char text[3] = "  ";
        text[0] = (wdata >> 8) & 0xFF;
        text[1] = wdata & 0xFF;
        printf(text);
    }
    
    // If the buffer is shorter than 512 bytes, pad the rest of the sector with 0.
    for(int i = count + (count%2); i < 512; i += 2)
        dataPort.Write(0x0000);
}

/*
 * Flush:
 *   - Issues the 'FLUSH CACHE' command (0xE7) to ensure any data cached by the drive is written to disk.
 *   - Waits for BSY=0 and checks ERR. If an error is encountered, prints "ERROR".
 */
void AdvancedTechnologyAttachment::Flush()
{
    // Select drive
    devicePort.Write( master ? 0xE0 : 0xF0 );
    
    // FLUSH CACHE command
    commandPort.Write(0xE7);

    uint8_t status = commandPort.Read();
    if(status == 0x00)
        return;
    
    // Wait until BSY=0 or ERR=1
    while(((status & 0x80) == 0x80)  // BSY=1
       && ((status & 0x01) != 0x01)) // ERR=0
    {
        status = commandPort.Read();
    }
        
    if(status & 0x01) // ERR
    {
        printf("ERROR");
        return;
    }
}
