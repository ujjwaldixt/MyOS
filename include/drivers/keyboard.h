#ifndef __MYOS__DRIVERS__KEYBOARD_H                 // Header guard to prevent multiple definitions
#define __MYOS__DRIVERS__KEYBOARD_H

#include <common/types.h>                            // Provides standard type aliases like uint8_t, uint32_t
#include <hardwarecommunication/interrupts.h>        // Allows handling hardware interrupts
#include <drivers/driver.h>                          // Base Driver class definition
#include <hardwarecommunication/port.h>              // I/O port abstractions

namespace myos
{
    namespace drivers
    {
        // The KeyboardEventHandler class serves as a base or interface for handling keyboard events.
        // It has two virtual methods: OnKeyDown and OnKeyUp, which can be overridden to define custom behavior.
        class KeyboardEventHandler
        {
        public:
            // Constructor initializes the event handler (empty by default).
            KeyboardEventHandler();

            // Called when a key is pressed. The char parameter contains the pressed key's character representation.
            virtual void OnKeyDown(char);

            // Called when a key is released. The char parameter contains the released key's character representation.
            virtual void OnKeyUp(char);
        };
        
        // The KeyboardDriver class extends both InterruptHandler and Driver to handle keyboard-related interrupts
        // and provide driver functionality. It uses two ports for communicating with the PS/2 controller:
        //   - dataport for reading scan codes from the keyboard
        //   - commandport for sending commands to the keyboard controller
        class KeyboardDriver : public myos::hardwarecommunication::InterruptHandler, 
                               public Driver
        {
            // Port used to read the data (scan code) sent from the keyboard.
            myos::hardwarecommunication::Port8Bit dataport;

            // Port used to send commands to the keyboard controller (e.g., to enable scanning).
            myos::hardwarecommunication::Port8Bit commandport;
            
            // Pointer to a KeyboardEventHandler that will process key events.
            KeyboardEventHandler* handler;

        public:
            // Constructor sets up the ports, registers the interrupt handler with the provided interrupt manager,
            // and stores a pointer to the event handler that will handle keyboard events.
            KeyboardDriver(myos::hardwarecommunication::InterruptManager* manager, KeyboardEventHandler *handler);
            
            // Destructor for cleanup (currently nothing specific).
            ~KeyboardDriver();
            
            // The HandleInterrupt method is called when the interrupt for the keyboard is triggered.
            // 'esp' is the current stack pointer, which can be changed and returned if needed. 
            // The method processes the scan code read from the keyboard and passes key events to the handler.
            virtual myos::common::uint32_t HandleInterrupt(myos::common::uint32_t esp);
            
            // Activate is called to enable the keyboard driver, typically enabling keyboard interrupts
            // and configuring the keyboard controller.
            virtual void Activate();
        };

    }
}

#endif  // __MYOS__DRIVERS__KEYBOARD_H
