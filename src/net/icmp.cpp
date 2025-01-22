#include <net/icmp.h>
 
using namespace myos;
using namespace myos::common;
using namespace myos::net;
 
/*
 * ----------------------------------------------------------------------------
 * InternetControlMessageProtocol Class
 * ----------------------------------------------------------------------------
 *
 * This class implements a basic handler for the Internet Control Message Protocol (ICMP),
 * which is used for network diagnostic or error-reporting functions—for example,
 * the "ping" command (which sends ICMP Echo Request messages and receives Echo Replies).
 *
 * InternetControlMessageProtocol inherits from InternetProtocolHandler, which means it 
 * processes IP packets whose protocol field is 0x01 (ICMP). The ICMP messages are 
 * encapsulated in IP packets and are forwarded to this handler by the InternetProtocolProvider.
 */

/*
 * Constructor:
 *   - backend: A pointer to the InternetProtocolProvider, which is responsible for handling
 *              all incoming IP packets. Passing in the appropriate EtherType is handled by 
 *              InternetProtocolHandler's constructor.
 *
 * Here, we specify 0x01 (ICMP protocol) as the protocol number. The base class constructor 
 * will register this handler with the backend.
 */
InternetControlMessageProtocol::InternetControlMessageProtocol(InternetProtocolProvider* backend)
: InternetProtocolHandler(backend, 0x01)
{
}
 
/*
 * Destructor:
 *  - Currently, no cleanup is necessary.
 */
InternetControlMessageProtocol::~InternetControlMessageProtocol()
{
}
            
/*
 * printf and printfHex functions are declared externally and used here for debugging
 * and informational output.
 */
void printf(char*);
void printfHex(uint8_t);

/*
 * OnInternetProtocolReceived:
 *   - This function is called by the InternetProtocolProvider when an ICMP packet arrives.
 *
 * Parameters:
 *   - srcIP_BE: Source IP address in big-endian format.
 *   - dstIP_BE: Destination IP address in big-endian format.
 *   - internetprotocolPayload: Pointer to the ICMP message payload (immediately after the IP header).
 *   - size: Size in bytes of the ICMP payload.
 *
 * Functionality:
 *   - First, it checks that the size of the payload is at least large enough to hold an 
 *     InternetControlMessageProtocolMessage. If not, it returns false.
 *   - It then casts the payload to an InternetControlMessageProtocolMessage pointer.
 *
 *   - The handler examines the "type" field of the ICMP message:
 *       * If the type is 0, this indicates an Echo Reply (ping response).
 *         The handler prints a message showing the source IP address.
 *         The IP is split into four bytes and printed as hexadecimal values separated by dots.
 *
 *       * If the type is 8, this indicates an Echo Request (ping request).
 *         The handler converts the request into an Echo Reply by:
 *             - Changing the type field from 8 to 0.
 *             - Resetting the checksum to 0.
 *             - Recalculating the checksum for the modified message.
 *         It then returns true to indicate that a reply should be sent.
 *
 *   - For any other type, or if no special handling is needed, the function returns false.
 *
 * Returns:
 *   - true if an Echo Request was processed and a response should be sent.
 *   - false otherwise.
 */
bool InternetControlMessageProtocol::OnInternetProtocolReceived(common::uint32_t srcIP_BE, common::uint32_t dstIP_BE,
                                            common::uint8_t* internetprotocolPayload, common::uint32_t size)
{
    if(size < sizeof(InternetControlMessageProtocolMessage))
        return false;
    
    InternetControlMessageProtocolMessage* msg = (InternetControlMessageProtocolMessage*)internetprotocolPayload;
    
    switch(msg->type)
    {
        case 0:
            // ICMP Echo Reply
            printf("ping response from ");
            // Print source IP address byte by byte in hexadecimal (displayed in network order)
            printfHex(srcIP_BE & 0xFF);
            printf(".");
            printfHex((srcIP_BE >> 8) & 0xFF);
            printf(".");
            printfHex((srcIP_BE >> 16) & 0xFF);
            printf(".");
            printfHex((srcIP_BE >> 24) & 0xFF);
            printf("\n");
            break;
            
        case 8:
            // ICMP Echo Request: Prepare an Echo Reply.
            msg->type = 0;  // Set type to 0 to indicate an Echo Reply.
            msg->checksum = 0;  // Reset checksum to zero before recalculation.
            msg->checksum = InternetProtocolProvider::Checksum((uint16_t*)msg,
                sizeof(InternetControlMessageProtocolMessage));
            return true;  // Return true so that the calling code knows to send the reply.
    }
    
    return false;
}

/*
 * RequestEchoReply:
 *   - Constructs and sends an ICMP Echo Request to the specified IP address.
 *
 * Parameters:
 *   - ip_be: The target IP address in big-endian format.
 *
 * Functionality:
 *   - Fills in an InternetControlMessageProtocolMessage structure:
 *       * type: set to 8 (indicating an Echo Request).
 *       * code: set to 0 (no special code).
 *       * data: set arbitrarily to 0x3713 (this example uses a hardcoded data value such as an identifier or sequence number).
 *       * checksum: initially set to 0, then recalculated using the Checksum function.
 *   - Finally, sends the ICMP packet by calling the Send() method from the base class, 
 *     which packages the ICMP message into an IP packet and transmits it.
 */
void InternetControlMessageProtocol::RequestEchoReply(uint32_t ip_be)
{
    InternetControlMessageProtocolMessage icmp;
    icmp.type = 8; // Set to ICMP Echo Request (ping).
    icmp.code = 0;
    icmp.data = 0x3713; // Example arbitrary data (could be used as an identifier or sequence number).
    icmp.checksum = 0;
    icmp.checksum = InternetProtocolProvider::Checksum((uint16_t*)&icmp,
        sizeof(InternetControlMessageProtocolMessage));
    
    // Send the ICMP Echo Request message to the target IP address.
    InternetProtocolHandler::Send(ip_be, (uint8_t*)&icmp, sizeof(InternetControlMessageProtocolMessage));
}
