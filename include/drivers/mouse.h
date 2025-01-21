#ifndef __MYOS__DRIVERS__MOUSE_H                     // Header guard to prevent multiple inclusion of this header
#define __MYOS__DRIVERS__MOUSE_H

#include <common/types.h>                            // Includes standard type definitions (uint8_t, uint32_t, etc.)
#include <hardwarecommunication/port.h>              // Abstraction for I/O port operations
#include <drivers/driver.h>                          // Base class for drivers (Driver)
#include <hardwarecommunication/interrupts.h>        // Interrupt handling mechanisms

namespace myos
{
    namespace drivers
    {
        // MouseEventHandler is an interface-like class to handle mouse events from the MouseDriver.
        // It defines virtual functions that can be overridden in a subclass to handle mouse activities.
        class MouseEventHandler
        {
        public:
            // Constructor, can initialize mouse event handling logic (empty by default).
            MouseEventHandler();

            // Called when the mouse driver is activated (e.g., when the mouse is recognized or enabled).
            virtual void OnActivate();

            // Called when a mouse button is pressed (e.g., button 0 = left, 1 = right, etc.).
            // 'button' indicates which button was pressed.
            virtual void OnMouseDown(myos::common::uint8_t button);

            // Called when a mouse button is released.
            // 'button' indicates which button was released.
            virtual void OnMouseUp(myos::common::uint8_t button);

            // Called when the mouse is moved. 
            // 'x' and 'y' represent the distance moved in the horizontal and vertical directions.
            virtual void OnMouseMove(int x, int y);
        };
        
        
        // The MouseDriver class extends both InterruptHandler and Driver.
        // It handles the low-level communication with the PS/2 mouse device.
        class MouseDriver : public myos::hardwarecommunication::InterruptHandler, 
                            public Driver
        {
            // Ports used for communication with the mouse through the PS/2 controller.
            myos::hardwarecommunication::Port8Bit dataport;    // Data port (commonly 0x60) to read mouse data (packets).
            myos::hardwarecommunication::Port8Bit commandport;  // Command port (commonly 0x64) for sending commands to the mouse/PS/2 controller.

            // The mouse sends data in packets of three bytes. We store them here in 'buffer'.
            // 'offset' indicates which of the three bytes we are currently processing.
            myos::common::uint8_t buffer[3];
            myos::common::uint8_t offset;

            // Stores the status of the mouse buttons.
            // Each bit in 'buttons' can represent a button’s current pressed/released state.
            myos::common::uint8_t buttons;

            // Pointer to a MouseEventHandler that processes high-level mouse events (clicks, movement).
            MouseEventHandler* handler;

        public:
            // Constructor registers the driver with the interrupt manager,
            // initializes the ports, and sets the event handler.
            MouseDriver(myos::hardwarecommunication::InterruptManager* manager, MouseEventHandler* handler);

            // Destructor for cleanup if needed (currently empty).
            ~MouseDriver();

            // Called by the interrupt manager when an IRQ for the mouse occurs (IRQ12).
            // 'esp' is the stack pointer; can be modified if needed.
            // Reads the mouse data packet from 'dataport' and processes it.
            // Returns the potentially updated stack pointer.
            virtual myos::common::uint32_t HandleInterrupt(myos::common::uint32_t esp);

            // Activates the driver: often sends commands to the mouse to start sending data,
            // enables interrupts, and calls OnActivate on the handler.
            virtual void Activate();
        };

    }
}

#endif  // __MYOS__DRIVERS__MOUSE_H
