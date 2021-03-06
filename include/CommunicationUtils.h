#ifndef COMMUNICATIONUTILS_H
#define COMMUNICATIONUTILS_H

#include <sys/socket.h>
#include <string>
#include <cstring>
#include <errno.h>
#include <signal.h>
#include <chrono>

#include "data_types.h"
#include "constants.h"

class CommunicationUtils
{

protected:
    // Protected methods
    static std::string appendErrorMessage(const std::string message); // Add details of the error to the message

    /**
     * @brief Composes a coordniator packetfor updating session mappings
     * @param translation The old_socket to new_socket mapping
     * @returns Pointer to the allocated structure 
     */
    static coordinator *composeCoordinatorUpdate(std::map<int, int> translation);

    /**
     * @brief Composes a message to start a election
     * @param vote      Value of the vote
     * @param type      Type of the message. (see constants.h)
     * @returns Pointer to allocated structure
     */
    static election_message *composeElectionMessage(uint16_t vote, uint16_t type);

    /**
     * @brief Composes a front-end register structure with the ip and port
     * @param login  Packet with the login information
     * @param ip     Front end IP
     * @param port   Front end port
     * @param socket Socket where the connection came from
     * @returns Pointer to allocated structure
     */
    static login_update *composeLoginUpdate(char *login, std::string ip, int port, int socket);

    /**
     * @brief Composes a message update structure, with the groupname and socket where it came from
     * @param message   The message record
     * @param groupname Name of the group where this message is headed
     * @param socket    Socket where the message came from
     * @returns Pointer to allocated structure
     */
    static message_update *composeMessageUpdate(message_record *message, std::string groupname, int socket);

    /**
     * @brief Composes a replica update structure, for new replicas to catch up with the rest of the list
     * @param identifier Replica's unique identifier
     * @param port       Replica's listening port
     * @returns Pointer to allocated structure
     */
    static replica_update *composeReplicaUpdate(int identifier, int port);

    /**
     * @brief Composes a packet with the provided data
     * @param packet_type Type of packet to be created (see constants.h)
     * @param payload Packet data
     * @param payload_size Size of the data provided in payload
     * @returns Pointer to allocated structure
     */
    static packet *composePacket(int packet_type, char *payload, int payload_size);

    /**
     * @brief Sends a packet with given payload to the provided socket descriptor
     * @param socket Socket descriptor where the packet will be sent
     * @param packet_type Type of packet that should be sent (see constants.h)
     * @param payload Data that will be sent through that socket
     * @param payload_size Size of the data provided in payload
     * @returns Number of bytes sent
     */
    static int sendPacket(int socket, int packet_type, char *payload, int payload_size);

    /**
     * @brief Creates a struct of type message_record with the provided data
     * @param sender_name Username of the user who sent this message
     * @param message_content Actual chat message
     * @param message_type Type of message 
     * @param port The port where the sender is listening for reconnects (None by default, regular messages)
     * @returns Pointer to the created message record
     */
    static message_record *composeMessage(std::string sender_name, std::string message_content, int message_type, int port = 0xFFFF);

    /**
     * @brief Tries to fully receive a packet from the informed socket, putting it in buffer
     * @param   socket From whence to receive the packet
     * @param   buffer Buffer where received data should be put
     * @param   buf_size Max size of the passed buffer
     * @returns Number of bytes received and put into buffer
     */
    static int receivePacket(int socket, char *buffer, int buf_size);
};

#endif