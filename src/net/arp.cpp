#include <net/arp.h>
using namespace myos;
using namespace myos::common;
using namespace myos::net;
using namespace myos::drivers;

/*
 * ----------------------------------------------------------------------------
 * AddressResolutionProtocol Class Constructor and Destructor
 * ----------------------------------------------------------------------------
 *
 * The ARP protocol is used to map an IPv4 address (logical address) to a MAC 
 * address (physical hardware address) on a local network. This class extends 
 * the EtherFrameHandler (which processes raw Ethernet frames) and specifically 
 * handles frames with EtherType 0x806 (the ARP protocol identifier in Ethernet).
 */

/*
 * Constructor:
 *   - backend: Pointer to an EtherFrameProvider that provides raw Ethernet communication.
 *   The constructor initializes the ARP handler by setting the number of cached 
 *   entries to 0 and passing the EtherType (0x806) to the EtherFrameHandler base class.
 */
AddressResolutionProtocol::AddressResolutionProtocol(EtherFrameProvider* backend)
: EtherFrameHandler(backend, 0x806)  // 0x806 is the ARP EtherType in big-endian format.
{
    numCacheEntries = 0;
}

/*
 * Destructor:
 *  - Currently does nothing but can be used for cleanup if necessary.
 */
AddressResolutionProtocol::~AddressResolutionProtocol()
{
}


/*
 * ----------------------------------------------------------------------------
 * OnEtherFrameReceived
 * ----------------------------------------------------------------------------
 *
 * This function is invoked by the underlying EtherFrameProvider when an Ethernet
 * frame with the ARP EtherType (0x806) is received.
 *
 * Parameters:
 *   - etherframePayload: Pointer to the payload of the Ethernet frame.
 *   - size: The length of the payload in bytes.
 *
 * Returns:
 *   - true if the ARP request should be automatically sent (e.g., an ARP request 
 *     was received and a response was generated).
 *   - false otherwise.
 */
bool AddressResolutionProtocol::OnEtherFrameReceived(uint8_t* etherframePayload, uint32_t size)
{
    // Verify that the payload size is large enough to contain an ARP message.
    if(size < sizeof(AddressResolutionProtocolMessage))
        return false;
    
    // Cast the raw payload to an ARP message structure.
    AddressResolutionProtocolMessage* arp = (AddressResolutionProtocolMessage*)etherframePayload;
    
    // Check if the hardware type in the ARP message equals 0x0100.
    // For Ethernet devices, hardware type 1 (0x0001) is expected, but here it is stored in big-endian,
    // so 0x0100 indicates an Ethernet ARP message.
    if(arp->hardwareType == 0x0100)
    {
        // Check that the protocol field indicates IPv4 (0x0800 => 0x0008 in little-endian representation),
        // the hardware address size is 6 bytes (MAC), protocol address size is 4 bytes (IPv4),
        // and the destination IP in the ARP message equals the IP of our network interface.
        if(arp->protocol == 0x0008
        && arp->hardwareAddressSize == 6
        && arp->protocolAddressSize == 4
        && arp->dstIP == backend->GetIPAddress())
        {
            switch(arp->command)
            {
                case 0x0100: // ARP Request
                    /*
                     * For an ARP request:
                     *  - Change the command to 0x0200 (ARP Response).
                     *  - Swap the source and destination IP addresses.
                     *  - Swap the source and destination MAC addresses.
                     *  - Set the new source IP and MAC to our device's values.
                     *  - Return true so that the ARP response is sent back via the Send() method.
                     */
                    arp->command = 0x0200;
                    arp->dstIP = arp->srcIP;
                    arp->dstMAC = arp->srcMAC;
                    arp->srcIP = backend->GetIPAddress();
                    arp->srcMAC = backend->GetMACAddress();
                    return true;
                    break;
                    
                case 0x0200: // ARP Response
                    /*
                     * For an ARP response:
                     *  - Cache the mapping (IP -> MAC) in our ARP cache if there is room.
                     */
                    if(numCacheEntries < 128)
                    {
                        IPcache[numCacheEntries] = arp->srcIP;
                        MACcache[numCacheEntries] = arp->srcMAC;
                        numCacheEntries++;
                    }
                    break;
            }
        }
        
    }
    
    // If no action was taken (e.g., if the ARP message does not match our criteria), return false.
    return false;
}

/*
 * ----------------------------------------------------------------------------
 * BroadcastMACAddress
 * ----------------------------------------------------------------------------
 *
 * Sends an ARP response (broadcast) for a given IP address. This function
 * essentially "announces" the MAC address for the specified IP.
 *
 * Parameters:
 *   - IP_BE: The target IP address in big-endian format.
 */
void AddressResolutionProtocol::BroadcastMACAddress(uint32_t IP_BE)
{
    AddressResolutionProtocolMessage arp;
    arp.hardwareType = 0x0100;          // Ethernet hardware type (big-endian representation for 0x0001)
    arp.protocol = 0x0008;              // IPv4 protocol (big-endian representation for 0x0800)
    arp.hardwareAddressSize = 6;        // MAC addresses are 6 bytes
    arp.protocolAddressSize = 4;        // IPv4 addresses are 4 bytes
    arp.command = 0x0200;               // ARP response command
    
    // Set source addresses to our device's values.
    arp.srcMAC = backend->GetMACAddress();
    arp.srcIP = backend->GetIPAddress();
    // Set destination addresses: use the result from Resolve() to get the correct MAC for the given IP.
    arp.dstMAC = Resolve(IP_BE);
    arp.dstIP = IP_BE;
    
    // Send the ARP response frame using the Ethernet provider.
    this->Send(arp.dstMAC, (uint8_t*)&arp, sizeof(AddressResolutionProtocolMessage));
}

/*
 * ----------------------------------------------------------------------------
 * RequestMACAddress
 * ----------------------------------------------------------------------------
 *
 * Sends an ARP request broadcast to determine the MAC address that corresponds
 * to a specific IP address.
 *
 * Parameters:
 *   - IP_BE: The target IP address in big-endian format for which the MAC is requested.
 */
void AddressResolutionProtocol::RequestMACAddress(uint32_t IP_BE)
{
    AddressResolutionProtocolMessage arp;
    arp.hardwareType = 0x0100;          // Ethernet hardware type
    arp.protocol = 0x0008;              // IPv4 protocol
    arp.hardwareAddressSize = 6;        // MAC size is 6 bytes
    arp.protocolAddressSize = 4;        // IPv4 size is 4 bytes
    arp.command = 0x0100;               // ARP request command
    
    arp.srcMAC = backend->GetMACAddress();  // Our device's MAC address
    arp.srcIP = backend->GetIPAddress();    // Our device's IP address
    arp.dstMAC = 0xFFFFFFFFFFFF;              // Broadcast MAC address (all bits set)
    arp.dstIP = IP_BE;                        // The target IP for which we are requesting the MAC
    
    // Broadcast the ARP request frame to the network.
    this->Send(arp.dstMAC, (uint8_t*)&arp, sizeof(AddressResolutionProtocolMessage));
}

/*
 * ----------------------------------------------------------------------------
 * GetMACFromCache
 * ----------------------------------------------------------------------------
 *
 * Checks the ARP cache for a MAC address that corresponds to the given IP.
 *
 * Parameters:
 *   - IP_BE: Target IP address in big-endian format.
 *
 * Returns:
 *   - The corresponding MAC address if found in the cache.
 *   - The broadcast MAC address (0xFFFFFFFFFFFF) if not found.
 */
uint64_t AddressResolutionProtocol::GetMACFromCache(uint32_t IP_BE)
{
    for(int i = 0; i < numCacheEntries; i++)
        if(IPcache[i] == IP_BE)
            return MACcache[i];
    return 0xFFFFFFFFFFFF; // Return the broadcast address if not found.
}

/*
 * ----------------------------------------------------------------------------
 * Resolve
 * ----------------------------------------------------------------------------
 *
 * Attempts to resolve a given IP address to a MAC address by consulting the ARP cache.
 * If the MAC address is not in the cache, it sends an ARP request and then waits 
 * (busy-wait loop) until the MAC address is found.
 *
 * Parameters:
 *   - IP_BE: The IP address in big-endian format to resolve.
 *
 * Returns:
 *   - The resolved MAC address.
 *
 * Note: The busy-wait loop can potentially block forever if no response is received.
 */
uint64_t AddressResolutionProtocol::Resolve(uint32_t IP_BE)
{
    uint64_t result = GetMACFromCache(IP_BE);
    if(result == 0xFFFFFFFFFFFF)
        RequestMACAddress(IP_BE);

    // Busy-wait until the ARP cache is updated with the MAC address.
    while(result == 0xFFFFFFFFFFFF) // potential infinite loop if no reply is received
        result = GetMACFromCache(IP_BE);
    
    return result;
}
