#ifndef __MYOS__NET__TCP_H                                    // Header guard to prevent multiple inclusions
#define __MYOS__NET__TCP_H

#include <common/types.h>                                     // Common type definitions (e.g. uint8_t, uint16_t, etc.)
#include <net/ipv4.h>                                         // InternetProtocolHandler for IPv4 layer integration
#include <memorymanagement.h>                                 // Memory management helpers (if needed for dynamic allocations)

namespace myos
{
    namespace net
    {
        /*
         * TransmissionControlProtocolSocketState:
         *   Enumerates possible states of a TCP connection (as described in RFC 793).
         *   Each state represents a distinct phase in the TCP connection lifecycle.
         *   - CLOSED: No connection state.
         *   - LISTEN: Waiting for incoming connections.
         *   - SYN_SENT: SYN packet sent, waiting for SYN+ACK from remote.
         *   - SYN_RECEIVED: SYN received from remote, waiting to finalize connection handshake.
         *   - ESTABLISHED: Connection fully open and data can be exchanged.
         *   - FIN_WAIT1, FIN_WAIT2, CLOSING, TIME_WAIT: States for orderly shutdown with FIN/ACK sequences.
         *   - CLOSE_WAIT: Local side has received FIN but not yet closed.
         */
        enum TransmissionControlProtocolSocketState
        {
            CLOSED,
            LISTEN,
            SYN_SENT,
            SYN_RECEIVED,
            ESTABLISHED,
            FIN_WAIT1,
            FIN_WAIT2,
            CLOSING,
            TIME_WAIT,
            CLOSE_WAIT
            // LAST_ACK (not listed but often present in full TCP state diagrams)
        };
        
        /*
         * TransmissionControlProtocolFlag:
         *   Defines bit flags that indicate various control signals in a TCP segment header.
         *   - FIN: Indicates the sender is finished sending (final).
         *   - SYN: Synchronize sequence numbers (used in connection setup).
         *   - RST: Reset the connection.
         *   - PSH: Push function (data should be passed to the application ASAP).
         *   - ACK: Acknowledgment field is valid.
         *   - URG: Urgent pointer is valid.
         *   - ECE, CWR, NS: Extended Congestion Notification features.
         */
        enum TransmissionControlProtocolFlag
        {
            FIN = 1,
            SYN = 2,
            RST = 4,
            PSH = 8,
            ACK = 16,
            URG = 32,
            ECE = 64,
            CWR = 128,
            NS  = 256
        };
        
        
        /*
         * TransmissionControlProtocolHeader:
         *   Represents the structure of a TCP segment header (simplified for demonstration).
         *   - srcPort, dstPort: 16-bit source/destination ports.
         *   - sequenceNumber: 32-bit sequence number for data ordering.
         *   - acknowledgementNumber: 32-bit acknowledgement number for confirming received data.
         *   - headerSize32 (4 bits): Size of the TCP header in 32-bit words.
         *   - flags (8 bits): Holds the combination of TCP flags (FIN, SYN, ACK, etc.).
         *   - windowSize: The size of the receive window, used for flow control.
         *   - checksum: Used for error-checking the header and data.
         *   - urgentPtr: Points to urgent data within the segment (if URG flag set).
         *   - options: A placeholder for optional header fields (e.g., maximum segment size). 
         *     This structure uses 32 bits for simplicity; real implementations can have variable-length options.
         *
         *   Marked 'packed' to prevent alignment padding, matching the on-wire format.
         */
        struct TransmissionControlProtocolHeader
        {
            common::uint16_t srcPort;
            common::uint16_t dstPort;
            common::uint32_t sequenceNumber;
            common::uint32_t acknowledgementNumber;
            
            // The top 4 bits of this byte are reserved; the lower 4 bits define the header size in 32-bit words.
            common::uint8_t reserved : 4;
            common::uint8_t headerSize32 : 4;

            // 8 bits of flags (e.g., SYN, ACK, FIN, etc.).
            common::uint8_t flags;
            
            common::uint16_t windowSize;
            common::uint16_t checksum;
            common::uint16_t urgentPtr;
            
            // This simplified header reserves 32 bits for possible options.
            common::uint32_t options;
        } __attribute__((packed));
       
        /*
         * TransmissionControlProtocolPseudoHeader:
         *   A pseudo-header used for calculating the TCP checksum. It includes fields from the IP header
         *   plus TCP length, ensuring TCP’s checksum covers certain IP-level fields as well.
         *   - srcIP, dstIP: Source/destination IP addresses.
         *   - protocol: Protocol identifier (typically 6 for TCP).
         *   - totalLength: Length of the TCP segment (header + data).
         */
        struct TransmissionControlProtocolPseudoHeader
        {
            common::uint32_t srcIP;
            common::uint32_t dstIP;
            common::uint16_t protocol;
            common::uint16_t totalLength;
        } __attribute__((packed));
      
      
        // Forward declarations
        class TransmissionControlProtocolSocket;
        class TransmissionControlProtocolProvider;
        
        /*
         * TransmissionControlProtocolHandler:
         *   A base class that can be extended to handle incoming TCP data for a specific socket or connection.
         *   - HandleTransmissionControlProtocolMessage: called when data arrives on an associated socket.
         */
        class TransmissionControlProtocolHandler
        {
        public:
            TransmissionControlProtocolHandler();
            ~TransmissionControlProtocolHandler();

            /*
             * HandleTransmissionControlProtocolMessage:
             *   Called by the socket when new TCP data arrives. 
             *   'data' is the payload portion after the TCP header, and 'size' is its length in bytes.
             *   Returns 'true' if handled successfully, or false if not.
             */
            virtual bool HandleTransmissionControlProtocolMessage(TransmissionControlProtocolSocket* socket,
                                                                  common::uint8_t* data,
                                                                  common::uint16_t size);
        };
      
        
        /*
         * TransmissionControlProtocolSocket:
         *   Represents a TCP socket. Contains information about:
         *     - Local and remote IP/ports,
         *     - Sequence and acknowledgement numbers,
         *     - Current state of the TCP finite state machine (ESTABLISHED, SYN_SENT, etc.),
         *     - References to the provider (backend) and a handler for received data.
         *
         *   Provides methods for handling incoming data and sending/closing connections.
         */
        class TransmissionControlProtocolSocket
        {
            // Allows the provider to modify internal state (e.g., during connection setup or teardown).
            friend class TransmissionControlProtocolProvider;

        protected:
            // Remote endpoint IP/port
            common::uint16_t remotePort;
            common::uint32_t remoteIP;

            // Local endpoint IP/port
            common::uint16_t localPort;
            common::uint32_t localIP;

            // Track current sequence and acknowledgement numbers.
            common::uint32_t sequenceNumber;
            common::uint32_t acknowledgementNumber;

            // Reference to the TCP provider that manages this socket's network interactions.
            TransmissionControlProtocolProvider* backend;

            // Application-level handler for incoming data.
            TransmissionControlProtocolHandler* handler;
            
            // The current state of this TCP socket (e.g., ESTABLISHED).
            TransmissionControlProtocolSocketState state;

        public:
            // Constructor links this socket to a specific TCP provider.
            TransmissionControlProtocolSocket(TransmissionControlProtocolProvider* backend);
            ~TransmissionControlProtocolSocket();

            /*
             * HandleTransmissionControlProtocolMessage:
             *   Invoked when a TCP segment is received for this socket. The method can process data, update state, etc.
             */
            virtual bool HandleTransmissionControlProtocolMessage(common::uint8_t* data, common::uint16_t size);

            /*
             * Send:
             *   Sends data over this socket to the remote endpoint.
             *   The provider handles framing the data in a TCP segment and sending via IP.
             */
            virtual void Send(common::uint8_t* data, common::uint16_t size);

            /*
             * Disconnect:
             *   Initiates a proper TCP teardown sequence (FIN/ACK) to close the socket connection.
             */
            virtual void Disconnect();
        };
      
      
        /*
         * TransmissionControlProtocolProvider:
         *   Inherits from InternetProtocolHandler to handle TCP packets at the IP level (protocol = 6).
         *   Manages a set of TCP sockets and coordinates their states. 
         *   When a packet arrives, it identifies which socket it belongs to (based on ports/IPs) 
         *   and forwards the data to that socket’s handler.
         *
         *   Provides methods to create/listen/connect/disconnect sockets, and to send data.
         */
        class TransmissionControlProtocolProvider : InternetProtocolHandler
        {
        protected:
            // Array of pointers to sockets, keyed by local port (simplified approach).
            TransmissionControlProtocolSocket* sockets[65535];

            // Number of sockets currently in use (not strictly necessary if we track them differently, but used here).
            common::uint16_t numSockets;

            // Used to allocate ephemeral ports (dynamic port assignment).
            common::uint16_t freePort;
            
        public:
            /*
             * Constructor:
             *   - backend: The InternetProtocolProvider for sending/receiving IP packets with protocol number 6 (TCP).
             *   Initializes data structures for socket management.
             */
            TransmissionControlProtocolProvider(InternetProtocolProvider* backend);
            ~TransmissionControlProtocolProvider();
            
            /*
             * OnInternetProtocolReceived:
             *   Called when an IP packet with protocol = TCP arrives.
             *   It extracts the TCP header, finds or creates a corresponding socket, 
             *   and passes the data to that socket's HandleTransmissionControlProtocolMessage method.
             */
            virtual bool OnInternetProtocolReceived(common::uint32_t srcIP_BE,
                                                    common::uint32_t dstIP_BE,
                                                    common::uint8_t* internetprotocolPayload,
                                                    common::uint32_t size);

            /*
             * Connect:
             *   Actively initiate a TCP connection to a remote IP/port.
             *   Creates a new socket, sends a SYN packet, and returns the socket reference.
             */
            virtual TransmissionControlProtocolSocket* Connect(common::uint32_t ip, common::uint16_t port);

            /*
             * Disconnect:
             *   Closes the given socket by sending a FIN or RST if necessary, 
             *   and marking the socket’s state as closed.
             */
            virtual void Disconnect(TransmissionControlProtocolSocket* socket);

            /*
             * Send:
             *   Sends data via the specified socket. 
             *   'flags' can be used to send control flags (SYN, ACK, etc.) in addition to data.
             */
            virtual void Send(TransmissionControlProtocolSocket* socket,
                              common::uint8_t* data,
                              common::uint16_t size,
                              common::uint16_t flags = 0);

            /*
             * Listen:
             *   Opens a socket in the LISTEN state on the specified local port, 
             *   ready to accept incoming TCP connections.
             */
            virtual TransmissionControlProtocolSocket* Listen(common::uint16_t port);

            /*
             * Bind:
             *   Associates a given socket with a specific TransmissionControlProtocolHandler. 
             *   This handler will process incoming data for that socket.
             */
            virtual void Bind(TransmissionControlProtocolSocket* socket, TransmissionControlProtocolHandler* handler);
        };
        
    } // namespace net
} // namespace myos

#endif // __MYOS__NET__TCP_H
