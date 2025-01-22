#include <net/udp.h>
using namespace myos;
using namespace myos::common;
using namespace myos::net;

/*
 * ----------------------------------------------------------------------------
 * UserDatagramProtocolHandler Class
 * ----------------------------------------------------------------------------
 *
 * This class defines the interface for handling UDP (User Datagram Protocol)
 * messages received on a UDP socket. It is intended to be subclassed by
 * applications or higher-level protocols to process incoming UDP datagrams.
 *
 * The default implementation provided here does nothing.
 */

UserDatagramProtocolHandler::UserDatagramProtocolHandler()
{
}

UserDatagramProtocolHandler::~UserDatagramProtocolHandler()
{
}

/*
 * HandleUserDatagramProtocolMessage:
 *  This virtual method is invoked when a UDP datagram is received on a socket.
 *  Parameters:
 *    - socket: A pointer to the UDP socket on which the datagram was received.
 *    - data: A pointer to the data payload of the UDP datagram.
 *    - size: The size, in bytes, of the data payload.
 *
 *  The default implementation in this base class does nothing.
 */
void UserDatagramProtocolHandler::HandleUserDatagramProtocolMessage(UserDatagramProtocolSocket* socket, uint8_t* data, uint16_t size)
{
}





/*
 * ----------------------------------------------------------------------------
 * UserDatagramProtocolSocket Class
 * ----------------------------------------------------------------------------
 *
 * This class encapsulates a UDP socket. A UDP socket represents an endpoint
 * that can send and receive UDP datagrams. It stores connection-related
 * information such as local and remote IP addresses, local and remote ports,
 * and a pointer to a handler that processes incoming UDP messages.
 *
 * The "listening" flag indicates whether the socket is in a state where it is
 * waiting for incoming datagrams (as a server), as opposed to being connected
 * to a remote endpoint.
 */

UserDatagramProtocolSocket::UserDatagramProtocolSocket(UserDatagramProtocolProvider* backend)
{
    this->backend = backend;
    handler = 0;       // No handler is attached initially.
    listening = false; // By default, the socket is not in a "listening" state.
}

UserDatagramProtocolSocket::~UserDatagramProtocolSocket()
{
}

/*
 * HandleUserDatagramProtocolMessage:
 *  When a UDP datagram is received for this socket, if a handler has been bound
 *  to the socket, the datagram is forwarded to the handler.
 *
 * Parameters:
 *    - data: Pointer to the UDP payload data (after the UDP header).
 *    - size: Size in bytes of the UDP payload.
 */
void UserDatagramProtocolSocket::HandleUserDatagramProtocolMessage(uint8_t* data, uint16_t size)
{
    if(handler != 0)
        handler->HandleUserDatagramProtocolMessage(this, data, size);
}

/*
 * Send:
 *  Sends the given data as a UDP datagram from this socket.
 *  It delegates the sending operation to the UDP provider.
 *
 * Parameters:
 *    - data: Pointer to the payload data to be sent.
 *    - size: The size (in bytes) of the payload.
 */
void UserDatagramProtocolSocket::Send(uint8_t* data, uint16_t size)
{
    backend->Send(this, data, size);
}

/*
 * Disconnect:
 *  - Instructs the UDP provider to disconnect (free) this socket.
 *  - Once a socket is disconnected, it will no longer receive datagrams.
 */
void UserDatagramProtocolSocket::Disconnect()
{
    backend->Disconnect(this);
}





/*
 * ----------------------------------------------------------------------------
 * UserDatagramProtocolProvider Class
 * ----------------------------------------------------------------------------
 *
 * This class implements the UDP protocol over IP. It receives IPv4 packets with
 * protocol number 17 (UDP) through its base class InternetProtocolHandler.
 *
 * The provider maintains an array of pointers to UserDatagramProtocolSocket
 * objects, which represent individual UDP endpoints. It is responsible for
 * demultiplexing incoming UDP datagrams to the appropriate socket based on the
 * destination port and IP address.
 *
 * It also allows for creating new UDP sockets via Connect() or Listen() methods,
 * and binding higher-level handlers to these sockets.
 */

UserDatagramProtocolProvider::UserDatagramProtocolProvider(InternetProtocolProvider* backend)
: InternetProtocolHandler(backend, 0x11) // 0x11 is the protocol number for UDP.
{
    // Initialize the sockets array to zero.
    for(int i = 0; i < 65535; i++)
        sockets[i] = 0;
    numSockets = 0;
    freePort = 1024;  // Start allocating ephemeral ports from 1024.
}

UserDatagramProtocolProvider::~UserDatagramProtocolProvider()
{
}

/*
 * OnInternetProtocolReceived:
 *  This method is called when an IP packet carrying a UDP segment is received.
 *
 *  Parameters:
 *    - srcIP_BE: Source IP address (big-endian) of the UDP packet.
 *    - dstIP_BE: Destination IP address (big-endian) of the UDP packet.
 *    - internetprotocolPayload: Pointer to the payload of the IP packet (starting at the UDP header).
 *    - size: Total size in bytes of the UDP segment, including the UDP header.
 *
 *  It performs the following steps:
 *    1. Verifies that the size is sufficient to hold a UDP header.
 *    2. Casts the payload to a UserDatagramProtocolHeader to extract UDP header fields.
 *    3. Searches through the active UDP sockets for one that matches either:
 *         a) A socket in a listening state with a matching local port, or
 *         b) A fully connected socket matching both local and remote addresses/ports.
 *    4. If a listening socket is found, it is taken out of the listening state and initialized with the remote IP and port.
 *    5. If a matching socket is found, the UDP payload (data after the header) is passed to the socket’s handler.
 *
 *  Returns:
 *    - false, meaning that no echo-back mechanism is implemented by default.
 */
bool UserDatagramProtocolProvider::OnInternetProtocolReceived(uint32_t srcIP_BE, uint32_t dstIP_BE,
                                        uint8_t* internetprotocolPayload, uint32_t size)
{
    // Verify the packet is large enough to contain the UDP header.
    if(size < sizeof(UserDatagramProtocolHeader))
        return false;
    
    // Interpret the beginning of the payload as a UDP header.
    UserDatagramProtocolHeader* msg = (UserDatagramProtocolHeader*)internetprotocolPayload;
    uint16_t localPort = msg->dstPort;
    uint16_t remotePort = msg->srcPort;
    
    UserDatagramProtocolSocket* socket = 0;
    // Search through all allocated UDP sockets to find a match.
    for(uint16_t i = 0; i < numSockets && socket == 0; i++)
    {
        // Case 1: a socket in a listening state with a matching local port and IP.
        if(sockets[i]->localPort == msg->dstPort
           && sockets[i]->localIP == dstIP_BE
           && sockets[i]->listening)
        {
            socket = sockets[i];
            // The socket is no longer just listening; we record the remote endpoint details.
            socket->listening = false;
            socket->remotePort = msg->srcPort;
            socket->remoteIP = srcIP_BE;
        }
        // Case 2: an already connected socket matching both local and remote endpoints.
        else if(sockets[i]->localPort == msg->dstPort
                && sockets[i]->localIP == dstIP_BE
                && sockets[i]->remotePort == msg->srcPort
                && sockets[i]->remoteIP == srcIP_BE)
        {
            socket = sockets[i];
        }
    }
    
    // If a matching socket is found, pass the UDP payload (after the UDP header) to its handler.
    if(socket != 0)
        socket->HandleUserDatagramProtocolMessage(
            internetprotocolPayload + sizeof(UserDatagramProtocolHeader),
            size - sizeof(UserDatagramProtocolHeader)
        );
    
    return false;
}


/*
 * Connect:
 *  - Establishes a new outbound UDP connection.
 *  - Allocates and constructs a new UserDatagramProtocolSocket.
 *  - Sets up the socket with the remote IP and remote port, assigns a free local port,
 *    and sets the local IP from the underlying IP provider.
 *  - Converts both the remote and local port numbers to network byte order.
 *  - Adds the newly created socket to the internal sockets array and returns it.
 *
 * Parameters:
 *   - ip: Remote IP address (big-endian) to which to connect.
 *   - port: Remote UDP port.
 *
 * Returns:
 *   - Pointer to the new UserDatagramProtocolSocket, or 0 if allocation failed.
 */
UserDatagramProtocolSocket* UserDatagramProtocolProvider::Connect(uint32_t ip, uint16_t port)
{
    UserDatagramProtocolSocket* socket = (UserDatagramProtocolSocket*)MemoryManager::activeMemoryManager->malloc(sizeof(UserDatagramProtocolSocket));
    
    if(socket != 0)
    {
        // Construct the UDP socket in the allocated memory using placement new.
        new (socket) UserDatagramProtocolSocket(this);
        
        socket->remotePort = port;
        socket->remoteIP = ip;
        socket->localPort = freePort++;
        socket->localIP = backend->GetIPAddress();
        
        // Swap bytes for port numbers to convert them to network byte order.
        socket->remotePort = ((socket->remotePort & 0xFF00) >> 8) | ((socket->remotePort & 0x00FF) << 8);
        socket->localPort = ((socket->localPort & 0xFF00) >> 8) | ((socket->localPort & 0x00FF) << 8);
        
        sockets[numSockets++] = socket;
    }
    
    return socket;
}

/*
 * Listen:
 *  - Creates a UDP socket set to "listening" state, meaning it is ready to accept 
 *    incoming UDP datagrams on a particular local port.
 *  - Allocates and constructs a new UserDatagramProtocolSocket.
 *  - Sets the socket's local IP and local port (converted to network byte order),
 *    and marks it as listening.
 *  - Adds the new socket to the sockets array.
 *
 * Parameters:
 *   - port: The UDP port on which to listen for incoming datagrams.
 *
 * Returns:
 *   - Pointer to the new UserDatagramProtocolSocket.
 */
UserDatagramProtocolSocket* UserDatagramProtocolProvider::Listen(uint16_t port)
{
    UserDatagramProtocolSocket* socket = (UserDatagramProtocolSocket*)MemoryManager::activeMemoryManager->malloc(sizeof(UserDatagramProtocolSocket));
    
    if(socket != 0)
    {
        new (socket) UserDatagramProtocolSocket(this);
        
        socket->listening = true;
        socket->localIP = backend->GetIPAddress();
        // Convert the port number to network byte order.
        socket->localPort = ((port & 0xFF00) >> 8) | ((port & 0x00FF) << 8);
        
        sockets[numSockets++] = socket;
    }
    
    return socket;
}

/*
 * Disconnect:
 *  - Removes a UDP socket from active use.
 *  - Searches for the provided socket in the sockets array and, if found, removes it,
 *    and frees its allocated memory.
 *
 * Parameters:
 *   - socket: Pointer to the UserDatagramProtocolSocket to disconnect.
 */
void UserDatagramProtocolProvider::Disconnect(UserDatagramProtocolSocket* socket)
{
    for(uint16_t i = 0; i < numSockets && socket == 0; i++)
        if(sockets[i] == socket)
        {
            // Replace the socket with the last socket in the array.
            sockets[i] = sockets[--numSockets];
            MemoryManager::activeMemoryManager->free(socket);
            break;
        }
}

/*
 * Send:
 *  - Constructs and transmits a UDP datagram.
 *  - Allocates a buffer to hold the complete UDP packet (UDP header + payload).
 *  - Fills the UDP header fields:
 *       * srcPort: Taken from the socket's local port.
 *       * dstPort: Taken from the socket's remote port.
 *       * length: Total length of the UDP packet (header + data), converted to network byte order.
 *  - Copies the payload data into the allocated buffer after the header.
 *  - Calls the InternetProtocolHandler::Send method to send the UDP packet, which encapsulates
 *    it in an IP packet and transmits it over the network.
 *  - Finally, frees the allocated buffer.
 *
 * Parameters:
 *   - socket: The UDP socket through which to send the datagram.
 *   - data: Pointer to the UDP payload data.
 *   - size: Size in bytes of the payload.
 */
void UserDatagramProtocolProvider::Send(UserDatagramProtocolSocket* socket, uint8_t* data, uint16_t size)
{
    uint16_t totalLength = size + sizeof(UserDatagramProtocolHeader);
    // Allocate a buffer for the UDP header and payload.
    uint8_t* buffer = (uint8_t*)MemoryManager::activeMemoryManager->malloc(totalLength);
    // Pointer to the payload portion, immediately after the UDP header.
    uint8_t* buffer2 = buffer + sizeof(UserDatagramProtocolHeader);
    
    // Cast the beginning of the buffer to a UDP header.
    UserDatagramProtocolHeader* msg = (UserDatagramProtocolHeader*)buffer;
    
    // Fill in the UDP header fields.
    msg->srcPort = socket->localPort;
    msg->dstPort = socket->remotePort;
    // Set the length of the UDP packet and convert it to network byte order.
    msg->length = ((totalLength & 0x00FF) << 8) | ((totalLength & 0xFF00) >> 8);
    
    // Copy the payload data into the buffer after the UDP header.
    for(int i = 0; i < size; i++)
        buffer2[i] = data[i];
    
    // For UDP, the checksum is often optional, so we simply set it to 0.
    msg->checksum = 0;
    
    // Send the UDP packet through the InternetProtocolHandler which encapsulates it in an IP packet.
    InternetProtocolHandler::Send(socket->remoteIP, buffer, totalLength);
    
    // Free the allocated memory.
    MemoryManager::activeMemoryManager->free(buffer);
}

/*
 * Bind:
 *  - Associates a higher-level UDP message handler with a specific UDP socket.
 *  - This enables the socket to forward incoming UDP datagrams to the application
 *    or protocol that is interested in them.
 *
 * Parameters:
 *    - socket: Pointer to the UDP socket.
 *    - handler: Pointer to the UserDatagramProtocolHandler that will process received messages.
 */
void UserDatagramProtocolProvider::Bind(UserDatagramProtocolSocket* socket, UserDatagramProtocolHandler* handler)
{
    socket->handler = handler;
}
