#ifndef CLIENT_H
#define CLIENT_H

#include <regex>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>
#include <ctime>
#include <unistd.h>
#include <iostream>
#include <atomic>
#include <pthread.h>
#include <unistd.h>

#include "constants.h"
#include "data_types.h"
#include "CommunicationUtils.h"
#include "RW_Monitor.h"

#include "ClientInterface.h"

class Client : protected CommunicationUtils
{
    // Private attributes
private:
    static std::atomic<bool> server_down; // Signlas that the server has disconnected, and app is waiting for another one
    static std::atomic<bool> stop_issued; // Atomic flag for stopping all threads
    static std::string username;          // Client display name
    std::string groupname;                // Group the client wishes to join
    std::string server_ip;                // IP of the remote server
    static int server_port;               // Port the remote server listens at
    static int server_socket;             // Socket for remote server communication
    struct sockaddr_in server_address;    // Server socket address

    static int listen_port; // Port where the client listens for reconnects

    static pthread_t input_handler_thread;     // Thread to listen for new incoming server messages
    static pthread_t keep_alive_thread;        // Thread to keep the client "alive" by sending periodic messages to server
    static pthread_t ui_update_thread;         // Thread to keep UI timer and info up-to-date
    static pthread_t election_listener_thread; // Thread to listen to the result of an election

    static RW_Monitor socket_monitor; // Monitor that controls the sending of data through the socket

    // Public methods
public:
    /**
     * @brief Class constructor
     * @param username User display name, must be between 4-20 characters (inclusive) and be composed of only letters, numbers and dots(.)
     * @param groupname Chat room the user wishes to join, must be between 4-20 characters (inclusive) and be composed of only letters, numbers and dots(.)
     * @param server_ip IP Address for the remote server, formatted in IPv4 (x.x.x.x)
     * @param server_port Port the remote server is listening at
     * @param listen_port Port to listen for an election result
     */
    Client(std::string username, std::string groupname, std::string server_ip, std::string server_port, std::string listen_port);

    /**
     * Class destructor
     * Closes communication socket with the remote server if one has been opened
     */
    ~Client();

    /**
     * Setup connection to remote server
     */
    void setupConnection();

    /**
     * Poll server for any new messages, showing them to the user
     */
    void getMessages();

    class ElectionListener : protected CommunicationUtils
    {
        // Private attributes
    private:
        //static std::atomic<bool> stop_issued; // Atomic thread for stopping all threads
        static int server_socket;          // Socket the server listens at for new incoming connections
        struct sockaddr_in client_address; // Client socket address
        static int listen_port;
        struct sockaddr_in server_address; // Server socket address

        // Public methods
    public:
        /**
         * Class constructor
         * Initializes server socket
         * @param N Port to listen for connections
         */
        ElectionListener(int N);

        /**
         * Class destructor, closes any open sockets
         */
        ~ElectionListener();

        /**
         * Listens for incoming connections from clients
         */
        void listenConnections();

        // Private methods
    private:
        /**
         * Sets up the server socket to begin for listening
         */
        void setupConnection();

        /**
         * Handle any incoming connections, spawned by listenConnections
         */
        static void handleConnection(int socket);
    };

private:
    /**
     * Handles getting user input so that it may be sent to the remtoe server
     * This should run in a separate thread to getMessages
     */
    static void *handleUserInput(void *arg);

    /**
     * Loops sending messages to server in order to keep the connection "alive"
     */
    static void *keepAlive(void *arg);

    /**
    * Thread procedure that handles the socket for incomming election results
    */
    static void *startElectionListener(void *arg);
};

#endif
