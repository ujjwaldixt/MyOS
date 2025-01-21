#ifndef __MYOS__NET__ETHERFRAME_H                     // Header guard to prevent multiple inclusions
#define __MYOS__NET__ETHERFRAME_H

#include <common/types.h>                            // Provides fixed-width integer types (e.g., uint8_t, uint64_t)
#include <drivers/amd_am79c973.h>                    // Network driver for AMD AM79C973 NIC
#include <memorymanagement.h>                        // Memory allocation/deallocation functions (if needed)

namespace myos
{
    namespace net
    {
        /*
         * EtherFrameHeader:
         *  Defines the structure of an Ethernet frame header in memory.
         *  Fields are stored in big-endian format (BE) as is typical on the wire. 
         *
         *  Members:
         *    - dstMAC_BE (48 bits): Destination MAC address (in big-endian format).
         *    - srcMAC_BE (48 bits): Source MAC address (in big-endian format).
         *    - etherType_BE (16 bits): Identifies the protocol encapsulated in the Ethernet frame 
         *      (e.g., 0x0800 for IPv4, 0x0806 for ARP).
         *
         *  The bit-fields (': 48') ensure that only 48 bits (6 bytes) are used for MAC addresses, 
         *  although stored within a 64-bit container. This struct is marked 'packed' to avoid any 
         *  compiler-inserted alignment padding.
         */
        struct EtherFrameHeader
        {
            common::uint64_t dstMAC_BE : 48;  
            common::uint64_t srcMAC_BE : 48;  
            common::uint16_t etherType_BE;
        } __attribute__ ((packed));
        
        /*
         * EtherFrameFooter:
         *  An Ethernet frame trailer might contain a Frame Check Sequence (FCS) or other data.
         *  In many implementations, the FCS is handled by hardware. For simplicity, 
         *  this typedef can represent a 32-bit trailer if needed.
         */
        typedef common::uint32_t EtherFrameFooter;
        
        
        
        // Forward declaration to allow EtherFrameHandler to reference EtherFrameProvider.
        class EtherFrameProvider;
        
        /*
         * EtherFrameHandler:
         *  Serves as a base class for handling specific EtherType frames (e.g., ARP, IPv4).
         *  Each derived handler registers with an EtherFrameProvider for a specific EtherType.
         *
         *  - backend: Pointer to the EtherFrameProvider that sends/receives raw Ethernet frames.
         *  - etherType_BE: The big-endian EtherType this handler processes (e.g. 0x0800 for IPv4).
         */
        class EtherFrameHandler
        {
        protected:
            EtherFrameProvider* backend;
            common::uint16_t etherType_BE;
             
        public:
            // Constructor: associates this handler with a provider and an EtherType.
            EtherFrameHandler(EtherFrameProvider* backend, common::uint16_t etherType);
            
            // Destructor for cleanup (currently empty in most cases).
            ~EtherFrameHandler();
            
            /*
             * OnEtherFrameReceived:
             *  Called by the EtherFrameProvider when a frame with a matching EtherType is received.
             *  etherframePayload: pointer to the raw data following the Ethernet header.
             *  size: length of the payload in bytes.
             *  Returns true if the frame was handled, false otherwise.
             */
            virtual bool OnEtherFrameReceived(common::uint8_t* etherframePayload, common::uint32_t size);

            /*
             * Send:
             *  Sends an Ethernet frame to a specified destination MAC address using the associated EtherType.
             *  dstMAC_BE: 48-bit MAC address in big-endian format.
             *  etherframePayload: pointer to the payload data to be transmitted.
             *  size: size of the payload in bytes.
             */
            void Send(common::uint64_t dstMAC_BE, common::uint8_t* etherframePayload, common::uint32_t size);

            /*
             * GetIPAddress:
             *  A helper method (often overridden or used by derived classes) to retrieve the 
             *  local IP address from the underlying network interface. Typically used by higher-level protocols.
             */
            common::uint32_t GetIPAddress();
        };
        
        
        /*
         * EtherFrameProvider:
         *  Inherits from RawDataHandler, allowing it to receive raw data from the AMD AM79C973 NIC driver.
         *  It is responsible for:
         *    - Receiving raw Ethernet frames
         *    - Dispatching them to the correct EtherFrameHandler based on EtherType
         *    - Sending frames on behalf of EtherFrameHandler objects
         *
         *  The handlers array (65535 in size) stores pointers to EtherFrameHandler instances indexed by EtherType.
         *  This allows quick lookup to find the right handler when a frame arrives.
         */
        class EtherFrameProvider : public myos::drivers::RawDataHandler
        {
            // Allows EtherFrameHandler objects to access private/protected members of EtherFrameProvider.
            friend class EtherFrameHandler;

        protected:
            // Array of EtherFrameHandler pointers, indexed by EtherType (0..65534).
            EtherFrameHandler* handlers[65535];

        public:
            /*
             * Constructor:
             *  - backend: pointer to the AMD AM79C973 driver, which provides raw network data.
             *  Registers this EtherFrameProvider with the driver, enabling Ethernet frame handling.
             */
            EtherFrameProvider(drivers::amd_am79c973* backend);

            // Destructor for cleanup if necessary.
            ~EtherFrameProvider();
            
            /*
             * OnRawDataReceived:
             *  Called by the AMD AM79C973 driver whenever an Ethernet frame arrives.
             *  buffer: pointer to the raw Ethernet frame data.
             *  size: length of the frame in bytes.
             *  Returns true if processed, false otherwise.
             *
             *  This method parses the Ethernet header to determine dstMAC, srcMAC, EtherType,
             *  and then forwards the payload to the matching EtherFrameHandler.
             */
            bool OnRawDataReceived(common::uint8_t* buffer, common::uint32_t size);

            /*
             * Send:
             *  Transmits an Ethernet frame with the given destination MAC, EtherType, and payload.
             *  The raw data is assembled into a complete Ethernet frame and passed to the NIC driver.
             */
            void Send(common::uint64_t dstMAC_BE, common::uint16_t etherType_BE, common::uint8_t* buffer, common::uint32_t size);
            
            /*
             * GetMACAddress:
             *  Returns the local interface’s 48-bit MAC address (from the AMD AM79C973).
             */
            common::uint64_t GetMACAddress();

            /*
             * GetIPAddress:
             *  Retrieves the local IP address as configured on the AMD AM79C973 driver (if assigned).
             */
            common::uint32_t GetIPAddress();
        };
        
    }
}

#endif // __MYOS__NET__ETHERFRAME_H
