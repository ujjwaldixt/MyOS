#ifndef __MYOS__NET__IPV4_H                          // Header guard to prevent multiple inclusions
#define __MYOS__NET__IPV4_H

#include <common/types.h>                            // Common fixed-width type definitions (uint8_t, uint16_t, etc.)
#include <net/etherframe.h>                          // EtherFrameHandler and EtherFrameProvider definitions
#include <net/arp.h>                                 // ARP (AddressResolutionProtocol) definitions

namespace myos
{
    namespace net
    {
        /*
         * InternetProtocolV4Message:
         *  Represents the IPv4 (Internet Protocol version 4) header for an IP packet. 
         *  This struct is marked 'packed' so that the compiler does not add padding bytes 
         *  between fields, preserving the exact layout required by the protocol.
         *
         *  Fields:
         *    - version (4 bits): IPv4 is version 4.
         *    - headerLength (4 bits): The number of 32-bit words in the header (min. 5 for 20 bytes).
         *    - tos (Type of Service): Differentiated services field, indicating priority/QoS.
         *    - totalLength: Total length of the IP packet, including header and payload.
         *    - ident (Identification): Used for reassembling fragmented packets.
         *    - flagsAndOffset: Contains fragmentation flags and fragment offset.
         *    - timeToLive (TTL): A counter that decrements on each hop; if it hits zero, the packet is dropped.
         *    - protocol: Indicates the transport-level protocol (e.g., ICMP=1, TCP=6, UDP=17).
         *    - checksum: Header checksum for error detection of the IP header itself.
         *    - srcIP: 32-bit source IP address in big-endian (network byte order).
         *    - dstIP: 32-bit destination IP address in big-endian (network byte order).
         */
        struct InternetProtocolV4Message
        {
            // 4-bit fields stored within an 8-bit space.
            common::uint8_t headerLength : 4;
            common::uint8_t version : 4;
            
            common::uint8_t tos;
            common::uint16_t totalLength;
            
            common::uint16_t ident;
            common::uint16_t flagsAndOffset;
            
            common::uint8_t timeToLive;
            common::uint8_t protocol;
            common::uint16_t checksum;
            
            common::uint32_t srcIP;
            common::uint32_t dstIP;
        } __attribute__((packed));
        
        
        
        // Forward declaration to allow InternetProtocolHandler to reference InternetProtocolProvider.
        class InternetProtocolProvider;
     
        /*
         * InternetProtocolHandler:
         *  A base class for handling specific IP-based protocols (e.g. ICMP, TCP, UDP).
         *  It registers with an InternetProtocolProvider for a specific 'ip_protocol' number.
         */
        class InternetProtocolHandler
        {
        protected:
            // A pointer to the associated InternetProtocolProvider (the IPv4 provider).
            InternetProtocolProvider* backend;

            // The IP protocol number this handler manages (e.g., 1 for ICMP, 6 for TCP, 17 for UDP).
            common::uint8_t ip_protocol;
             
        public:
            // Constructor: associates this handler with a provider and a given protocol number.
            InternetProtocolHandler(InternetProtocolProvider* backend, common::uint8_t protocol);
            
            // Destructor (usually empty, could be used for cleanup if needed).
            ~InternetProtocolHandler();
            
            /*
             * OnInternetProtocolReceived:
             *  Called by the InternetProtocolProvider whenever a packet with matching 'ip_protocol' arrives.
             *   - srcIP_BE, dstIP_BE: Source and destination IP addresses in big-endian.
             *   - internetprotocolPayload: Pointer to the payload data following the IP header.
             *   - size: Size of the payload in bytes.
             *  Returns true if the packet was handled, false otherwise.
             */
            virtual bool OnInternetProtocolReceived(common::uint32_t srcIP_BE, 
                                                    common::uint32_t dstIP_BE,
                                                    common::uint8_t* internetprotocolPayload, 
                                                    common::uint32_t size);

            /*
             * Send:
             *  Sends a packet to the specified destination IP (in big-endian) using this protocol.
             *  The provider encapsulates the data in an IPv4 header before sending it over Ethernet.
             */
            void Send(common::uint32_t dstIP_BE, common::uint8_t* internetprotocolPayload, common::uint32_t size);
        };
     
     
        /*
         * InternetProtocolProvider:
         *  Implements IPv4 on top of Ethernet (by extending EtherFrameHandler).
         *  It processes incoming IP packets, demultiplexing them by their 'protocol' field and
         *  calling the appropriate InternetProtocolHandler. It also supports sending IP packets.
         *
         *  Fields:
         *    - handlers: An array of IP protocol handlers indexed by protocol number (0..254).
         *    - arp: Pointer to an AddressResolutionProtocol object to resolve IP -> MAC addresses.
         *    - gatewayIP: The IP address of the default gateway, used when sending packets not on the local network.
         *    - subnetMask: Defines the local network's size. If a destination is within the same subnet, 
         *      we send directly via ARP resolution. Otherwise, we send it to gatewayIP.
         */
        class InternetProtocolProvider : public EtherFrameHandler
        {
            // Allows InternetProtocolHandler to access private/protected members of InternetProtocolProvider.
            friend class InternetProtocolHandler;

        protected:
            // Array of protocol-specific handlers. Index = protocol number (e.g., 1 for ICMP).
            InternetProtocolHandler* handlers[255];

            // Pointer to ARP for resolving MAC addresses from IP addresses.
            AddressResolutionProtocol* arp;
            
            // The IP address of the network gateway (in big-endian).
            common::uint32_t gatewayIP;
            
            // The subnet mask used for determining if an IP address is local or requires routing via the gateway.
            common::uint32_t subnetMask;
            
        public:
            /*
             * Constructor:
             *   - backend: The EtherFrameProvider for low-level Ethernet sending/receiving.
             *   - arp: The ARP instance to query for MAC addresses.
             *   - gatewayIP: The IP of the default gateway (big-endian).
             *   - subnetMask: The local subnet mask (big-endian).
             *  Registers with the EtherFrameProvider for the EtherType corresponding to IPv4 (0x0800).
             */
            InternetProtocolProvider(EtherFrameProvider* backend, 
                                     AddressResolutionProtocol* arp,
                                     common::uint32_t gatewayIP, 
                                     common::uint32_t subnetMask);
            
            // Destructor for cleanup if necessary.
            ~InternetProtocolProvider();
            
            /*
             * OnEtherFrameReceived:
             *  Overridden from EtherFrameHandler. 
             *  Called when an Ethernet frame with EtherType 0x0800 (IPv4) is received.
             *  Parses the IP header, verifies the checksum, and dispatches the packet to the correct protocol handler.
             */
            bool OnEtherFrameReceived(common::uint8_t* etherframePayload, common::uint32_t size);

            /*
             * Send:
             *  Prepares an IP packet with the specified destination IP, protocol number, 
             *  and payload, then encapsulates it in an IPv4 header. 
             *  ARP is used to find the destination MAC (either directly if in subnet, or gateway if not).
             *  Finally, sends the packet using the underlying EtherFrameProvider.
             */
            void Send(common::uint32_t dstIP_BE, common::uint8_t protocol, common::uint8_t* buffer, common::uint32_t size);
            
            /*
             * Checksum:
             *  A utility function to compute the IP-style checksum over 'lengthInBytes' of data.
             *  Summation is done in 16-bit words, and the result is then bitwise-inverted 
             *  to produce the final checksum value.
             */
            static common::uint16_t Checksum(common::uint16_t* data, common::uint32_t lengthInBytes);
        };
    }
}

#endif // __MYOS__NET__IPV4_H
