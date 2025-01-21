#include <drivers/driver.h>

/*
 * We place all driver-related classes into the myos::drivers namespace. 
 * This helps to organize code and avoid naming conflicts.
 */
using namespace myos::drivers;

/*
 * ----------------------------------------------------------------------------
 * Driver Class Implementation
 * ----------------------------------------------------------------------------
 *
 * The Driver class is meant to serve as a generic interface or base class
 * for various hardware or software drivers within the system. 
 * Subclasses can override methods like Activate(), Reset(), and Deactivate().
 */

/*
 * Constructor:
 *  Currently empty, but it provides a base for initialization 
 *  in concrete driver subclasses.
 */
Driver::Driver()
{
}

/*
 * Destructor:
 *  Also empty in the base class; specific driver subclasses can clean up
 *  resources, memory, or hardware states in their destructors.
 */
Driver::~Driver()
{
}

/*
 * Activate:
 *  By default, this method does nothing. 
 *  In subclasses, it can be overridden to enable or initialize the driver 
 *  (e.g., registering interrupts, allocating buffers, etc.).
 */
void Driver::Activate()
{
}

/*
 * Reset:
 *  Returns an integer status (default 0). 
 *  Subclasses can override this to reset hardware or driver state.
 */
int Driver::Reset()
{
    return 0;
}

/*
 * Deactivate:
 *  Called to disable or shut down the driver’s functionality. 
 *  Base class is empty; subclasses can provide details.
 */
void Driver::Deactivate()
{
}




/*
 * ----------------------------------------------------------------------------
 * DriverManager Class Implementation
 * ----------------------------------------------------------------------------
 *
 * The DriverManager class maintains a list of Driver objects. 
 * It allows adding new drivers and activating all drivers at once.
 */

/*
 * Constructor:
 *  Initializes the driver counter (numDrivers) to 0. 
 *  The drivers array is sized at 265 in the header, though only 'numDrivers'
 *  are actually used at a given time.
 */
DriverManager::DriverManager()
{
    numDrivers = 0;
}

/*
 * AddDriver:
 *  Adds a new Driver pointer to the 'drivers' array, then increments 'numDrivers'.
 *  The caller is responsible for ensuring that 'numDrivers' does not exceed array bounds.
 */
void DriverManager::AddDriver(Driver* drv)
{
    drivers[numDrivers] = drv;
    numDrivers++;
}

/*
 * ActivateAll:
 *  Loops through all added drivers and calls each driver’s Activate() method.
 *  This provides a convenient way to initialize all registered drivers at once 
 *  (e.g., during OS startup).
 */
void DriverManager::ActivateAll()
{
    for(int i = 0; i < numDrivers; i++)
        drivers[i]->Activate();
}
