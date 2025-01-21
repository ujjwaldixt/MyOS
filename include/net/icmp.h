#ifndef __MYOS__NET__ICMP_H                           // Header guard to prevent multiple inclusions
#define __MYOS__NET__ICMP_H

#include <common/types.h>                             // Fixed-width type aliases (uint8_t, uint16_t, etc.)
#include <net/ipv4.h>                                 // InternetProtocolHandler and InternetProtocolProvider definitions

namespace myos
{
    namespace net
    {
        /*
         * InternetControlMessageProtocolMessage:
         *  Represents an ICMP (Internet Control Message Protocol) message header and data.
         *  Fields:
         *    - type: The ICMP message type (e.g., 8 for Echo Request, 0 for Echo Reply).
         *    - code: Subtype or extra information depending on the ICMP type.
         *    - checksum: Used for error-checking the ICMP header and data.
         *    - data: Depending on the ICMP type, this can hold identifiers, sequence numbers, or other info.
         *
         *  __attribute__((packed)) ensures there is no compiler-added padding between fields,
         *  matching the exact ICMP on-wire format.
         */
        struct InternetControlMessageProtocolMessage
        {
            common::uint8_t type;
            common::uint8_t code;
            
            common::uint16_t checksum;
            common::uint32_t data;

        } __attribute__((packed));
        
        
        /*
         * InternetControlMessageProtocol:
         *  Handles ICMP traffic (often used for ping - Echo Request/Echo Reply). 
         *  Inherits from InternetProtocolHandler so it can process ICMP packets (etherType: IPv4, protocol: ICMP).
         *
         *  Methods:
         *    - OnInternetProtocolReceived: Called when an ICMP packet arrives. It processes or replies if necessary.
         *    - RequestEchoReply: Sends an ICMP Echo Request (type 8) to the given IP address,
         *      expecting an Echo Reply (type 0) back from the target if reachable.
         */
        class InternetControlMessageProtocol : InternetProtocolHandler
        {
        public:
            /*
             * Constructor:
             *  Binds this ICMP handler to an InternetProtocolProvider (IPv4) backend, allowing it
             *  to send and receive ICMP packets over IPv4.
             */
            InternetControlMessageProtocol(InternetProtocolProvider* backend);

            // Destructor for cleanup if necessary (usually empty).
            ~InternetControlMessageProtocol();
            
            /*
             * OnInternetProtocolReceived:
             *  Called by the IPv4 provider whenever an ICMP packet arrives.
             *    - srcIP_BE, dstIP_BE: The source and destination IP addresses in big-endian format.
             *    - internetprotocolPayload: Pointer to the ICMP message data.
             *    - size: Length of the ICMP segment in bytes.
             *  Returns true if the packet was handled, false otherwise.
             *  Typically checks for Echo Requests and sends Echo Replies.
             */
            bool OnInternetProtocolReceived(common::uint32_t srcIP_BE,
                                            common::uint32_t dstIP_BE,
                                            common::uint8_t* internetprotocolPayload,
                                            common::uint32_t size);

            /*
             * RequestEchoReply:
             *  Sends an ICMP Echo Request (type 8) to the specified IP address (ip_be in big-endian).
             *  The target should respond with an Echo Reply (type 0) if reachable.
             */
            void RequestEchoReply(common::uint32_t ip_be);
        };
        
    }
}

#endif // __MYOS__NET__ICMP_H
