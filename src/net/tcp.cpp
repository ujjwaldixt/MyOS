#include <net/tcp.h>

using namespace myos;
using namespace myos::common;
using namespace myos::net;

/*
 * ----------------------------------------------------------------------------
 * TransmissionControlProtocolHandler
 * ----------------------------------------------------------------------------
 *
 * This class defines a base interface for handling incoming TCP messages.
 * It can be subclassed by higher-level code (e.g., a TCP server or client)
 * to process data received over a TCP connection.
 */

/*
 * Constructor:
 *  - Currently does nothing special.
 */
TransmissionControlProtocolHandler::TransmissionControlProtocolHandler()
{
}

/*
 * Destructor:
 *  - Currently does nothing.
 */
TransmissionControlProtocolHandler::~TransmissionControlProtocolHandler()
{
}

/*
 * HandleTransmissionControlProtocolMessage:
 *  - Default method called when a TCP segment is received on a socket.
 *  - The default implementation returns true.
 *  - Subclasses should override this function to process the data as required.
 *
 * Parameters:
 *   - socket: The TCP socket on which data was received.
 *   - data: Pointer to the TCP payload.
 *   - size: Size in bytes of the payload.
 *
 * Returns:
 *   - true if processing was successful; false otherwise.
 */
bool TransmissionControlProtocolHandler::HandleTransmissionControlProtocolMessage(TransmissionControlProtocolSocket* socket, uint8_t* data, uint16_t size)
{
    return true;
}


/*
 * ----------------------------------------------------------------------------
 * TransmissionControlProtocolSocket
 * ----------------------------------------------------------------------------
 *
 * Represents a single TCP connection, keeping track of local and remote ports and IPs,
 * sequence numbers, and the current state in the TCP state machine.
 */

/*
 * Constructor:
 *  - backend: Pointer to the TransmissionControlProtocolProvider that manages this socket.
 *
 * Initializes the socket:
 *   - Stores the backend pointer.
 *   - Sets the handler to 0 (no data handler attached yet).
 *   - Sets the initial state to CLOSED.
 */
TransmissionControlProtocolSocket::TransmissionControlProtocolSocket(TransmissionControlProtocolProvider* backend)
{
    this->backend = backend;
    handler = 0;
    state = CLOSED;
}

/*
 * Destructor:
 *  - Currently does nothing.
 */
TransmissionControlProtocolSocket::~TransmissionControlProtocolSocket()
{
}

/*
 * HandleTransmissionControlProtocolMessage:
 *  - Called when a TCP segment is received on this socket.
 *  - If a higher-level handler is attached, it delegates the processing to that handler.
 *  - Returns the result of the handler call, or false if no handler is set.
 */
bool TransmissionControlProtocolSocket::HandleTransmissionControlProtocolMessage(uint8_t* data, uint16_t size)
{
    if(handler != 0)
        return handler->HandleTransmissionControlProtocolMessage(this, data, size);
    return false;
}

/*
 * Send:
 *  - Transmits data over the TCP connection represented by this socket.
 *  - This function busy-waits until the socket reaches the ESTABLISHED state.
 *  - Then, it instructs the TransmissionControlProtocolProvider to send a segment with the PSH and ACK flags.
 *
 * Parameters:
 *   - data: Pointer to the data to send.
 *   - size: Number of bytes to send.
 */
void TransmissionControlProtocolSocket::Send(uint8_t* data, uint16_t size)
{
    // Wait until the connection is established before sending data.
    while(state != ESTABLISHED)
    {
    }
    backend->Send(this, data, size, PSH | ACK);
}

/*
 * Disconnect:
 *  - Initiates a disconnect sequence for this TCP connection.
 *  - Changes the state to FIN_WAIT1 and sends a segment with FIN and ACK flags.
 */
void TransmissionControlProtocolSocket::Disconnect()
{
    backend->Disconnect(this);
}


/*
 * ----------------------------------------------------------------------------
 * TransmissionControlProtocolProvider
 * ----------------------------------------------------------------------------
 *
 * Implements the TCP protocol over IP. It inherits from InternetProtocolHandler,
 * meaning that it receives IPv4 packets with protocol number 6 (TCP) and processes
 * them, dispatching them to the appropriate TCP socket.
 *
 * It maintains an array of pointers to TransmissionControlProtocolSocket objects,
 * keeps track of the number of active sockets, and assigns free local ports for new connections.
 */

/*
 * Constructor:
 *  - backend: Pointer to the InternetProtocolProvider that handles IPv4.
 *
 * Initializes:
 *   - Registers itself to handle TCP packets (protocol 0x06) via the InternetProtocolHandler base.
 *   - Initializes the sockets array to 0.
 *   - Sets the initial number of sockets to 0.
 *   - Sets freePort to 1024 (ephemeral port numbers start at 1024).
 */
TransmissionControlProtocolProvider::TransmissionControlProtocolProvider(InternetProtocolProvider* backend)
: InternetProtocolHandler(backend, 0x06) // 0x06 is the protocol number for TCP.
{
    for(int i = 0; i < 65535; i++)
        sockets[i] = 0;
    numSockets = 0;
    freePort = 1024;
}

/*
 * Destructor:
 *  - Currently does nothing. In a complete system, it might clean up allocated sockets.
 */
TransmissionControlProtocolProvider::~TransmissionControlProtocolProvider()
{
}


/*
 * bigEndian32:
 *  - A helper function that converts a 32-bit integer from little-endian to big-endian or vice versa.
 *  - This is necessary for correctly interpreting TCP sequence and acknowledgement numbers,
 *    which are transmitted in network (big-endian) byte order.
 */
uint32_t bigEndian32(uint32_t x)
{
    return ((x & 0xFF000000) >> 24)
         | ((x & 0x00FF0000) >> 8)
         | ((x & 0x0000FF00) << 8)
         | ((x & 0x000000FF) << 24);
}


/*
 * OnInternetProtocolReceived:
 *  - Invoked when an IPv4 packet (with protocol 6, TCP) is received.
 *  - The function first validates that the payload is large enough to hold a TCP header.
 *  - It then casts the payload to a TransmissionControlProtocolHeader.
 *
 *  It searches for a corresponding TCP socket:
 *    - If a socket is in the LISTEN state with a matching local port and IP and the received packet has SYN set,
 *      it is considered a new connection request.
 *    - Otherwise, if a socket matches both local and remote addresses/ports, it is selected.
 *
 *  Based on the TCP flags in the header, the function updates the socket's state:
 *    - SYN: For new connection requests (LISTEN state), the socket state changes to SYN_RECEIVED,
 *           local/remote ports and IPs are set, and an acknowledgment (SYN+ACK) is sent.
 *    - SYN | ACK: For a socket in SYN_SENT state (client mode), the connection is established,
 *                 an ACK is sent, and the state becomes ESTABLISHED.
 *    - FIN and FIN|ACK: Manage connection termination sequences.
 *    - ACK: Processes normal data transmission and piggybacked acknowledgments.
 *
 *  If the sequence number matches the expected acknowledgement number,
 *  the socket's handler is called to process the TCP payload.
 *  If data is processed successfully, the acknowledgement number is advanced,
 *  and an ACK is sent. If not, or if packets are out of order, a reset (RST) is triggered.
 *
 *  If the packet signals a reset, the provider sends a reset segment (RST)
 *  to the peer; if no appropriate socket exists, a temporary socket is created
 *  for this purpose.
 *
 *  Finally, if a socket becomes CLOSED, it is removed from the sockets array
 *  and its memory is freed.
 *
 * Returns:
 *   - false, as the return value is used to indicate if a packet should be echoed back.
 */
bool TransmissionControlProtocolProvider::OnInternetProtocolReceived(uint32_t srcIP_BE, uint32_t dstIP_BE,
                                        uint8_t* internetprotocolPayload, uint32_t size)
{
    // Ensure the received data is large enough to contain a minimum TCP header.
    if(size < 20)
        return false;
    TransmissionControlProtocolHeader* msg = (TransmissionControlProtocolHeader*)internetprotocolPayload;

    uint16_t localPort = msg->dstPort;
    uint16_t remotePort = msg->srcPort;
    
    TransmissionControlProtocolSocket* socket = 0;
    // Search through active sockets for a matching connection or a listening socket.
    for(uint16_t i = 0; i < numSockets && socket == 0; i++)
    {
        if( sockets[i]->localPort == msg->dstPort
         && sockets[i]->localIP == dstIP_BE
         && sockets[i]->state == LISTEN
         && (((msg->flags) & (SYN | ACK)) == SYN))
            socket = sockets[i];
        else if( sockets[i]->localPort == msg->dstPort
              && sockets[i]->localIP == dstIP_BE
              && sockets[i]->remotePort == msg->srcPort
              && sockets[i]->remoteIP == srcIP_BE)
            socket = sockets[i];
    }

    bool reset = false;
    
    // If a matching socket exists and the RST flag is set, mark the socket as CLOSED.
    if(socket != 0 && msg->flags & RST)
        socket->state = CLOSED;
    
    if(socket != 0 && socket->state != CLOSED)
    {
        // Process the TCP flags (SYN, ACK, FIN) to drive the state machine.
        switch((msg->flags) & (SYN | ACK | FIN))
        {
            case SYN:
                // Incoming connection request
                if(socket->state == LISTEN)
                {
                    socket->state = SYN_RECEIVED;
                    socket->remotePort = msg->srcPort;
                    socket->remoteIP = srcIP_BE;
                    // Set acknowledgement to sequence number + 1 (converted to big-endian)
                    socket->acknowledgementNumber = bigEndian32(msg->sequenceNumber) + 1;
                    // Initialize sequence number to a chosen value (0xbeefcafe)
                    socket->sequenceNumber = 0xbeefcafe;
                    // Send a SYN+ACK response to acknowledge the connection attempt.
                    Send(socket, 0, 0, SYN | ACK);
                    socket->sequenceNumber++;
                }
                else
                    reset = true;
                break;

            case SYN | ACK:
                // Response to our connection request.
                if(socket->state == SYN_SENT)
                {
                    socket->state = ESTABLISHED;
                    socket->acknowledgementNumber = bigEndian32(msg->sequenceNumber) + 1;
                    socket->sequenceNumber++;
                    // Send an ACK to complete the three-way handshake.
                    Send(socket, 0, 0, ACK);
                }
                else
                    reset = true;
                break;
                
            case SYN | FIN:
            case SYN | FIN | ACK:
                // Invalid combination: simultaneous SYN and FIN indicates an error.
                reset = true;
                break;

            case FIN:
            case FIN | ACK:
                // Connection termination sequence.
                if(socket->state == ESTABLISHED)
                {
                    socket->state = CLOSE_WAIT;
                    socket->acknowledgementNumber++;
                    Send(socket, 0, 0, ACK);
                    Send(socket, 0, 0, FIN | ACK);
                }
                else if(socket->state == CLOSE_WAIT)
                {
                    socket->state = CLOSED;
                }
                else if(socket->state == FIN_WAIT1 || socket->state == FIN_WAIT2)
                {
                    socket->state = CLOSED;
                    socket->acknowledgementNumber++;
                    Send(socket, 0, 0, ACK);
                }
                else
                    reset = true;
                break;
                
            case ACK:
                if(socket->state == SYN_RECEIVED)
                {
                    socket->state = ESTABLISHED;
                    return false;
                }
                else if(socket->state == FIN_WAIT1)
                {
                    socket->state = FIN_WAIT2;
                    return false;
                }
                else if(socket->state == CLOSE_WAIT)
                {
                    socket->state = CLOSED;
                    break;
                }
                
                if(msg->flags == ACK)
                    break;
                
                // Falls through if data is piggybacked with ACK.
            default:
                // Check if the sequence number in the packet (converted to big-endian)
                // matches what we expect (socket->acknowledgementNumber). If so, process the payload.
                if(bigEndian32(msg->sequenceNumber) == socket->acknowledgementNumber)
                {
                    // Call the socket's handler to process the TCP payload.
                    // If the handler returns false (i.e., fails to process the data), signal a reset.
                    reset = !(socket->HandleTransmissionControlProtocolMessage(
                        internetprotocolPayload + msg->headerSize32 * 4,
                        size - msg->headerSize32 * 4));
                    if(!reset)
                    {
                        int x = 0;
                        // Determine how many bytes of data were processed by inspecting the payload.
                        for(int i = msg->headerSize32 * 4; i < size; i++)
                            if(internetprotocolPayload[i] != 0)
                                x = i;
                        // Update the acknowledgement number to reflect the received data.
                        socket->acknowledgementNumber += x - msg->headerSize32 * 4 + 1;
                        // Send an ACK to acknowledge the received data.
                        Send(socket, 0, 0, ACK);
                    }
                }
                else
                {
                    // If the sequence number does not match, consider the packet out of order.
                    reset = true;
                }
        }
    }
    
    // If a reset is required, send an RST segment to abort the connection.
    if(reset)
    {
        if(socket != 0)
        {
            Send(socket, 0, 0, RST);
        }
        else
        {
            // If no matching socket, create a temporary socket for sending RST.
            TransmissionControlProtocolSocket socket(this);
            socket.remotePort = msg->srcPort;
            socket.remoteIP = srcIP_BE;
            socket.localPort = msg->dstPort;
            socket.localIP = dstIP_BE;
            socket.sequenceNumber = bigEndian32(msg->acknowledgementNumber);
            socket.acknowledgementNumber = bigEndian32(msg->sequenceNumber) + 1;
            Send(&socket, 0, 0, RST);
        }
    }
    
    // If a socket has reached the CLOSED state, remove it from the sockets array
    // and free its allocated memory.
    if(socket != 0 && socket->state == CLOSED)
        for(uint16_t i = 0; i < numSockets && socket == 0; i++)
            if(sockets[i] == socket)
            {
                sockets[i] = sockets[--numSockets];
                MemoryManager::activeMemoryManager->free(socket);
                break;
            }
    
    return false;
}


/*
 * ----------------------------------------------------------------------------
 * TransmissionControlProtocolProvider::Send
 * ----------------------------------------------------------------------------
 *
 * This method constructs and sends a TCP segment for a given socket.
 * It builds the TCP header, a pseudo-header for checksum calculation, appends the payload,
 * computes the checksum for the combined header and pseudo-header, and then sends the packet via the IP layer.
 *
 * Parameters:
 *   - socket: Pointer to the TCP socket from which the segment is sent.
 *   - data: Pointer to the TCP payload data.
 *   - size: Size of the payload.
 *   - flags: TCP control flags (such as SYN, ACK, FIN, etc.).
 */
void TransmissionControlProtocolProvider::Send(TransmissionControlProtocolSocket* socket, uint8_t* data, uint16_t size, uint16_t flags)
{
    // Calculate total TCP segment length (header + payload).
    uint16_t totalLength = size + sizeof(TransmissionControlProtocolHeader);
    // Also calculate length including the pseudo-header for checksum computation.
    uint16_t lengthInclPHdr = totalLength + sizeof(TransmissionControlProtocolPseudoHeader);
    
    // Allocate a buffer to hold the pseudo-header, TCP header, and payload.
    uint8_t* buffer = (uint8_t*)MemoryManager::activeMemoryManager->malloc(lengthInclPHdr);
    
    // Set up pointers to the pseudo-header, the TCP header, and the data payload in the buffer.
    TransmissionControlProtocolPseudoHeader* phdr = (TransmissionControlProtocolPseudoHeader*)buffer;
    TransmissionControlProtocolHeader* msg = (TransmissionControlProtocolHeader*)(buffer + sizeof(TransmissionControlProtocolPseudoHeader));
    uint8_t* buffer2 = buffer + sizeof(TransmissionControlProtocolPseudoHeader) + sizeof(TransmissionControlProtocolHeader);
    
    // Fill in TCP header fields:
    msg->headerSize32 = sizeof(TransmissionControlProtocolHeader) / 4;
    msg->srcPort = socket->localPort;
    msg->dstPort = socket->remotePort;
    
    // Set sequence and acknowledgement numbers (converted to big-endian).
    msg->acknowledgementNumber = bigEndian32(socket->acknowledgementNumber);
    msg->sequenceNumber = bigEndian32(socket->sequenceNumber);
    msg->reserved = 0;
    msg->flags = flags;
    msg->windowSize = 0xFFFF;
    msg->urgentPtr = 0;
    
    // Set TCP options if SYN flag is present.
    msg->options = ((flags & SYN) != 0) ? 0xB4050402 : 0;
    
    // Increase the sequence number by the size of the payload.
    socket->sequenceNumber += size;
        
    // Copy the payload data into the buffer after the TCP header.
    for(int i = 0; i < size; i++)
        buffer2[i] = data[i];
    
    // Build the pseudo-header for checksum computation.
    phdr->srcIP = socket->localIP;
    phdr->dstIP = socket->remoteIP;
    phdr->protocol = 0x0600;  // TCP protocol, represented in big-endian.
    // totalLength is byte-swapped to network byte order.
    phdr->totalLength = ((totalLength & 0x00FF) << 8) | ((totalLength & 0xFF00) >> 8);
    
    // Calculate TCP checksum:
    //  - First set checksum to 0, then compute the checksum over the pseudo-header plus the TCP header and payload.
    msg->checksum = 0;
    msg->checksum = InternetProtocolProvider::Checksum((uint16_t*)buffer, lengthInclPHdr);

    // Send the TCP segment using the InternetProtocolHandler's Send method.
    // It will be encapsulated in an IP packet and sent over the network.
    InternetProtocolHandler::Send(socket->remoteIP, (uint8_t*)msg, totalLength);

    // Free the allocated buffer.
    MemoryManager::activeMemoryManager->free(buffer);
}


/*
 * ----------------------------------------------------------------------------
 * TransmissionControlProtocolProvider::Connect
 * ----------------------------------------------------------------------------
 *
 * Establishes a new outbound TCP connection.
 *
 * Parameters:
 *   - ip: The remote IP address (in big-endian format) to connect to.
 *   - port: The remote TCP port.
 *
 * Steps:
 *   1. Allocate memory for a new TCP socket.
 *   2. Construct the socket using placement new.
 *   3. Set the remote IP and port.
 *   4. Choose an available local port from the freePort counter.
 *   5. Set the local IP to the NIC's IP address.
 *   6. Byte-swap the port numbers (to network order).
 *   7. Add the new socket to the sockets array.
 *   8. Set the state to SYN_SENT.
 *   9. Initialize a starting sequence number.
 *   10. Send an initial SYN segment to begin the TCP handshake.
 *
 * Returns:
 *   - A pointer to the new TransmissionControlProtocolSocket, or 0 if allocation failed.
 */
TransmissionControlProtocolSocket* TransmissionControlProtocolProvider::Connect(uint32_t ip, uint16_t port)
{
    TransmissionControlProtocolSocket* socket = (TransmissionControlProtocolSocket*)MemoryManager::activeMemoryManager->malloc(sizeof(TransmissionControlProtocolSocket));
    
    if(socket != 0)
    {
        new (socket) TransmissionControlProtocolSocket(this);
        
        socket->remotePort = port;
        socket->remoteIP = ip;
        socket->localPort = freePort++;
        socket->localIP = backend->GetIPAddress();
        
        // Convert port numbers to network byte order (swap bytes)
        socket->remotePort = ((socket->remotePort & 0xFF00) >> 8) | ((socket->remotePort & 0x00FF) << 8);
        socket->localPort = ((socket->localPort & 0xFF00) >> 8) | ((socket->localPort & 0x00FF) << 8);
        
        sockets[numSockets++] = socket;
        socket->state = SYN_SENT;
        
        socket->sequenceNumber = 0xbeefcafe;
        
        // Send a SYN segment to initiate the TCP handshake.
        Send(socket, 0, 0, SYN);
    }
    
    return socket;
}


/*
 * ----------------------------------------------------------------------------
 * TransmissionControlProtocolProvider::Disconnect
 * ----------------------------------------------------------------------------
 *
 * Gracefully disconnects a TCP connection.
 *
 * For a given socket, sets the state to FIN_WAIT1 and sends a FIN+ACK segment,
 * then increments the sequence number.
 */
void TransmissionControlProtocolProvider::Disconnect(TransmissionControlProtocolSocket* socket)
{
    socket->state = FIN_WAIT1;
    Send(socket, 0, 0, FIN + ACK);
    socket->sequenceNumber++;
}


/*
 * ----------------------------------------------------------------------------
 * TransmissionControlProtocolProvider::Listen
 * ----------------------------------------------------------------------------
 *
 * Creates a new TCP socket that listens for incoming connections on a specific port.
 *
 * Parameters:
 *   - port: The local TCP port on which to listen for incoming connection requests.
 *
 * Steps:
 *   1. Allocate memory for a new socket.
 *   2. Construct the socket via placement new.
 *   3. Set the socket state to LISTEN.
 *   4. Set the local IP and local port (converted to network order).
 *   5. Add the socket to the sockets array.
 *
 * Returns:
 *   - Pointer to the new listening socket.
 */
TransmissionControlProtocolSocket* TransmissionControlProtocolProvider::Listen(uint16_t port)
{
    TransmissionControlProtocolSocket* socket = (TransmissionControlProtocolSocket*)MemoryManager::activeMemoryManager->malloc(sizeof(TransmissionControlProtocolSocket));
    
    if(socket != 0)
    {
        new (socket) TransmissionControlProtocolSocket(this);
        
        socket->state = LISTEN;
        socket->localIP = backend->GetIPAddress();
        // Convert the port to network byte order.
        socket->localPort = ((port & 0xFF00) >> 8) | ((port & 0x00FF) << 8);
        
        sockets[numSockets++] = socket;
    }
    
    return socket;
}

/*
 * Bind:
 *  - Associates a particular TransmissionControlProtocolHandler with a TCP socket.
 *  - This allows the application or higher-level protocol to process data received on this socket.
 *
 * Parameters:
 *   - socket: The TCP socket to bind.
 *   - handler: The handler that will process received TCP segments.
 */
void TransmissionControlProtocolProvider::Bind(TransmissionControlProtocolSocket* socket, TransmissionControlProtocolHandler* handler)
{
    socket->handler = handler;
}
