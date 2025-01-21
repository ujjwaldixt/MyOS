#include <drivers/mouse.h>

/*
 * Using namespaces from the OS codebase:
 *   - myos::common: for integral types like uint8_t, int32_t, etc.
 *   - myos::drivers: for driver-related classes (MouseDriver, MouseEventHandler)
 *   - myos::hardwarecommunication: for port I/O and interrupt handling
 */
using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;

/*
 * A simple function for string output (provided externally).
 */
extern void printf(char*);


/*
 * --------------------------------------------------------------------------
 * MouseEventHandler Class
 * --------------------------------------------------------------------------
 *
 * Provides a high-level interface for handling mouse events such as activation,
 * button presses, and mouse movements. By default, all methods are empty
 * and meant to be overridden as needed.
 */

/*
 * Constructor: Initializes any internal state if necessary (currently empty).
 */
MouseEventHandler::MouseEventHandler()
{
}

/*
 * OnActivate:
 *  Called when the mouse driver is activated (i.e., when the mouse is recognized/enabled).
 *  By default, does nothing; can be overridden by a subclass to track activation.
 */
void MouseEventHandler::OnActivate()
{
}

/*
 * OnMouseDown:
 *  Called when a mouse button is pressed. 'button' indicates which button (1=left, 2=right, 3=middle).
 *  Default implementation is empty.
 */
void MouseEventHandler::OnMouseDown(uint8_t button)
{
}

/*
 * OnMouseUp:
 *  Called when a mouse button is released. 'button' indicates which button (1=left, 2=right, 3=middle).
 *  Default implementation is empty.
 */
void MouseEventHandler::OnMouseUp(uint8_t button)
{
}

/*
 * OnMouseMove:
 *  Called when the mouse is moved. 'x' and 'y' represent the signed displacement in each axis.
 *  A negative value for 'y' moves up on most OS coordinate systems. Here, the driver inverts 'y'.
 */
void MouseEventHandler::OnMouseMove(int x, int y)
{
}


/*
 * --------------------------------------------------------------------------
 * MouseDriver Class
 * --------------------------------------------------------------------------
 *
 * The MouseDriver interacts directly with the PS/2 mouse hardware through the ports:
 *   - dataport (0x60): for reading/writing mouse data bytes
 *   - commandport (0x64): for sending commands to the PS/2 controller
 *
 * It inherits from InterruptHandler to manage IRQ12 (interrupt 0x2C), which the
 * PS/2 mouse triggers whenever there is movement or button changes.
 */

/*
 * Constructor:
 *   - manager: The system's interrupt manager (to register this driver for IRQ12).
 *   - handler: The MouseEventHandler to delegate mouse events to.
 * Initializes the I/O ports for the data (0x60) and command (0x64).
 */
MouseDriver::MouseDriver(InterruptManager* manager, MouseEventHandler* handler)
    : InterruptHandler(manager, 0x2C),  // 0x2C = IRQ12 after PIC remapping
      dataport(0x60),
      commandport(0x64)
{
    this->handler = handler;
}

/*
 * Destructor: Frees any resources if necessary (currently empty).
 */
MouseDriver::~MouseDriver()
{
}

/*
 * Activate:
 *   - Initializes mouse-related states, including 'offset' (for 3-byte packets) and 'buttons'.
 *   - If there's a MouseEventHandler, calls OnActivate().
 *   - Sends commands to the PS/2 controller to enable the mouse:
 *       0xA8: enable auxiliary mouse device.
 *       0x20: read current controller command byte.
 *     Then modifies the command byte to enable IRQ12 and writes it back:
 *       0x60: set command byte.
 *       0xD4/0xF4: send "enable data reporting" command to the mouse itself.
 *   - Finally, an extra read is done to clear any residual input.
 */
void MouseDriver::Activate()
{
    offset = 0;
    buttons = 0;

    if(handler != 0)
        handler->OnActivate();
    
    // 0xA8 = enable PS/2 mouse device
    commandport.Write(0xA8);
    // 0x20 = read controller command byte
    commandport.Write(0x20);

    // Combine the read data with bitmask to enable IRQ12 and possibly other bits
    uint8_t status = dataport.Read() | 2;

    // 0x60 = write controller command byte
    commandport.Write(0x60);
    dataport.Write(status);

    // 0xD4 instructs the PS/2 controller that the next byte is for the mouse
    commandport.Write(0xD4);
    // 0xF4 = enable data reporting from the mouse
    dataport.Write(0xF4);

    // Read once more to flush any leftover data
    dataport.Read();
}

/*
 * HandleInterrupt:
 *   - Called by the interrupt manager when IRQ12 fires (PS/2 mouse event).
 *   - Reads the status byte from commandport (0x64). If the 5th bit (0x20) isn't set,
 *     the data isn't from the mouse, so returns immediately.
 *   - Reads one byte of mouse data from dataport (0x60), storing it in 'buffer'.
 *   - Every 3 bytes, it interprets the packet:
 *       buffer[0] = buttons state
 *       buffer[1] = X displacement
 *       buffer[2] = Y displacement
 *     The driver calls OnMouseMove if X/Y != 0, adjusting Y sign as negative.
 *     It also detects changes in button states (left, right, middle) and
 *     calls OnMouseDown/OnMouseUp appropriately.
 *   - Returns the stack pointer unchanged.
 */
uint32_t MouseDriver::HandleInterrupt(uint32_t esp)
{
    // Read status from command port to check if it's a mouse event
    uint8_t status = commandport.Read();
    if (!(status & 0x20))  // bit 5 indicates mouse data
        return esp;

    // Read the next byte of mouse data
    buffer[offset] = dataport.Read();

    // If there's no handler, nothing to do
    if(handler == 0)
        return esp;
    
    // Move to the next byte in the 3-byte cycle
    offset = (offset + 1) % 3;

    // Once we have all 3 bytes, interpret them
    if(offset == 0)
    {
        // If X or Y displacement is non-zero, report movement.
        if(buffer[1] != 0 || buffer[2] != 0)
        {
            // buffer[1] is X move, buffer[2] is Y move (inverted)
            handler->OnMouseMove((int8_t)buffer[1], -((int8_t)buffer[2]));
        }

        // Check each of the three mouse buttons (bits 0,1,2 in buffer[0])
        for(uint8_t i = 0; i < 3; i++)
        {
            // Compare current button bit with previous state
            if((buffer[0] & (0x1 << i)) != (buttons & (0x1 << i)))
            {
                // If previously set, now cleared => MouseUp event
                if(buttons & (0x1 << i))
                    handler->OnMouseUp(i + 1);
                else
                    // If previously clear, now set => MouseDown event
                    handler->OnMouseDown(i + 1);
            }
        }
        // Update buttons state
        buttons = buffer[0];
    }
    
    return esp;
}
