#include <hardwarecommunication/port.h>

/*
 * Using namespaces:
 *   - myos::common contains basic type definitions (e.g., uint8_t, uint16_t, uint32_t)
 *   - myos::hardwarecommunication contains classes for low-level I/O (port operations)
 */
using namespace myos::common;
using namespace myos::hardwarecommunication;


/*
 * ----------------------------------------------------------------------------
 * Port Base Class
 * ----------------------------------------------------------------------------
 *
 * This is the base class for all port I/O operations. It holds the port number,
 * which represents an I/O port address in the x86 architecture. The derived classes
 * implement specific word-length operations (8-bit, 16-bit, 32-bit).
 *
 * Note: The destructor is not declared virtual because memory management in this
 * simple kernel might not support full C++ polymorphism yet.
 */

/*
 * Constructor: Initializes the port with the given I/O port number.
 */
Port::Port(uint16_t portnumber)
{
    this->portnumber = portnumber;
}

/*
 * Destructor: Currently empty. If there were any allocated resources, they would be freed here.
 */
Port::~Port()
{
}


/*
 * ----------------------------------------------------------------------------
 * Port8Bit Class
 * ----------------------------------------------------------------------------
 *
 * Derived from Port, this class implements 8-bit I/O operations.
 * It uses inline assembly functions (Read8 and Write8) to perform port I/O.
 */

/*
 * Constructor: Passes the port number to the base Port constructor.
 */
Port8Bit::Port8Bit(uint16_t portnumber)
    : Port(portnumber)
{
}

/*
 * Destructor: Nothing special to clean up.
 */
Port8Bit::~Port8Bit()
{
}

/*
 * Write:
 *  - Writes an 8-bit value to the I/O port.
 *  - Calls the static inline function Write8, passing the stored port number and the data.
 */
void Port8Bit::Write(uint8_t data)
{
    Write8(portnumber, data);
}

/*
 * Read:
 *  - Reads an 8-bit value from the I/O port.
 *  - Calls the static inline function Read8 with the stored port number.
 */
uint8_t Port8Bit::Read()
{
    return Read8(portnumber);
}


/*
 * ----------------------------------------------------------------------------
 * Port8BitSlow Class
 * ----------------------------------------------------------------------------
 *
 * Derived from Port8Bit, this version implements a slower write,
 * adding a deliberate delay using jump instructions. Some hardware devices
 * require extra time between successive I/O operations.
 */

/*
 * Constructor: Calls the Port8Bit constructor with the given port number.
 */
Port8BitSlow::Port8BitSlow(uint16_t portnumber)
    : Port8Bit(portnumber)
{
}

/*
 * Destructor: No additional clean-up required.
 */
Port8BitSlow::~Port8BitSlow()
{
}

/*
 * Write:
 *  - Uses the slow version of the write operation.
 *  - Calls the static inline function Write8Slow with the port number and data.
 */
void Port8BitSlow::Write(uint8_t data)
{
    Write8Slow(portnumber, data);
}


/*
 * ----------------------------------------------------------------------------
 * Port16Bit Class
 * ----------------------------------------------------------------------------
 *
 * Derived directly from Port, this class implements 16-bit I/O operations.
 * It uses inline assembly functions (Read16 and Write16) to perform the operations.
 */

/*
 * Constructor: Initializes the Port16Bit with the given port number.
 */
Port16Bit::Port16Bit(uint16_t portnumber)
    : Port(portnumber)
{
}

/*
 * Destructor: Empty as no specific resource deallocation is needed.
 */
Port16Bit::~Port16Bit()
{
}

/*
 * Write:
 *  - Writes a 16-bit value to the specified I/O port using Write16.
 */
void Port16Bit::Write(uint16_t data)
{
    Write16(portnumber, data);
}

/*
 * Read:
 *  - Reads a 16-bit value from the specified I/O port using Read16.
 */
uint16_t Port16Bit::Read()
{
    return Read16(portnumber);
}


/*
 * ----------------------------------------------------------------------------
 * Port32Bit Class
 * ----------------------------------------------------------------------------
 *
 * Derived from Port, this class provides 32-bit I/O operations.
 * It uses the inline assembly functions Read32 and Write32 for these operations.
 */

/*
 * Constructor: Simply passes the port number to the base Port class.
 */
Port32Bit::Port32Bit(uint16_t portnumber)
    : Port(portnumber)
{
}

/*
 * Destructor: Does not need to perform any specific cleanup.
 */
Port32Bit::~Port32Bit()
{
}

/*
 * Write:
 *  - Writes a 32-bit value to the I/O port using Write32.
 */
void Port32Bit::Write(uint32_t data)
{
    Write32(portnumber, data);
}

/*
 * Read:
 *  - Reads a 32-bit value from the I/O port using Read32.
 */
uint32_t Port32Bit::Read()
{
    return Read32(portnumber);
}
