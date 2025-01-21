#ifndef __MYOS__NET__ARP_H                      // Header guard to prevent multiple inclusions
#define __MYOS__NET__ARP_H

#include <common/types.h>                       // Provides fixed-size integer types (uint8_t, uint16_t, etc.)
#include <net/etherframe.h>                     // EtherFrameHandler and EtherFrameProvider definitions (Ethernet layer)

namespace myos
{
    namespace net
    {
        /*
         * AddressResolutionProtocolMessage:
         *  Represents the ARP (Address Resolution Protocol) packet structure. 
         *  ARP is used to resolve an IP address into a corresponding MAC (hardware) address on a local network.
         *  
         *  Fields:
         *    - hardwareType: Typically 0x0001 for Ethernet.
         *    - protocol: Usually 0x0800 for IPv4.
         *    - hardwareAddressSize: For Ethernet, this is 6 (bytes in a MAC address).
         *    - protocolAddressSize: For IPv4, this is 4 (bytes in an IP address).
         *    - command: Indicates request (1) or reply (2).
         *    - srcMAC: The sender’s MAC address (48 bits).
         *    - srcIP: The sender’s IP address (32 bits).
         *    - dstMAC: The target’s MAC address (48 bits, often 0 in an ARP request).
         *    - dstIP: The target’s IP address (32 bits).
         *
         *  __attribute__((packed)) ensures the compiler doesn’t add any padding between fields.
         */
        struct AddressResolutionProtocolMessage
        {
            common::uint16_t hardwareType;
            common::uint16_t protocol;
            common::uint8_t hardwareAddressSize; // For Ethernet, 6
            common::uint8_t protocolAddressSize; // For IPv4, 4
            common::uint16_t command;

            // 48-bit fields for MAC addresses (bit fields). 
            // The compiler will store them in 64-bit containers (uint64_t) with only 48 bits in use.
            common::uint64_t srcMAC : 48;  
            common::uint32_t srcIP;
            common::uint64_t dstMAC : 48;
            common::uint32_t dstIP;
            
        } __attribute__((packed));
        
        
        
        /*
         * AddressResolutionProtocol:
         *  Implements ARP handling on top of the Ethernet layer (via EtherFrameHandler).
         *  Manages an ARP cache (IPcache/MACcache) to speed up lookups of IP -> MAC mappings.
         *
         *  Methods:
         *    - OnEtherFrameReceived: Processes incoming ARP messages (both requests and replies).
         *    - RequestMACAddress: Broadcasts an ARP request for a specific IP (in big-endian format).
         *    - GetMACFromCache: Retrieves a cached MAC address if present, or 0 if not found.
         *    - Resolve: Retrieves or requests the MAC address for a given IP, and returns it.
         *    - BroadcastMACAddress: Sends out an ARP reply or announcement (all-ones MAC as the destination).
         */
        class AddressResolutionProtocol : public EtherFrameHandler
        {
            // Arrays to store up to 128 IP->MAC mappings in cache.
            common::uint32_t IPcache[128];
            common::uint64_t MACcache[128];
            int numCacheEntries;
            
        public:
            // Constructor takes a pointer to an EtherFrameProvider (the lower-level Ethernet driver).
            AddressResolutionProtocol(EtherFrameProvider* backend);
            
            // Destructor for clean-up if needed (currently empty).
            ~AddressResolutionProtocol();
            
            /*
             * OnEtherFrameReceived:
             *  Called by EtherFrameProvider when an ARP packet is received.
             *  etherframePayload points to the ARP message data, size is its length.
             *  Returns true if the ARP packet is handled successfully, false otherwise.
             */
            bool OnEtherFrameReceived(common::uint8_t* etherframePayload, common::uint32_t size);

            /*
             * RequestMACAddress:
             *  Sends an ARP request packet asking for the MAC address associated with the specified IP_BE.
             *  IP_BE is the IP address in big-endian format.
             */
            void RequestMACAddress(common::uint32_t IP_BE);

            /*
             * GetMACFromCache:
             *  Looks up the given IP_BE in the ARP cache. If found, returns the stored MAC address.
             *  If not found, returns 0.
             */
            common::uint64_t GetMACFromCache(common::uint32_t IP_BE);

            /*
             * Resolve:
             *  Tries to get the MAC address for IP_BE from the cache. If it's not in cache,
             *  this method triggers an ARP request (RequestMACAddress) and then returns the MAC (if resolved).
             *  In a synchronous design, it might wait or attempt more logic; here it returns 0 if unresolved.
             */
            common::uint64_t Resolve(common::uint32_t IP_BE);

            /*
             * BroadcastMACAddress:
             *  Sends an ARP broadcast or reply to announce our MAC for the given IP_BE.
             *  Useful for certain ARP operations such as "Gratuitous ARP" or updating ARP tables.
             */
            void BroadcastMACAddress(common::uint32_t IP_BE);
        };
        
    }
}

#endif // __MYOS__NET__ARP_H
