#include "Client.h"

std::atomic<bool> Client::stop_issued;
int Client::server_socket;
pthread_t Client::input_handler_thread;
pthread_t Client::keep_alive_thread;
std::string Client::username;
RW_Monitor Client::socket_monitor;

Client::Client(std::string username, std::string groupname, std::string server_ip, std::string server_port)
{
    // Validate username
    if ( !std::regex_match(username,std::regex(NAME_REGEX)) )
        throw std::invalid_argument("Invalid username format");

    // Valide groupname
    if ( !std::regex_match(groupname, std::regex(NAME_REGEX)))
        throw std::invalid_argument("Invalid group name format");

    // Validate IP
    if (!std::regex_match(server_ip, std::regex(IP_REGEX)))
        throw std::invalid_argument("Invalid IP format");

    // TODO Validate port maybe?

    // Initialize values
    this->username = username;
    this->groupname = groupname;
    this->server_ip = server_ip;
    this->server_port = stoi(server_port);
    this->server_socket = -1;

    // Set atomic flag as false
    stop_issued = false;

    // Initialize client interface
    ClientInterface::init(groupname);   
};

Client::~Client()
{
    // Finalize client interface
    ClientInterface::destroy();

    // Close server socket if it is still open
    if (server_socket > 0)
        close(server_socket);
};

void Client::setupConnection()
{
    int payload_size; // Payload size for the first packet(login packet)

    // Create socket
    if ( (server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        throw std::runtime_error(appendErrorMessage("Error during socket creation"));

    // Fill server socket address
    server_address.sin_family = AF_INET;
    server_address.sin_port   = htons(server_port);
    server_address.sin_addr.s_addr = inet_addr(server_ip.c_str());

    // Try to connect to remote server
    if (connect(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
        throw std::runtime_error(appendErrorMessage("Error connecting to server"));

    // Prepare login content payload
    login_payload lp;
    sprintf(lp.username, "%s", username.c_str());
    sprintf(lp.groupname, "%s", groupname.c_str());
    payload_size = sizeof(lp);

    // Sends the command packet to the server
    CommunicationUtils::sendPacket(server_socket, PAK_COMMAND, (char*)&lp, payload_size); 

    // Start user input getter thread
    pthread_create(&input_handler_thread, NULL, handleUserInput, NULL);

    // Start keep-alive thread
    pthread_create(&keep_alive_thread, NULL, keepAlive, NULL);
};

void Client::getMessages()
{
    int read_bytes = -1;                // Number of bytes read from the header
    char server_message[PACKET_MAX];    // Buffer for message sent from server
    message_record* received_message;   // Pointer to a message record, used to decode received packet payload
    packet* received_packet;

    char message_time[9];     // Timestamp of the message
    std::string chat_message; // Final composed chat message string, printed to the interface
    std::string username;     // Name of the user who sent the message

    // Clear buffer to receive new packets
    bzero((void*)server_message, PACKET_MAX);

    // Wait for messages from the server
    while(!stop_issued && (read_bytes = CommunicationUtils::receivePacket(server_socket, server_message, PACKET_MAX)) > 0)
    {
        // Decode message into packet format
        received_packet = (packet*)server_message;

        // Try to read the rest of the payload from the socket stream
        switch(received_packet->type)
        {
            case PAK_DATA: // Data packet (messages)

                // Decode payload into a message record
                received_message = (message_record*)received_packet->_payload;

                // If message was sent by this user, change display name to "You"
                if (strcmp(received_message->username, this->username.c_str()) == 0)
                {
                    sprintf(received_message->username,"%s","You");
                }
                // If not, add brackets to display name: [username]
                else
                {
                    username = std::string("[") + received_message->username + "]";
                    sprintf(received_message->username,"%s",username.c_str());
                }          

                // Get time into a readable format
                strftime(message_time, sizeof(message_time), "%H:%M:%S", std::localtime((time_t*)&received_message->timestamp));

                if (received_message->type == SERVER_MESSAGE)
                {
                    chat_message = message_time + std::string(" ") + received_message->_message;
                }
                else if (received_message->type == USER_MESSAGE)
                {
                    chat_message = message_time + std::string(" ") + received_message->username + ": " + received_message->_message;
                }

                // Display message
                ClientInterface::printMessage(chat_message);
                break;

            case PAK_SERVER_MESSAGE: // Server broadcast message for clients login/logout 
                
                // Decode payload into a message record
                received_message = (message_record*)received_packet->_payload;

                // Get time into a readable format
                strftime(message_time, sizeof(message_time), "%H:%M:%S", std::localtime((time_t*)&received_message->timestamp));

                // Display message
                chat_message = message_time + std::string(" ") + std::string(received_message->_message);
                ClientInterface::printMessage(chat_message);

                break;

            case PAK_COMMAND: // Command packet (disconnect)

                ClientInterface::printMessage(received_packet->_payload);

                // Stop the client application
                stop_issued = true;
                read_bytes = 0;
                break;

            default: // Unknown packet
                ClientInterface::printMessage("Received unkown packet from server");
                break;
        }

        // Clear buffer to receive new packets
        bzero((void*)server_message, PACKET_MAX);

    }
    // If server closes connection
    if (read_bytes == 0)
    {
        std::cout << "\nConnection closed." << std::endl;
    }

    // Signal input handler to stop
    stop_issued = true;

    // Wait for other threads to finish
    pthread_join(Client::input_handler_thread,NULL);
    pthread_join(Client::keep_alive_thread, NULL);
};

void *Client::handleUserInput(void* arg)
{
    int payload_size;
    
    // Flush stdin so no empty message is sent
    fflush(stdin);

    // Get user messages to be sent until Ctrl D is pressed
    char user_message[MESSAGE_MAX + 1];
    do
    {
        // Get user message
        // TODO Detect Ctrl D press instead of Ctrl C
        wgetstr(ClientInterface::inptscr, user_message);
        try
        {
            // Reset input area below the screen
            ClientInterface::resetInput();

            if (strlen(user_message) > 0) 
 	        {
                // Request write rights
                socket_monitor.requestWrite();

                // Prepare message payload
                char* payload = user_message + '\0';
                payload_size = strlen(payload) + 1;
                CommunicationUtils::sendPacket(server_socket, PAK_DATA, payload, payload_size);

                // Release write rights
                socket_monitor.releaseWrite();
            }

        }
        catch(const std::runtime_error& e)
        {
            std::cerr << e.what() << std::endl;
        }

    }
    while(!stop_issued);

    // Signal server-litening thread to stop
    stop_issued = true;

    // Close listening socket
    shutdown(server_socket, SHUT_RDWR);

    // End with no return value
    pthread_exit(NULL);
};

void *Client::keepAlive(void* arg)
{
    // Useless message that will be sent to server to keep connection going
    char keep_alive = '\0';

    while(!stop_issued)
    {
        // Sleep for SLEEP_TIME seconds between attempting to send messages to the server
        sleep(SLEEP_TIME);
    
        // Request write rights
        socket_monitor.requestWrite();
    
        // Send message
        CommunicationUtils::sendPacket(Client::server_socket, PAK_KEEP_ALIVE, &keep_alive, sizeof(keep_alive));

        // Release write rights
        socket_monitor.releaseWrite();

    }

    // Exit
    pthread_exit(NULL);
}