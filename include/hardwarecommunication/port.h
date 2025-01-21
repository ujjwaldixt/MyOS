#ifndef __MYOS__HARDWARECOMMUNICATION__PORT_H                 // Header guard to prevent multiple inclusions
#define __MYOS__HARDWARECOMMUNICATION__PORT_H

#include <common/types.h>                                     // Includes common type definitions (e.g., uint8_t, uint16_t, etc.)

namespace myos
{
    namespace hardwarecommunication
    {
        /*
         * Port:
         *  A base class for port operations (in/out) on the x86 platform. It stores the port number
         *  but does not provide read/write implementations by itself. Specialized classes (Port8Bit,
         *  Port16Bit, Port32Bit) derive from this class to actually perform the I/O.
         *
         *  NOTE: The destructor is not currently virtual because the kernel’s memory manager
         *  may not support certain features yet. In a more advanced environment, it should be virtual.
         */
        class Port
        {
        protected:
            // Initializes the port with a specific I/O port number.
            Port(myos::common::uint16_t portnumber);

            // Destructor for cleanup (currently non-virtual).
            ~Port();

            // The I/O port number (0–65535) associated with this port.
            myos::common::uint16_t portnumber;
        };


        /*
         * Port8Bit:
         *  Provides 8-bit read and write operations using inline assembly (inb/outb).
         *  These operations are the building blocks for hardware communication on many devices
         *  (e.g., keyboard controller, PIT, PIC).
         */
        class Port8Bit : public Port
        {
        public:
            // Constructor that assigns the I/O port number to this object.
            Port8Bit(myos::common::uint16_t portnumber);

            // Destructor (usually empty, because no dynamic allocation is done).
            ~Port8Bit();

            // Public read method, uses the static inline assembly function to return an 8-bit value from the port.
            virtual myos::common::uint8_t Read();

            // Public write method, uses the static inline assembly function to write an 8-bit value to the port.
            virtual void Write(myos::common::uint8_t data);

        protected:
            /*
             * Read8:
             *  Static inline function that executes the 'inb' instruction to read a byte from the specified port.
             *  The result is returned in 'result'. 
             */
            static inline myos::common::uint8_t Read8(myos::common::uint16_t _port)
            {
                myos::common::uint8_t result;
                __asm__ volatile("inb %1, %0" : "=a" (result) : "Nd" (_port));
                return result;
            }

            /*
             * Write8:
             *  Static inline function that executes the 'outb' instruction to write a byte to the specified port.
             */
            static inline void Write8(myos::common::uint16_t _port, myos::common::uint8_t _data)
            {
                __asm__ volatile("outb %0, %1" : : "a" (_data), "Nd" (_port));
            }
        };


        /*
         * Port8BitSlow:
         *  This class behaves like Port8Bit but adds a small delay after writing. Some hardware
         *  controllers (e.g., older hardware or certain chipsets) require a delay to allow the operation
         *  to settle before subsequent operations occur.
         */
        class Port8BitSlow : public Port8Bit
        {
        public:
            // Constructor sets the port number, same as Port8Bit but uses the slower write.
            Port8BitSlow(myos::common::uint16_t portnumber);

            // Destructor (empty, same reason as other port classes).
            ~Port8BitSlow();

            // Overridden Write method inserts a small delay using a pair of jump instructions after outb.
            virtual void Write(myos::common::uint8_t data);

        protected:
            /*
             * Write8Slow:
             *  Similar to Write8, but includes two jump instructions to force a short delay before continuing.
             *  This can help older or sensitive hardware properly register the output.
             */
            static inline void Write8Slow(myos::common::uint16_t _port, myos::common::uint8_t _data)
            {
                __asm__ volatile("outb %0, %1\n"
                                 "jmp 1f\n1: jmp 1f\n1:"
                                 : : "a" (_data), "Nd" (_port));
            }

        };


        /*
         * Port16Bit:
         *  Provides 16-bit read and write operations using 'inw' and 'outw'. 
         *  Useful for devices that communicate using 16-bit registers (e.g., certain older network cards).
         */
        class Port16Bit : public Port
        {
        public:
            // Constructor assigning the 16-bit port number.
            Port16Bit(myos::common::uint16_t portnumber);

            // Destructor (empty).
            ~Port16Bit();

            // Reads a 16-bit (2-byte) value from this port.
            virtual myos::common::uint16_t Read();

            // Writes a 16-bit value to this port.
            virtual void Write(myos::common::uint16_t data);

        protected:
            /*
             * Read16:
             *  Executes 'inw' to read 16 bits from the specified port into 'result'.
             */
            static inline myos::common::uint16_t Read16(myos::common::uint16_t _port)
            {
                myos::common::uint16_t result;
                __asm__ volatile("inw %1, %0" : "=a" (result) : "Nd" (_port));
                return result;
            }

            /*
             * Write16:
             *  Executes 'outw' to write 16 bits from '_data' to '_port'.
             */
            static inline void Write16(myos::common::uint16_t _port, myos::common::uint16_t _data)
            {
                __asm__ volatile("outw %0, %1" : : "a" (_data), "Nd" (_port));
            }
        };


        /*
         * Port32Bit:
         *  Provides 32-bit (4-byte) read and write operations using 'inl' and 'outl'.
         *  This is used for devices or operations requiring a full 32-bit register transfer 
         *  (e.g., some PCI operations, video registers).
         */
        class Port32Bit : public Port
        {
        public:
            // Constructor that sets the port number for 32-bit operations.
            Port32Bit(myos::common::uint16_t portnumber);

            // Destructor (empty).
            ~Port32Bit();

            // Reads a 32-bit value from this port (e.g., out of PCI config space).
            virtual myos::common::uint32_t Read();

            // Writes a 32-bit value to this port.
            virtual void Write(myos::common::uint32_t data);

        protected:
            /*
             * Read32:
             *  Uses 'inl' to read a 32-bit value from the specified port.
             */
            static inline myos::common::uint32_t Read32(myos::common::uint16_t _port)
            {
                myos::common::uint32_t result;
                __asm__ volatile("inl %1, %0" : "=a" (result) : "Nd" (_port));
                return result;
            }

            /*
             * Write32:
             *  Uses 'outl' to write a 32-bit value to the specified port.
             */
            static inline void Write32(myos::common::uint16_t _port, myos::common::uint32_t _data)
            {
                __asm__ volatile("outl %0, %1" : : "a"(_data), "Nd" (_port));
            }
        };

    }
}

#endif // __MYOS__HARDWARECOMMUNICATION__PORT_H
