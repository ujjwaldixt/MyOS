#include <net/ipv4.h>

using namespace myos;
using namespace myos::common;
using namespace myos::net;
using namespace myos::drivers;

/*
 * ----------------------------------------------------------------------------
 * InternetProtocolHandler Class
 * ----------------------------------------------------------------------------
 *
 * This class acts as the base handler for IP packets carrying a specific protocol.
 * It is used to demultiplex incoming IPv4 packets (based on their 'protocol' field)
 * to the corresponding higher-level protocol handler (e.g. ICMP, TCP, UDP).
 *
 * Each InternetProtocolHandler instance is registered with its InternetProtocolProvider.
 */

/*
 * Constructor:
 *   - backend: A pointer to the InternetProtocolProvider that received the IP packets.
 *   - protocol: The protocol number this handler should handle (e.g., 0x01 for ICMP).
 *
 * The constructor stores the provided backend and protocol, then registers this handler
 * in the backend's handler array for the given protocol.
 */
InternetProtocolHandler::InternetProtocolHandler(InternetProtocolProvider* backend, uint8_t protocol)
{
    this->backend = backend;
    this->ip_protocol = protocol;
    backend->handlers[protocol] = this;
}

/*
 * Destructor:
 *  - Unregisters this handler from the backend if it is still registered.
 */
InternetProtocolHandler::~InternetProtocolHandler()
{
    if(backend->handlers[ip_protocol] == this)
        backend->handlers[ip_protocol] = 0;
}
            
/*
 * OnInternetProtocolReceived:
 *   - This virtual function is called when an IP packet with a matching protocol
 *     is received by the InternetProtocolProvider.
 *   - Parameters:
 *       srcIP_BE: Source IP address in big-endian format.
 *       dstIP_BE: Destination IP address in big-endian format.
 *       internetprotocolPayload: Pointer to the payload following the IP header.
 *       size: Size in bytes of the payload.
 *
 *   - The default implementation does nothing and returns false.
 *     Subclasses should override this to process the IP payload.
 */
bool InternetProtocolHandler::OnInternetProtocolReceived(uint32_t srcIP_BE, uint32_t dstIP_BE,
                                            uint8_t* internetprotocolPayload, uint32_t size)
{
    return false;
}

/*
 * Send:
 *   - Sends an IP packet with the given payload to a destination IP address.
 *   - Delegates the send operation to the backend (InternetProtocolProvider).
 *
 * Parameters:
 *   - dstIP_BE: Destination IP address (in big-endian format).
 *   - internetprotocolPayload: Pointer to the data payload.
 *   - size: Length in bytes of the data payload.
 */
void InternetProtocolHandler::Send(uint32_t dstIP_BE, uint8_t* internetprotocolPayload, uint32_t size)
{
    backend->Send(dstIP_BE, ip_protocol, internetprotocolPayload, size);
}


/*
 * ----------------------------------------------------------------------------
 * InternetProtocolProvider Class
 * ----------------------------------------------------------------------------
 *
 * The InternetProtocolProvider manages sending and receiving IPv4 packets.
 * It extends the EtherFrameHandler to handle Ethernet frames with the EtherType for IPv4.
 *
 * It maintains an array (handlers[]) of InternetProtocolHandler pointers
 * so that when an IP packet is received, the correct higher-level protocol handler
 * can be invoked based on the 'protocol' field in the IP header.
 *
 * It also integrates with ARP for address resolution, and uses parameters such as
 * gatewayIP and subnetMask for routing.
 */

/*
 * Constructor:
 *   - backend: Pointer to an EtherFrameProvider which receives raw Ethernet frames.
 *   - arp: Pointer to an AddressResolutionProtocol instance for mapping IPs to MAC addresses.
 *   - gatewayIP: The IP address of the default gateway (in big-endian format) used for routing.
 *   - subnetMask: The network’s subnet mask (in big-endian format), used to determine if
 *                 a destination IP is local or needs to be routed via the gateway.
 *
 * The constructor initializes the handler array to all zeros and stores the ARP instance,
 * gatewayIP, and subnetMask for later use.
 * It also calls the base class constructor with EtherType 0x800, which is used for IPv4.
 */
InternetProtocolProvider::InternetProtocolProvider(EtherFrameProvider* backend, 
                                                   AddressResolutionProtocol* arp,
                                                   uint32_t gatewayIP, uint32_t subnetMask)
: EtherFrameHandler(backend, 0x800)
{
    for(int i = 0; i < 255; i++)
        handlers[i] = 0;
    this->arp = arp;
    this->gatewayIP = gatewayIP;
    this->subnetMask = subnetMask;
}

/*
 * Destructor:
 *   - Currently empty since no dynamic resources are allocated within the provider.
 */
InternetProtocolProvider::~InternetProtocolProvider()
{
}

/*
 * OnEtherFrameReceived:
 *   - This method is called when an Ethernet frame with EtherType 0x0800 (IPv4) is received.
 *   - It first verifies the frame is large enough to contain an InternetProtocolV4Message header.
 *   - It then processes the IP header:
 *       * Checks if the destination IP matches the NIC's IP address.
 *       * Uses the 'totalLength' field from the header to determine the length of the IP packet.
 *       * Passes the payload (i.e., after the IP header) to the registered protocol handler
 *         based on the 'protocol' field in the IP header.
 *
 *   - If the protocol handler indicates the packet should be "sent back" (sendBack is true):
 *       * Swaps the source and destination IP addresses.
 *       * Resets the Time-to-Live (TTL) to a default value (0x40).
 *       * Recalculates the IP header checksum.
 *
 * Returns:
 *   - true if the packet was processed and modified for echo or reply.
 *   - false otherwise.
 */
bool InternetProtocolProvider::OnEtherFrameReceived(uint8_t* etherframePayload, uint32_t size)
{
    if(size < sizeof(InternetProtocolV4Message))
        return false;
    
    // Cast the buffer to an IP header
    InternetProtocolV4Message* ipmessage = (InternetProtocolV4Message*)etherframePayload;
    bool sendBack = false;
    
    // Check if the destination IP matches our NIC's IP address.
    if(ipmessage->dstIP == backend->GetIPAddress())
    {
        int length = ipmessage->totalLength;
        if(length > size)
            length = size;
        
        // Invoke the appropriate InternetProtocolHandler based on ipmessage->protocol.
        if(handlers[ipmessage->protocol] != 0)
            sendBack = handlers[ipmessage->protocol]->OnInternetProtocolReceived(
                ipmessage->srcIP, ipmessage->dstIP, 
                etherframePayload + 4 * ipmessage->headerLength, 
                length - 4 * ipmessage->headerLength);
    }
    
    // If a response is required (sendBack is true), prepare the IP header for a reply:
    if(sendBack)
    {
        // Swap source and destination IP addresses.
        uint32_t temp = ipmessage->dstIP;
        ipmessage->dstIP = ipmessage->srcIP;
        ipmessage->srcIP = temp;
        
        // Reset TTL to 0x40.
        ipmessage->timeToLive = 0x40;
        // Recalculate the checksum (first set checksum to 0).
        ipmessage->checksum = 0;
        ipmessage->checksum = Checksum((uint16_t*)ipmessage, 4 * ipmessage->headerLength);
    }
    
    return sendBack;
}


/*
 * Send:
 *  - Constructs and sends an IPv4 packet.
 *  - Allocates a buffer large enough to hold both the InternetProtocolV4Message header and the data payload.
 *  - Fills in the IP header fields such as version, header length, Type of Service (TOS), total length,
 *    identification, flags and fragment offset, TTL, and protocol.
 *  - Sets the source IP (from the NIC) and the destination IP as given.
 *  - The total length field is byte-swapped to match network byte order.
 *  - Computes the checksum for the IP header.
 *  - Copies the payload data into the allocated buffer after the header.
 *  - Determines the next hop:
 *      * If the destination IP is on the same subnet as the local IP (using the subnet mask),
 *        the packet is sent directly.
 *      * Otherwise, the packet is routed via the gateway IP.
 *  - Resolves the next hop’s MAC address using ARP.
 *  - Sends the complete IP packet via the backend's Send() method.
 *  - Finally, frees the allocated memory.
 *
 * Parameters:
 *   - dstIP_BE: Destination IP in big-endian format.
 *   - protocol: The IP protocol number for the packet payload.
 *   - data: Pointer to the payload data.
 *   - size: Size of the payload in bytes.
 */
void InternetProtocolProvider::Send(uint32_t dstIP_BE, uint8_t protocol, uint8_t* data, uint32_t size)
{
    // Allocate memory for the IP packet (header + payload)
    uint8_t* buffer = (uint8_t*)MemoryManager::activeMemoryManager->malloc(sizeof(InternetProtocolV4Message) + size);
    InternetProtocolV4Message* message = (InternetProtocolV4Message*)buffer;
    
    message->version = 4;  // IPv4
    // Set header length (in 32-bit words). The header size is sizeof(InternetProtocolV4Message).
    message->headerLength = sizeof(InternetProtocolV4Message) / 4;
    message->tos = 0;  // Type of Service; 0 means routine precedence
    message->totalLength = size + sizeof(InternetProtocolV4Message);
    // Convert totalLength to network byte order by swapping bytes.
    message->totalLength = ((message->totalLength & 0xFF00) >> 8)
                         | ((message->totalLength & 0x00FF) << 8);
    message->ident = 0x0100;           // Identification field; arbitrary value
    message->flagsAndOffset = 0x0040;  // Flags and fragment offset; no fragmentation desired
    message->timeToLive = 0x40;        // TTL set to 64 (0x40)
    message->protocol = protocol;      // Set the protocol field (e.g., ICMP, TCP, UDP)
    
    // Set destination IP and source IP (obtained from the NIC)
    message->dstIP = dstIP_BE;
    message->srcIP = backend->GetIPAddress();
    
    // Compute checksum: first zero the checksum field, then compute it over the header.
    message->checksum = 0;
    message->checksum = Checksum((uint16_t*)message, sizeof(InternetProtocolV4Message));
    
    // Copy the payload data into the buffer immediately after the IP header.
    uint8_t* databuffer = buffer + sizeof(InternetProtocolV4Message);
    for(int i = 0; i < size; i++)
        databuffer[i] = data[i];
    
    // Determine the routing: if the destination IP is in a different subnet,
    // send the packet to the gateway instead.
    uint32_t route = dstIP_BE;
    if((dstIP_BE & subnetMask) != (message->srcIP & subnetMask))
        route = gatewayIP;
    
    // Resolve the next-hop MAC address using ARP, then send the IP packet via the backend.
    backend->Send(arp->Resolve(route), this->etherType_BE, buffer, sizeof(InternetProtocolV4Message) + size);
    
    // Free the allocated memory.
    MemoryManager::activeMemoryManager->free(buffer);
}

/*
 * Checksum:
 *  - Computes the Internet checksum (RFC 1071) over the provided data.
 *  - This checksum is used in the IP header and other protocols to verify the integrity 
 *    of data.
 *
 * Parameters:
 *   - data: Pointer to the data over which to compute the checksum (interpreted as an array of 16-bit words).
 *   - lengthInBytes: The total length in bytes of the data.
 *
 * Steps:
 *   1. Sum the 16-bit words, byte-swapping each word to the correct endianness.
 *   2. If there is an odd byte, add it (shifted appropriately).
 *   3. Fold any carry bits from the upper 16 bits back into the lower 16 bits.
 *   4. Return the one's complement of the result, byte-swapped to match network byte order.
 *
 * Returns:
 *   - The computed checksum as a 16-bit value.
 */
uint16_t InternetProtocolProvider::Checksum(uint16_t* data, uint32_t lengthInBytes)
{
    uint32_t temp = 0;

    // Sum each 16-bit word, swapping byte order as needed
    for(int i = 0; i < lengthInBytes / 2; i++)
        temp += ((data[i] & 0xFF00) >> 8) | ((data[i] & 0x00FF) << 8);

    // If length is odd, add the last byte shifted appropriately
    if(lengthInBytes % 2)
        temp += ((uint16_t)((char*)data)[lengthInBytes - 1]) << 8;
    
    // Fold the carry bits into the lower 16 bits
    while(temp & 0xFFFF0000)
        temp = (temp & 0xFFFF) + (temp >> 16);
    
    // Return the one's complement of the result (swapping bytes to network order)
    return ((~temp & 0xFF00) >> 8) | ((~temp & 0x00FF) << 8);
}
