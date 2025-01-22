#include <net/etherframe.h>
using namespace myos;
using namespace myos::common;
using namespace myos::net;
using namespace myos::drivers;

/*
 * ----------------------------------------------------------------------------
 * EtherFrameHandler Class
 * ----------------------------------------------------------------------------
 *
 * This class provides an abstraction for handling Ethernet frames for a
 * specific EtherType. It inherits from no base class here (other than indirectly
 * from C++’s object model) and is associated with an EtherFrameProvider backend
 * that supplies the actual data transmission functionality.
 *
 * The EtherType is stored in big-endian format (network byte order).
 */

/*
 * Constructor:
 *   - backend: pointer to an EtherFrameProvider that is responsible for low-level Ethernet
 *              frame transmission and reception.
 *   - etherType: the specific EtherType (e.g., 0x0800 for IPv4, 0x0806 for ARP) that this
 *                handler will process.
 *
 * In the constructor, the provided etherType is converted from little-endian to big-endian
 * using bitwise operations, and then this handler is registered in the backend's handler array.
 */
EtherFrameHandler::EtherFrameHandler(EtherFrameProvider* backend, uint16_t etherType)
{
    // Convert etherType into big-endian format:
    // Swap the lower and upper bytes of the 16-bit value.
    this->etherType_BE = ((etherType & 0x00FF) << 8) | ((etherType & 0xFF00) >> 8);
    this->backend = backend;
    
    // Register this handler for the specific EtherType in the backend.
    backend->handlers[etherType_BE] = this;
}

/*
 * Destructor:
 *  - Unregisters this handler from its backend if it is currently registered.
 */
EtherFrameHandler::~EtherFrameHandler()
{
    if(backend->handlers[etherType_BE] == this)
        backend->handlers[etherType_BE] = 0;
}
            
/*
 * OnEtherFrameReceived:
 *  - This method is called when an Ethernet frame with the matching EtherType is received.
 *  - The default implementation simply returns false, meaning that the frame was not handled.
 *  - Derived classes should override this function to process the frame payload.
 *
 * Parameters:
 *   - etherframePayload: pointer to the data in the Ethernet frame after the header.
 *   - size: size in bytes of the payload.
 *
 * Returns:
 *   - true if the frame was successfully processed and a response should be sent,
 *   - false otherwise.
 */
bool EtherFrameHandler::OnEtherFrameReceived(common::uint8_t* etherframePayload, common::uint32_t size)
{
    return false;
}

/*
 * Send:
 *  - Sends an Ethernet frame with this handler's EtherType.
 *  - It calls the backend's Send() method, passing along the destination MAC (in big-endian),
 *    EtherType (in big-endian), the data, and its size.
 *
 * Parameters:
 *   - dstMAC_BE: The destination MAC address in big-endian format.
 *   - data: Pointer to the payload to be transmitted.
 *   - size: Length in bytes of the payload.
 */
void EtherFrameHandler::Send(common::uint64_t dstMAC_BE, common::uint8_t* data, common::uint32_t size)
{
    backend->Send(dstMAC_BE, etherType_BE, data, size);
}

/*
 * GetIPAddress:
 *  - Retrieves the IP address of the underlying network interface.
 *  - Simply calls the backend's GetIPAddress() function.
 */
uint32_t EtherFrameHandler::GetIPAddress()
{
    return backend->GetIPAddress();
}


/*
 * ----------------------------------------------------------------------------
 * EtherFrameProvider Class
 * ----------------------------------------------------------------------------
 *
 * The EtherFrameProvider is responsible for sending and receiving raw Ethernet frames.
 * It extends from RawDataHandler (which is typically used by network drivers) and contains an
 * array of EtherFrameHandler pointers to delegate incoming frames based on their EtherType.
 *
 * It also provides functions to send Ethernet frames, and to retrieve the IP and MAC addresses
 * from the underlying hardware (such as an AMD am79c973 NIC driver).
 */

/*
 * Constructor:
 *   - backend: A pointer to an amd_am79c973 driver that provides the low-level interface.
 *
 * The constructor calls the base class (RawDataHandler) constructor with the provided backend.
 * It also initializes its internal handlers array (size 65535) by setting all entries to 0.
 */
EtherFrameProvider::EtherFrameProvider(amd_am79c973* backend)
: RawDataHandler(backend)
{
    for(uint32_t i = 0; i < 65535; i++)
        handlers[i] = 0;
}

/*
 * Destructor:
 *  - Currently does nothing special beyond what the base class does.
 */
EtherFrameProvider::~EtherFrameProvider()
{
}

/*
 * OnRawDataReceived:
 *  - Called by the underlying NIC driver when a raw Ethernet frame is received.
 *  - First verifies that the received data is at least as long as the Ethernet header.
 *  - Then casts the buffer to an EtherFrameHeader structure to extract the header fields.
 *
 * Processing:
 *  - If the destination MAC matches either the broadcast MAC (0xFFFFFFFFFFFF) or the local NIC’s MAC,
 *    and if a handler is registered for the frame’s EtherType, it calls the handler’s
 *    OnEtherFrameReceived() method with the payload.
 *  - If the handler returns true (indicating the frame should be echoed back), the source and destination
 *    MAC addresses in the header are swapped.
 *
 * Returns:
 *  - true if the frame was processed and should be sent back,
 *  - false otherwise.
 */
bool EtherFrameProvider::OnRawDataReceived(common::uint8_t* buffer, common::uint32_t size)
{
    if(size < sizeof(EtherFrameHeader))
        return false;
    
    EtherFrameHeader* frame = (EtherFrameHeader*)buffer;
    bool sendBack = false;
    
    // Check if the frame is broadcast or destined for this NIC.
    if(frame->dstMAC_BE == 0xFFFFFFFFFFFF || frame->dstMAC_BE == backend->GetMACAddress())
    {
        if(handlers[frame->etherType_BE] != 0)
            sendBack = handlers[frame->etherType_BE]->OnEtherFrameReceived(
                buffer + sizeof(EtherFrameHeader), size - sizeof(EtherFrameHeader));
    }
    
    // If the handler signals to send a response, swap MAC addresses.
    if(sendBack)
    {
        frame->dstMAC_BE = frame->srcMAC_BE;
        frame->srcMAC_BE = backend->GetMACAddress();
    }
    
    return sendBack;
}

/*
 * Send:
 *  - Allocates memory for a new Ethernet frame (header + payload),
 *    builds the frame by copying the header fields and the provided data,
 *    and then passes the frame to the backend's Send() method for transmission.
 *
 * Steps:
 *   1. Allocate a buffer large enough for an EtherFrameHeader plus the payload.
 *   2. Fill in the EtherFrameHeader fields:
 *         - dstMAC_BE: Destination MAC address.
 *         - srcMAC_BE: Local NIC's MAC address (obtained from backend).
 *         - etherType_BE: EtherType indicating the protocol of the payload.
 *   3. Copy the payload (data) into the allocated buffer right after the header.
 *   4. Invoke the backend's Send() method to actually transmit the frame.
 *   5. Free the allocated memory.
 */
void EtherFrameProvider::Send(common::uint64_t dstMAC_BE, common::uint16_t etherType_BE, common::uint8_t* buffer, common::uint32_t size)
{
    // Allocate memory for the complete Ethernet frame.
    uint8_t* buffer2 = (uint8_t*)MemoryManager::activeMemoryManager->malloc(sizeof(EtherFrameHeader) + size);
    EtherFrameHeader* frame = (EtherFrameHeader*)buffer2;
    
    // Set header fields for the Ethernet frame.
    frame->dstMAC_BE = dstMAC_BE;
    frame->srcMAC_BE = backend->GetMACAddress();
    frame->etherType_BE = etherType_BE;
    
    // Copy payload data into the frame buffer immediately after the header.
    uint8_t* src = buffer;
    uint8_t* dst = buffer2 + sizeof(EtherFrameHeader);
    for(uint32_t i = 0; i < size; i++)
        dst[i] = src[i];
    
    // Use the NIC's Send method to transmit the complete frame.
    backend->Send(buffer2, size + sizeof(EtherFrameHeader));
    
    // Free the allocated memory used for the frame.
    MemoryManager::activeMemoryManager->free(buffer2);
}

/*
 * GetIPAddress:
 *  - Retrieves the IP address of the underlying network interface by calling the backend.
 */
uint32_t EtherFrameProvider::GetIPAddress()
{
    return backend->GetIPAddress();
}

/*
 * GetMACAddress:
 *  - Retrieves the MAC address of the underlying network interface by calling the backend.
 */
uint64_t EtherFrameProvider::GetMACAddress()
{
    return backend->GetMACAddress();
}
