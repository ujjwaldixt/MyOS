#include <common/types.h>
#include <gdt.h>
#include <memorymanagement.h>
#include <hardwarecommunication/interrupts.h>
#include <syscalls.h>
#include <hardwarecommunication/pci.h>
#include <drivers/driver.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/vga.h>
#include <drivers/ata.h>
#include <gui/desktop.h>
#include <gui/window.h>
#include <multitasking.h>

#include <drivers/amd_am79c973.h>
#include <net/etherframe.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/tcp.h>

// #define GRAPHICSMODE

using namespace myos;
using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;
using namespace myos::gui;
using namespace myos::net;

/*
 * printf:
 *  A simple implementation writing to VGA text mode memory at 0xb8000.
 *  It maintains cursor positions x, y, and handles line wrapping. 
 *  If the screen is filled, it scrolls up by clearing the screen.
 */
void printf(char* str)
{
    static uint16_t* VideoMemory = (uint16_t*)0xb8000;

    static uint8_t x=0,y=0;

    for(int i = 0; str[i] != '\0'; ++i)
    {
        switch(str[i])
        {
            case '\n':
                x = 0;
                y++;
                break;
            default:
                VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0xFF00) | str[i];
                x++;
                break;
        }

        if(x >= 80)
        {
            x = 0;
            y++;
        }

        if(y >= 25)
        {
            // clear the screen if we go past 25 lines
            for(y = 0; y < 25; y++)
                for(x = 0; x < 80; x++)
                    VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0xFF00) | ' ';
            x = 0;
            y = 0;
        }
    }
}

/*
 * printfHex:
 *  Prints a byte (uint8_t) in hexadecimal representation (two chars).
 */
void printfHex(uint8_t key)
{
    char* foo = "00";
    char* hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & 0xF];
    foo[1] = hex[key & 0xF];
    printf(foo);
}

/*
 * printfHex16:
 *  Prints a 16-bit value (two bytes) as hex by printing the upper byte and then the lower byte.
 */
void printfHex16(uint16_t key)
{
    printfHex((key >> 8) & 0xFF);
    printfHex( key & 0xFF);
}

/*
 * printfHex32:
 *  Prints a 32-bit value (four bytes) as hex by printing each byte in turn.
 */
void printfHex32(uint32_t key)
{
    printfHex((key >> 24) & 0xFF);
    printfHex((key >> 16) & 0xFF);
    printfHex((key >> 8) & 0xFF);
    printfHex( key & 0xFF);
}


/*
 * PrintfKeyboardEventHandler:
 *  Demonstrates a keyboard handler that simply prints the pressed character.
 */
class PrintfKeyboardEventHandler : public KeyboardEventHandler
{
public:
    void OnKeyDown(char c) override
    {
        char* foo = " ";
        foo[0] = c;
        printf(foo);
    }
};

/*
 * MouseToConsole:
 *  This mouse handler moves a visual indicator in the text mode console:
 *    - 'x' and 'y' track the cursor position on screen. 
 *    - The pixel (character) at the old position is toggled back, and the new position is toggled to show the mouse.
 */
class MouseToConsole : public MouseEventHandler
{
    int8_t x, y;
public:
    MouseToConsole()
    {
        uint16_t* VideoMemory = (uint16_t*)0xb8000;
        x = 40;
        y = 12;
        
        // Toggle the character colors at the initial position to indicate mouse presence
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);        
    }
    
    virtual void OnMouseMove(int xoffset, int yoffset) override
    {
        static uint16_t* VideoMemory = (uint16_t*)0xb8000;
        
        // Restore old position
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);

        // Adjust by offset
        x += xoffset;
        if(x >= 80) x = 79;
        if(x < 0) x = 0;

        y += yoffset;
        if(y >= 25) y = 24;
        if(y < 0) y = 0;

        // Toggle new position
        VideoMemory[80*y+x] = (VideoMemory[80*y+x] & 0x0F00) << 4
                            | (VideoMemory[80*y+x] & 0xF000) >> 4
                            | (VideoMemory[80*y+x] & 0x00FF);
    }
};

/*
 * PrintfUDPHandler:
 *  Handles incoming UDP datagrams by printing their contents as ASCII characters.
 */
class PrintfUDPHandler : public UserDatagramProtocolHandler
{
public:
    void HandleUserDatagramProtocolMessage(UserDatagramProtocolSocket* socket,
                                           common::uint8_t* data,
                                           common::uint16_t size) override
    {
        char* foo = " ";
        for(int i = 0; i < size; i++)
        {
            foo[0] = data[i];
            printf(foo);
        }
    }
};

/*
 * PrintfTCPHandler:
 *  Handles TCP data by printing incoming bytes and, if an HTTP GET request is found,
 *  sends a simple HTTP response, then closes the socket.
 */
class PrintfTCPHandler : public TransmissionControlProtocolHandler
{
public:
    bool HandleTransmissionControlProtocolMessage(TransmissionControlProtocolSocket* socket,
                                                  common::uint8_t* data,
                                                  common::uint16_t size) override
    {
        // Print all incoming data
        char* foo = " ";
        for(int i = 0; i < size; i++)
        {
            foo[0] = data[i];
            printf(foo);
        }
        
        // Check if data contains an HTTP GET request for "/"
        if(size > 9
           && data[0] == 'G'
           && data[1] == 'E'
           && data[2] == 'T'
           && data[3] == ' '
           && data[4] == '/'
           && data[5] == ' '
           && data[6] == 'H'
           && data[7] == 'T'
           && data[8] == 'T'
           && data[9] == 'P')
        {
            // Send HTTP/1.1 200 OK response
            socket->Send((uint8_t*)
                "HTTP/1.1 200 OK\r\n"
                "Server: MyOS\r\n"
                "Content-Type: text/html\r\n\r\n"
                "<html><head><title>My Operating System</title></head>"
                "<body><b>My Operating System</b> http://www.AlgorithMan.de</body></html>\r\n",
                184);
            socket->Disconnect();
        }
        
        return true;
    }
};

/*
 * sysprintf:
 *  Uses an interrupt (0x80) syscall to print a string.
 *  The kernel side of the syscall must handle 'a=4' as the print function.
 */
void sysprintf(char* str)
{
    asm("int $0x80" : : "a" (4), "b" (str));
}

/*
 * Simple tasks (taskA and taskB):
 *  Repeatedly print 'A' or 'B' using sysprintf. Demonstrates basic multitasking.
 */
void taskA()
{
    while(true)
        sysprintf("A");
}

void taskB()
{
    while(true)
        sysprintf("B");
}

/*
 * callConstructors:
 *  Called during early boot to invoke global C++ constructors in the kernel.
 */
typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;
extern "C" void callConstructors()
{
    for(constructor* i = &start_ctors; i != &end_ctors; i++)
        (*i)();
}

/*
 * kernelMain:
 *  The main entry point for our kernel after loading.
 *  - Initializes GDT, memory manager, interrupt manager, and drivers.
 *  - Sets up networking (Ethernet, ARP, IP, etc.).
 *  - Optionally runs in graphics mode with a GUI desktop and windows.
 *  - Enters an infinite loop to keep the OS running.
 */
extern "C" void kernelMain(const void* multiboot_structure, uint32_t /*multiboot_magic*/)
{
    printf("Hello World! --- http://www.AlgorithMan.de\n");

    // Initialize the Global Descriptor Table
    GlobalDescriptorTable gdt;
    
    /*
     * multiboot_structure contains memory info; we read it to get the upper mem size.
     * Then we reserve part of it as a heap managed by 'memoryManager'.
     */
    uint32_t* memupper = (uint32_t*)(((size_t)multiboot_structure) + 8);
    size_t heap = 10*1024*1024;  // 10 MB heap start
    MemoryManager memoryManager(heap, (*memupper)*1024 - heap - 10*1024);
    
    printf("heap: 0x");
    printfHex32(heap); // Print the address of the heap in hex
    
    // Allocate a small chunk of memory for demonstration
    void* allocated = memoryManager.malloc(1024);
    printf("\nallocated: 0x");
    printfHex32((size_t)allocated);
    printf("\n");
    
    /*
     * The TaskManager can schedule multiple tasks (taskA, taskB, etc.).
     * Currently, these tasks are commented out for demonstration, but
     * they show how we can add tasks to the manager.
     */
    TaskManager taskManager;
    // Task task1(&gdt, taskA);
    // Task task2(&gdt, taskB);
    // taskManager.AddTask(&task1);
    // taskManager.AddTask(&task2);
    
    // Set up interrupts with hardware offset 0x20 (for the PIC) and attach the taskManager
    InterruptManager interrupts(0x20, &gdt, &taskManager);
    
    // Syscall handler on interrupt 0x80
    SyscallHandler syscalls(&interrupts, 0x80);
    
    printf("Initializing Hardware, Stage 1\n");
    
    #ifdef GRAPHICSMODE
        // Create a GUI desktop if we are in graphic mode
        Desktop desktop(320,200, 0x00,0x00,0xA8);
    #endif
    
    // DriverManager allows us to register and activate multiple drivers
    DriverManager drvManager;
    
    // Keyboard setup
    #ifdef GRAPHICSMODE
        // If in graphics mode, pass keyboard events to the desktop
        KeyboardDriver keyboard(&interrupts, &desktop);
    #else
        // Otherwise, print characters to the console
        PrintfKeyboardEventHandler kbhandler;
        KeyboardDriver keyboard(&interrupts, &kbhandler);
    #endif
    drvManager.AddDriver(&keyboard);
    
    // Mouse setup
    #ifdef GRAPHICSMODE
        MouseDriver mouse(&interrupts, &desktop);
    #else
        MouseToConsole mousehandler;
        MouseDriver mouse(&interrupts, &mousehandler);
    #endif
    drvManager.AddDriver(&mouse);
    
    // PCI scanning: detect and set up drivers for PCI devices
    PeripheralComponentInterconnectController PCIController;
    PCIController.SelectDrivers(&drvManager, &interrupts);

    #ifdef GRAPHICSMODE
        // Provide a VGA driver if we are using graphics mode
        VideoGraphicsArray vga;
    #endif
    
    printf("Initializing Hardware, Stage 2\n");
    drvManager.ActivateAll();
        
    printf("Initializing Hardware, Stage 3\n");

    #ifdef GRAPHICSMODE
        // If in graphics mode, set 320x200x8, and add two small windows
        vga.SetMode(320,200,8);
        Window win1(&desktop, 10,10,20,20, 0xA8,0x00,0x00);
        desktop.AddChild(&win1);
        Window win2(&desktop, 40,15,30,30, 0x00,0xA8,0x00);
        desktop.AddChild(&win2);
    #endif

    /*
     * ATA driver examples:
     *  Uncomment to test identifying and reading/writing IDE disks.
     *
     *  AdvancedTechnologyAttachment ata0m(true, 0x1F0);  // Primary master
     *  ata0m.Identify();
     *  ata0m.Read28(...), ata0m.Write28(...), etc.
     */

    // Grab the AMD am79c973 (Ethernet) driver from the driver manager
    amd_am79c973* eth0 = (amd_am79c973*)(drvManager.drivers[2]);

    // Assign IP address 10.0.2.15 (in big-endian)
    uint8_t ip1 = 10, ip2 = 0, ip3 = 2, ip4 = 15;
    uint32_t ip_be = ((uint32_t)ip4 << 24)
                  | ((uint32_t)ip3 << 16)
                  | ((uint32_t)ip2 << 8)
                  |  (uint32_t)ip1;
    eth0->SetIPAddress(ip_be);

    // Create an EtherFrameProvider to send/receive raw Ethernet frames
    EtherFrameProvider etherframe(eth0);

    // ARP resolves IP <-> MAC addresses on the local network
    AddressResolutionProtocol arp(&etherframe);
    
    // Default gateway: 10.0.2.2
    uint8_t gip1 = 10, gip2 = 0, gip3 = 2, gip4 = 2;
    uint32_t gip_be = ((uint32_t)gip4 << 24)
                    | ((uint32_t)gip3 << 16)
                    | ((uint32_t)gip2 << 8)
                    |  (uint32_t)gip1;
    
    // Subnet mask: 255.255.255.0
    uint8_t subnet1 = 255, subnet2 = 255, subnet3 = 255, subnet4 = 0;
    uint32_t subnet_be = ((uint32_t)subnet4 << 24)
                       | ((uint32_t)subnet3 << 16)
                       | ((uint32_t)subnet2 << 8)
                       |  (uint32_t)subnet1;
                   
    // Provide IPv4 on top of EtherFrame + ARP. Pass gateway & subnet.
    InternetProtocolProvider ipv4(&etherframe, &arp, gip_be, subnet_be);
    
    // ICMP (for pings)
    InternetControlMessageProtocol icmp(&ipv4);

    // UDP support
    UserDatagramProtocolProvider udp(&ipv4);

    // TCP support
    TransmissionControlProtocolProvider tcp(&ipv4);
    
    // Enable interrupts
    interrupts.Activate();

    printf("\n\n\n\n");
    
    // Attempt to discover and cache the gateway's MAC via ARP
    arp.BroadcastMACAddress(gip_be);
    
    // Listen on TCP port 1234
    PrintfTCPHandler tcphandler;
    TransmissionControlProtocolSocket* tcpsocket = tcp.Listen(1234);
    tcp.Bind(tcpsocket, &tcphandler);

    // Main loop
    while(1)
    {
        #ifdef GRAPHICSMODE
            // Continuously redraw the desktop in graphics mode
            desktop.Draw(&vga);
        #endif
    }
}
