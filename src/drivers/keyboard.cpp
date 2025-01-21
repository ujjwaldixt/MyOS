#include <drivers/keyboard.h>

/*
 * Using the namespaces from the operating system:
 *   - myos::common: fundamental types (uint8_t, uint32_t, etc.)
 *   - myos::drivers: driver classes and interfaces
 *   - myos::hardwarecommunication: interrupt and port I/O classes
 */
using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;


/*
 * ----------------------------------------------------------------------------
 * KeyboardEventHandler Class 
 * ----------------------------------------------------------------------------
 *
 * This class provides a basic interface for handling keyboard events (key up/down).
 * By default, it has empty methods for OnKeyDown and OnKeyUp, which can be overridden
 * by derived classes to implement custom keyboard handling logic (e.g., text input).
 */

/*
 * Constructor for KeyboardEventHandler. 
 * The base implementation is empty; it can be overridden or extended by subclasses.
 */
KeyboardEventHandler::KeyboardEventHandler()
{
}

/*
 * OnKeyDown:
 *   Triggered when a key is pressed (scan code in the 0..0x7F range).
 *   The parameter is the ASCII character for the pressed key, if available.
 *   By default, does nothing.
 */
void KeyboardEventHandler::OnKeyDown(char)
{
}

/*
 * OnKeyUp:
 *   Triggered when a key is released (scan code in the 0x80..0xFF range).
 *   By default, does nothing.
 */
void KeyboardEventHandler::OnKeyUp(char)
{
}


/*
 * ----------------------------------------------------------------------------
 * KeyboardDriver Class 
 * ----------------------------------------------------------------------------
 *
 * Handles the low-level communication with the PS/2 keyboard controller by:
 *   - Reading scancodes from the data port (0x60).
 *   - Sending commands to the command port (0x64).
 *   - Translating scancodes into ASCII characters (for selected keys).
 *   - Notifying a KeyboardEventHandler about key presses.
 *
 * The keyboard typically raises IRQ1 (interrupt 0x21 on the PIC).
 */

/*
 * Constructor:
 *   - manager: The InterruptManager responsible for handling IRQs.
 *   - handler: An object implementing the KeyboardEventHandler interface to receive key events.
 * It initializes I/O ports for data (0x60) and command (0x64) and registers this driver at interrupt 0x21.
 */
KeyboardDriver::KeyboardDriver(InterruptManager* manager, KeyboardEventHandler* handler)
: InterruptHandler(manager, 0x21),  // IRQ1 for PS/2 keyboard
  dataport(0x60),
  commandport(0x64)
{
    this->handler = handler;
}

/*
 * Destructor: 
 * Currently empty, but could be used for cleanup if necessary.
 */
KeyboardDriver::~KeyboardDriver()
{
}

/*
 * External C-style functions for printing:
 *   - printf: prints a null-terminated string to the screen.
 *   - printfHex: prints a byte as two hexadecimal digits.
 */
extern void printf(char*);
extern void printfHex(uint8_t);

/*
 * Activate:
 *   - Clears any leftover data in the keyboard buffer by reading from the data port while 
 *     the command port's status indicates data is available.
 *   - Sends commands to the PS/2 controller:
 *       0xAE: activate keyboard interrupts
 *       0x20: read controller command byte
 *   - Modifies the command byte to enable the keyboard (sets bit 0), disables mouse clock if bit 4 is set.
 *   - Writes the updated command byte back with 0x60.
 *   - Finally, sends 0xF4 to the keyboard to enable scanning.
 */
void KeyboardDriver::Activate()
{
    // Flush any residual data from the keyboard buffer
    while(commandport.Read() & 0x1)
        dataport.Read();
    
    // 0xAE = enable (activate) keyboard interrupts
    commandport.Write(0xae);

    // 0x20 = read controller command byte, which we'll modify
    commandport.Write(0x20);
    uint8_t status = (dataport.Read() | 1) & ~0x10; // Set bit 0, clear bit 4

    // 0x60 = write controller command byte
    commandport.Write(0x60);
    dataport.Write(status);

    // 0xF4 = enable scanning on the keyboard
    dataport.Write(0xf4);
}

/*
 * HandleInterrupt:
 *   - Called when IRQ1 (keyboard interrupt) is triggered.
 *   - Reads the scancode from the data port (0x60). 
 *   - If there's a valid handler and the scancode is less than 0x80 (a key press, not release),
 *     it translates the scancode to an ASCII character for certain keys and calls OnKeyDown.
 *   - If the key is not recognized, prints "KEYBOARD 0x.." with the scancode in hex.
 *   - Returns the stack pointer (esp) unchanged.
 */
uint32_t KeyboardDriver::HandleInterrupt(uint32_t esp)
{
    // Read scancode
    uint8_t key = dataport.Read();
    
    // If no event handler is set, there's nothing to do
    if(handler == 0)
        return esp;
    
    // Only handle key-down events (scancodes < 0x80)
    if(key < 0x80)
    {
        switch(key)
        {
            // Numeric keys above QWERTY row
            case 0x02: handler->OnKeyDown('1'); break;
            case 0x03: handler->OnKeyDown('2'); break;
            case 0x04: handler->OnKeyDown('3'); break;
            case 0x05: handler->OnKeyDown('4'); break;
            case 0x06: handler->OnKeyDown('5'); break;
            case 0x07: handler->OnKeyDown('6'); break;
            case 0x08: handler->OnKeyDown('7'); break;
            case 0x09: handler->OnKeyDown('8'); break;
            case 0x0A: handler->OnKeyDown('9'); break;
            case 0x0B: handler->OnKeyDown('0'); break;

            // QWERT row
            case 0x10: handler->OnKeyDown('q'); break;
            case 0x11: handler->OnKeyDown('w'); break;
            case 0x12: handler->OnKeyDown('e'); break;
            case 0x13: handler->OnKeyDown('r'); break;
            case 0x14: handler->OnKeyDown('t'); break;
            case 0x15: handler->OnKeyDown('z'); break; // Key 0x15 is 'z' on some layouts (QWERTZ)
            case 0x16: handler->OnKeyDown('u'); break;
            case 0x17: handler->OnKeyDown('i'); break;
            case 0x18: handler->OnKeyDown('o'); break;
            case 0x19: handler->OnKeyDown('p'); break;

            // ASDF row
            case 0x1E: handler->OnKeyDown('a'); break;
            case 0x1F: handler->OnKeyDown('s'); break;
            case 0x20: handler->OnKeyDown('d'); break;
            case 0x21: handler->OnKeyDown('f'); break;
            case 0x22: handler->OnKeyDown('g'); break;
            case 0x23: handler->OnKeyDown('h'); break;
            case 0x24: handler->OnKeyDown('j'); break;
            case 0x25: handler->OnKeyDown('k'); break;
            case 0x26: handler->OnKeyDown('l'); break;

            // ZXCV row
            case 0x2C: handler->OnKeyDown('y'); break;
            case 0x2D: handler->OnKeyDown('x'); break;
            case 0x2E: handler->OnKeyDown('c'); break;
            case 0x2F: handler->OnKeyDown('v'); break;
            case 0x30: handler->OnKeyDown('b'); break;
            case 0x31: handler->OnKeyDown('n'); break;
            case 0x32: handler->OnKeyDown('m'); break;
            case 0x33: handler->OnKeyDown(','); break;
            case 0x34: handler->OnKeyDown('.'); break;
            case 0x35: handler->OnKeyDown('-'); break;

            // Enter (0x1C) and Space (0x39)
            case 0x1C: handler->OnKeyDown('\n'); break;
            case 0x39: handler->OnKeyDown(' ');  break;

            // Default case: scancode not mapped here
            default:
            {
                printf("KEYBOARD 0x");
                printfHex(key);
                break;
            }
        }
    }
    // Return the (potentially) unchanged stack pointer after handling
    return esp;
}
