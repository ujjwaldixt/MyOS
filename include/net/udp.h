#ifndef __MYOS__NET__UDP_H                         // Header guard to prevent multiple inclusions
#define __MYOS__NET__UDP_H

#include <common/types.h>                          // Common fixed-size types (uint8_t, uint16_t, etc.)
#include <net/ipv4.h>                              // InternetProtocolHandler and InternetProtocolProvider
#include <memorymanagement.h>                      // Memory management routines (if needed for dynamic allocations)

namespace myos
{
    namespace net
    {
        /*
         * UserDatagramProtocolHeader:
         *  Represents the header for a UDP (User Datagram Protocol) packet.
         *  Fields:
         *    - srcPort: Source port (16 bits).
         *    - dstPort: Destination port (16 bits).
         *    - length: Length of the entire UDP packet (header + data).
         *    - checksum: Used for error-checking. Optional in IPv4, mandatory in IPv6, but often computed anyway.
         *
         *  Marked 'packed' to ensure the compiler doesn’t insert any padding between fields.
         */
        struct UserDatagramProtocolHeader
        {
            common::uint16_t srcPort;
            common::uint16_t dstPort;
            common::uint16_t length;
            common::uint16_t checksum;
        } __attribute__((packed));
       
      
        // Forward declarations
        class UserDatagramProtocolSocket;
        class UserDatagramProtocolProvider;
        
        /*
         * UserDatagramProtocolHandler:
         *  A base class for handling incoming UDP data. 
         *  Extend this class to process datagrams received on a particular socket.
         *
         *  Method:
         *    - HandleUserDatagramProtocolMessage: called when data arrives on a socket. 
         *      'data' points to the payload, and 'size' is the length in bytes.
         */
        class UserDatagramProtocolHandler
        {
        public:
            // Constructors/destructors (empty for most implementations).
            UserDatagramProtocolHandler();
            ~UserDatagramProtocolHandler();

            /*
             * HandleUserDatagramProtocolMessage:
             *  Invoked by the UDP socket when a datagram is received. The default implementation is empty.
             *  Derived classes override this to provide application-specific behavior.
             */
            virtual void HandleUserDatagramProtocolMessage(UserDatagramProtocolSocket* socket, 
                                                           common::uint8_t* data, 
                                                           common::uint16_t size);
        };
      
        
        /*
         * UserDatagramProtocolSocket:
         *  Represents a UDP socket. Holds details about remote and local IP/port, a reference to the 
         *  underlying UDP provider, a handler for incoming messages, and whether it’s listening for data.
         *
         *  Methods:
         *    - HandleUserDatagramProtocolMessage: called when a datagram arrives for this socket.
         *    - Send: sends UDP data to the remote endpoint.
         *    - Disconnect: effectively closes the socket (though UDP doesn't have a formal connection teardown).
         */
        class UserDatagramProtocolSocket
        {
            // Allows the provider to modify internal fields if necessary.
            friend class UserDatagramProtocolProvider;

        protected:
            // Remote endpoint IP/port
            common::uint16_t remotePort;
            common::uint32_t remoteIP;

            // Local endpoint IP/port
            common::uint16_t localPort;
            common::uint32_t localIP;

            // Reference to the UDP provider managing this socket’s transmissions.
            UserDatagramProtocolProvider* backend;

            // Handler for processing incoming data.
            UserDatagramProtocolHandler* handler;

            // If true, this socket is awaiting incoming data on localPort.
            bool listening;

        public:
            // Constructor links the socket to a UDP provider.
            UserDatagramProtocolSocket(UserDatagramProtocolProvider* backend);
            ~UserDatagramProtocolSocket();

            /*
             * HandleUserDatagramProtocolMessage:
             *  Invoked when data arrives for this socket. Passes the payload to the handler if available.
             */
            virtual void HandleUserDatagramProtocolMessage(common::uint8_t* data, common::uint16_t size);

            /*
             * Send:
             *  Sends UDP data through this socket to the remote IP/port (if set).
             *  If remoteIP/port are not yet defined, typically the socket should be 'Connected'.
             */
            virtual void Send(common::uint8_t* data, common::uint16_t size);

            /*
             * Disconnect:
             *  Not a formal concept in UDP, but can be used to indicate the socket is no longer used for sending.
             *  In practice, it may just reset internal state, since UDP is stateless.
             */
            virtual void Disconnect();
        };
      
      
        /*
         * UserDatagramProtocolProvider:
         *  Inherits from InternetProtocolHandler, allowing it to process IPv4 packets with protocol = 17 (UDP).
         *  Manages an array of UDP sockets, each identified by local (and possibly remote) IP/ports.
         *
         *  Functionality:
         *    - Creating sockets for sending/receiving datagrams (Connect, Listen).
         *    - Receiving incoming UDP packets and dispatching them to the appropriate socket.
         *    - Sending outgoing datagrams for a given socket.
         */
        class UserDatagramProtocolProvider : InternetProtocolHandler
        {
        protected:
            // Array of all possible UDP sockets, indexed by local port (simplification).
            UserDatagramProtocolSocket* sockets[65535];
            // Number of sockets currently in use (optional, depending on management).
            common::uint16_t numSockets;
            // Used to allocate ephemeral ports when none is specified.
            common::uint16_t freePort;
            
        public:
            /*
             * Constructor:
             *  - backend: An InternetProtocolProvider used to send/receive IP packets (with protocol=17 for UDP).
             */
            UserDatagramProtocolProvider(InternetProtocolProvider* backend);
            ~UserDatagramProtocolProvider();
            
            /*
             * OnInternetProtocolReceived:
             *  Invoked by the IP layer when a UDP packet arrives.
             *  Extracts UDP header info, finds the matching socket based on dstPort, 
             *  and hands off the data to that socket’s handler.
             *  Returns true if processed, false otherwise.
             */
            virtual bool OnInternetProtocolReceived(common::uint32_t srcIP_BE, 
                                                    common::uint32_t dstIP_BE,
                                                    common::uint8_t* internetprotocolPayload, 
                                                    common::uint32_t size);

            /*
             * Connect:
             *  Creates a new socket (or reuses one), sets the remote IP/port, and returns the socket pointer.
             *  This is a simple approach, as UDP doesn’t require a handshake like TCP.
             */
            virtual UserDatagramProtocolSocket* Connect(common::uint32_t ip, common::uint16_t port);

            /*
             * Listen:
             *  Creates a new socket that listens for incoming datagrams on the specified local port.
             *  This is how a server would receive data from any remote host.
             */
            virtual UserDatagramProtocolSocket* Listen(common::uint16_t port);

            /*
             * Disconnect:
             *  Terminates use of a socket. Since UDP is stateless, this might just free the socket entry.
             */
            virtual void Disconnect(UserDatagramProtocolSocket* socket);

            /*
             * Send:
             *  Sends data using the specified socket. Builds a UDP packet, sets source/dest ports,
             *  calculates checksums, and passes it to the IP layer for transmission.
             */
            virtual void Send(UserDatagramProtocolSocket* socket, common::uint8_t* data, common::uint16_t size);

            /*
             * Bind:
             *  Associates a socket with a given handler that will process incoming datagrams for that socket.
             */
            virtual void Bind(UserDatagramProtocolSocket* socket, UserDatagramProtocolHandler* handler);
        };
        
    } // namespace net
} // namespace myos

#endif // __MYOS__NET__UDP_H
