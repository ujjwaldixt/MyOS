#ifndef __MYOS__DRIVERS__DRIVER_H        // Header guard to prevent multiple inclusions of this file
#define __MYOS__DRIVERS__DRIVER_H

namespace myos
{
    namespace drivers
    {
        // The Driver class is a base class representing a generic hardware or software driver.
        // Derived classes can override Activate, Reset, and Deactivate to perform device-specific tasks.
        class Driver
        {
        public:
            // Constructor: Initializes the driver (base constructor does not do anything special).
            Driver();
            
            // Destructor: Cleans up driver resources if needed (base destructor does not do anything special).
            ~Driver();
            
            // Activate: Called to prepare the driver for use (e.g., enable interrupts, allocate memory, etc.).
            // The base version is empty and is meant to be overridden by subclasses.
            virtual void Activate();
            
            // Reset: Resets the driver/device to a known state.
            // Returns an integer status or error code. The base version does nothing.
            virtual int Reset();
            
            // Deactivate: Shuts down or disables the driver/device safely.
            // The base version is empty and is meant to be overridden by subclasses.
            virtual void Deactivate();
        };

        // The DriverManager class manages a collection of drivers, allowing them to be activated together.
        class DriverManager
        {
        public:
            // An array of pointers to Driver objects. 265 drivers is an arbitrary capacity.
            Driver* drivers[265];
            
            // The current number of drivers stored in the array.
            int numDrivers;
            
        public:
            // Constructor initializes the DriverManager with zero drivers.
            DriverManager();
            
            // Adds a driver to the internal driver list.
            // driver: a pointer to the Driver object to add.
            void AddDriver(Driver* driver);
            
            // Invokes the Activate method on all added drivers, enabling them for use.
            void ActivateAll();
        };
    }
}

#endif // __MYOS__DRIVERS__DRIVER_H
