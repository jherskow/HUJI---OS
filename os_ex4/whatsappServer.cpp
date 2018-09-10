
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <list>
#include <map>
#include <iostream>
#include <algorithm>
#include "whatsappio.h"

//// ============================   Constants =====================================================
static const int MAX_QUEUE = 10;

//// ===========================  Structs & Typedefs ==============================================


// Command struct
struct Command {
    command_type type;
    std::string name;
    std::string message;
    std::vector<std::string> clients;
    std::string sender;
    int senderSockfd;
};

// case insensitive sort
struct case_insensitive_less : public std::binary_function< char,char,bool >
{
    bool operator () (char x, char y) const
    {
        return toupper( static_cast< unsigned char >(x)) <
                toupper( static_cast< unsigned char >(y));
    }
};



typedef std::map<std::string, int> clientsDB;
typedef std::vector<std::string> GroupMembers1;
typedef std::map<std::string, GroupMembers1> GroupsDB1;


//// ============================  Class Declarations =============================================

/**
 * Class representing running instance of Server.
 */
class Server {

    std::string serverName;
    unsigned short serverPort;
    int welcomeSocket;


    clientsDB clients1;
    GroupsDB1 groups1;
    char readBuf[WA_MAX_INPUT+1];
    char writeBuf[WA_MAX_INPUT+1];

    fd_set clientsfds;
    fd_set readfds;

public:

    //// C-tor
    Server(unsigned short portNumber);

    //// server actions
    void selectPhase();
    bool shouldTerminateServer();
    void handleClientRequest(int sockfd);
    void writeToClient(int sockfd, const std::string& command);

private:

    //// send/recv

    //// DB queries
    bool isClient(std::string& name);
    bool isGroup(std::string& name);
    int getMaxfd();
    std::string getClientNameById(int sockfd);


    //// request handling
    void createGroup(Command cmd);
    void send(Command cmd);
    void who(Command cmd);
    void clientExit(Command cmd);
    void registerClient(Command cmd);
    void tempRegisterClient(int sockfd);

    //// name legality
    bool isTakenName(std::string& name);

    void killAllSocketsAndClients();

};



//// ===============================  Forward Declarations ======================================

//// input checking
int parsePortNum(int argc, char** argv);

/// string case insensitive compare
bool NoCaseLess(const std::string &a, const std::string &b);


//// ===============================  Class Server ============================================

//// server actions

Server::Server(unsigned short portNumber) {

    char srvName[WA_MAX_NAME + 1];
    struct sockaddr_in sa;  // sin_port and sin_addr must be in Network Byte order.
    struct hostent* hostEnt;

    int retVal = gethostname(srvName, WA_MAX_NAME);
    if (retVal < 0) {
        print_error("gethostname", errno);
    }

    bzero(&sa, sizeof(struct sockaddr_in));
    hostEnt = gethostbyname(srvName);
    if (hostEnt == nullptr) {
        print_error("gethostbyname", errno);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = (sa_family_t) hostEnt->h_addrtype;
    memcpy(&sa.sin_addr, hostEnt->h_addr, (size_t) hostEnt->h_length);
    sa.sin_port = htons(portNumber);

    welcomeSocket = socket(AF_INET, SOCK_STREAM, 0);
    int enable = 1;
    if (setsockopt(welcomeSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
        print_error("setsockopt", errno);
    }

    if (welcomeSocket < 0) { print_error("socket", errno); }

    sa.sin_addr.s_addr = INADDR_ANY;

    retVal = bind(welcomeSocket, (struct sockaddr*) &sa, sizeof(struct sockaddr_in));
    if (retVal < 0) {
        print_error("bind", errno);
    }

    retVal = listen(welcomeSocket, MAX_QUEUE);

    if (retVal < 0) { print_error("listen", errno); }

    std::string srv_str(srvName);
    this->serverName = srv_str;
    this->serverPort = portNumber;
    bzero(this->readBuf, WA_MAX_INPUT +1);
    bzero(this->writeBuf, WA_MAX_INPUT +1);
}

/**
 * Server's select phase. Looping over select.
 */
void Server::selectPhase() {
    FD_ZERO(&clientsfds);
    FD_SET(welcomeSocket, &clientsfds);
    FD_SET(STDIN_FILENO, &clientsfds);

    int retVal;
    int maxfds;
    bool keepLoop = true;

    // looping to wait for input socket to be ready to read.
    while (keepLoop) {
        readfds = clientsfds;
        maxfds = this->getMaxfd();
        retVal = select((maxfds + 1), &readfds, nullptr, nullptr, nullptr);
        if (retVal == -1) {
            print_error("select", errno);
            continue;
        }
        else if (retVal == 0) {
            continue;
        }

        if (FD_ISSET(welcomeSocket, &readfds)) {

            //will also add the client to the clientsfds
            int connectionSocket = accept(welcomeSocket, nullptr, nullptr);
            if (connectionSocket < 0) {
                print_error("accept", errno);
            } else {
                FD_SET(connectionSocket, &clientsfds);
                tempRegisterClient(connectionSocket);
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) { // reading message from std input.
            keepLoop = !shouldTerminateServer();
            continue;

        } else { //will check each client if it’s in readfds and then receive a message from him.

            for (auto client:this->clients1) {
                if (FD_ISSET(client.second, &readfds)) {
                    handleClientRequest(client.second);
                }
            }
        }
    }
}

/**
 * This method will check if user has requested to EXIT the server.
 * If true server is notify clients about termination and closing sockets
 * @return true iff user typed to server console 'EXIT'
 */
bool Server::shouldTerminateServer() {
    std::string serverInput;
    getline(std::cin, serverInput);
    if (serverInput == "EXIT") {
        killAllSocketsAndClients();
        print_exit();
        return true;
    }
    else {
        return false;
    }
}

/**
 * Write command string into given sockfd
 * @param sockfd the sock fd to write the command to
 * @param command client sockfd
 */
void Server::writeToClient(int sockfd, const std::string& command) {

    strcpy(this->writeBuf, command.c_str());
    int bcount = 0; /* counts bytes read */
    int br = 0; /* bytes read this pass */
    while (bcount < WA_MAX_INPUT) { /* loop until full buffer */
        br = (int) write(sockfd, this->writeBuf, (size_t) WA_MAX_INPUT - bcount);
        if (br > 0) {
            bcount += br;
        }
        if (br < 1) {
            print_error("write", errno);
        }
    }
}

/**
 * Notifying all server's clients to terminate, and closing server's sockets.
 */
void Server::killAllSocketsAndClients() {

    std::string terminateCmd = "terminated";
    for (auto client:this->clients1) {

        // tell clients that server is terminated.
        this->writeToClient(client.second, terminateCmd);
        if (close(client.second) != 0) {  //close sockets
            print_error("close", errno);
        }
    }
    close(welcomeSocket);
}

/**
 * will read from client with given sockfd and will handle it's request
 * @param sockfd client sockfd
 */
void Server::handleClientRequest(int sockfd) {

    int bcount = 0; /* counts bytes read */
    int br = 0; /* bytes read this pass */

    while (bcount < WA_MAX_INPUT) { /* loop until full buffer */
        br = (int) read(sockfd, this->readBuf, (size_t) WA_MAX_INPUT - bcount); //reading is atounce
        if (br > 0) {
            bcount += br;
        }
        if (br < 1) {
            print_error("read", errno);
            return;
        }
    }

    // setting client message as string
    std::string incomingMsg = this->readBuf;
    Command cmd;

    // parsing client request
    cmd.type = INVALID;
    cmd.name = "";
    cmd.message = "";
    cmd.clients.clear();

    parse_command(incomingMsg, cmd.type, cmd.name, cmd.message, cmd.clients);
    cmd.sender = getClientNameById(sockfd);
    cmd.senderSockfd = sockfd;

    switch (cmd.type) {
        case CREATE_GROUP:
            createGroup(cmd);
            break;
        case SEND:
            send(cmd);
            break;
        case WHO:
            who(cmd);
            break;
        case CONNECT:
            registerClient(cmd);
            break;
        case EXIT:
            clientExit(cmd);
            break;
        case INVALID:
            print_invalid_input();
            break;
        case MESSAGE:
            print_invalid_input();
            break;
        case TERMINATED:
            print_invalid_input();
            break;
    }
};


//// DB modify
/**
 * This will register new client connection temporarly until getting his name.
 * @param sockfd to register.
 */
void Server::tempRegisterClient(int sockfd) {
    this->clients1.emplace(std::pair<std::string, int>("@" + std::to_string(sockfd), sockfd));
}

/**
 * This will register new client with his name instead of his temp name.
 * @param cmd
 */
void Server::registerClient(Command cmd) {
    this->clients1.erase("@" + std::to_string(cmd.senderSockfd));
    if (isTakenName(cmd.name)) {
        // notify failure (duplicated) to client
        writeToClient(cmd.senderSockfd, "connect D");
        return;
    }
    this->clients1.emplace(std::pair<std::string, int>(cmd.name, cmd.senderSockfd));

    // print success on server
    print_connection_server(cmd.name);
    // notify success to client
    writeToClient(cmd.senderSockfd, "connect T");
}

//// DB queries

/**
 * check if given name is client
 * @param name
 * @return true iff a client with given name is already exist.
 */
bool Server::isClient(std::string& name) {
    return ((bool) this->clients1.count(name)); // (count is zero (false) if not there.
}

/**
 * check if given name is group
 * @param name
 * @return true iff a group with given name is already exist.
 */
bool Server::isGroup(std::string& name) {
    return ((bool) this->groups1.count(name)); // (count is zero (false) if not there.
}

/**
 * check if a name is already registered as client/group
 * @param name name to check
 * @return true iff already registered
 */
bool Server::isTakenName(std::string& name) {
    // ensure  name not taken.
    return (isClient(name) || isGroup(name));
}

/**
 *
 * @return the max fd id exist on server
 */
int Server::getMaxfd() {
    int max = this->welcomeSocket;
    for (auto client :this->clients1) {
        if (client.second > max) { max = client.second; }
    }
    return max;
}

/**
 *
 * @param sockfd sockfd to check
 * @return client name of the given sock fd.
 */
std::string Server::getClientNameById(int sockfd) {

    for (auto client:this->clients1){
        if (client.second == sockfd) {
            return client.first;
        }
    }
    return "none";
}



//// request handling

/**
 * Creating group in server by the given command.
 * @param cmd the create_group command details.
 */
void Server::createGroup(Command cmd) {

    // ensure group name is unique (not taken)
    if (isTakenName(cmd.name)) {
        //print failure on server
        print_create_group(true, false, cmd.sender, cmd.name);

        //report failure to client
        writeToClient(cmd.senderSockfd, "create_group F " + cmd.name);
        return;
    }

    // make set of clients (allowing duplicates) and ensuring all clients exists
    GroupMembers1 members = GroupMembers1();

    // add each client
    for (std::string strName : cmd.clients) {
        // if exists:
        if (isClient(strName)) {
            members.push_back(strName);
            //otherwise client does not exist
        }else{
            //print failure on server
            print_create_group(true, false, cmd.sender, cmd.name);

            //report failure to client
            writeToClient(cmd.senderSockfd, "create_group F " + cmd.name);
            return;
        }
    }

    // add sender to set even if unspecified
    members.push_back(cmd.sender);

    // ensure group has at least 2 members (including creating client)
    if (members.size() < 2) {

        //print failure on server
        print_create_group(true, false, cmd.sender, cmd.name);

        //report failure to client
        writeToClient(cmd.senderSockfd, "create_group F " + cmd.name);
    }

    // add this group to DB
    groups1.emplace(std::pair<std::string, GroupMembers1>(cmd.name, members));

    //print success on server
    print_create_group(true, true, cmd.sender, cmd.name);
    //report success to client
    writeToClient(cmd.senderSockfd, "create_group T " + cmd.name);

}

/**
 * Sending message in server by the given command.
 * @param cmd to send command details.
 */
void Server::send(Command cmd) {

    std::string message = "message " + cmd.sender + " " + cmd.message;

    // if name in clients
    if (isClient(cmd.name)) {

        // message the recipient client
        writeToClient(this->clients1[cmd.name], message);

        // notify sender of success
        writeToClient(cmd.senderSockfd, "send T");
        // print success server
        print_send(true, true, cmd.sender, cmd.name, cmd.message);

    } else if (isGroup(cmd.name)) {   // if name in groups

        // ensure caller is in this group
        bool senderInGroup = false;
        for (const std::string & memberName : groups1.at(cmd.name)) {
            if (!(memberName.compare(cmd.sender))) {
                senderInGroup = true;
            }
        }

        if (senderInGroup) {
            for (const std::string & memberName : groups1.at(cmd.name)) {
                if (!(memberName.compare(cmd.sender))) {
                    // notify sender of success
                    writeToClient(cmd.senderSockfd, "send T");
                } else {
                    // message each recipient client
                    writeToClient((this->clients1[memberName]), message);
                }
            }
            
            // print success server
            print_send(true, true, cmd.sender, cmd.name, cmd.message);
            return;
        } else {
            // notify sender of failure
            writeToClient(cmd.senderSockfd, "send F");
            // print fail server
            print_send(true, false, cmd.sender, cmd.name, cmd.message);
        }
    } else { // else error
        // notify sender of failure
        writeToClient(cmd.senderSockfd, "send F");
        // print fail server
        print_send(true, false, cmd.sender, cmd.name, cmd.message);
    }
}

/**
 * Handling who in server by the given command.
 * @param cmd who command details.
 */
void Server::who(Command cmd) {
    //// order and return names
    std::vector<std::string> namesVec;

    // get all names
    for (auto client: this->clients1) {
        namesVec.emplace_back(client.first);
    }

    std::sort(namesVec.begin(), namesVec.end(), NoCaseLess );

    std::string list = "who T ";

    for (const std::string & name: namesVec) {
        list += name+",";
    }

    //send list to printing
    writeToClient(cmd.senderSockfd, list);
    // print who server
    print_who_server(cmd.sender);
}

/**
 * Handling client exit request by the given command.
 * @param cmd the exit command details.
 */
void Server::clientExit(Command cmd) {

    // remove sender from all groups
    std::vector<std::string> toErase;
    for (auto& group : groups1) {
        group.second.erase(std::remove(group.second.begin(), group.second.end(), cmd.sender),
                group.second.end());

        // remember groups to kill
        if(group.second.empty()){
            toErase.push_back(group.first);
        }
    }

    // kill emptied groups
    for(auto& name: toErase){
        groups1.erase(name);
    }

    // send success to client
    writeToClient(cmd.senderSockfd, "exit T ");

    //close sockets
    if (close(cmd.senderSockfd) != 0) {
        print_error("close", errno);
    }

    // remove sender from server (after reported success) and from fd_set
    this->clients1.erase(cmd.sender);
    FD_CLR(cmd.senderSockfd, &(this->clientsfds));

    //print success to server
    print_exit(true, cmd.sender);
}


//// ===============================  Helper Functions ============================================

//// input checking
/**
 * checking args count and parsing port number
 * @param argc main args count
 * @param argv main argv[]
 * @return port number as int.
 */
int parsePortNum(int argc, char** argv) {

    //// check args
    if (argc!=2) {
        print_server_usage();
        exit(1);
    }
    int portNumber = atoi(argv[1]);
    return portNumber;
}


//// =============================== Main Function ================================================

int main(int argc, char* argv[]) {

    //// --- Init  ---
    int portNumber = parsePortNum(argc, argv);
    //// Setup server
    Server server((unsigned short) portNumber);

    // Server's select phase.
    server.selectPhase();
    exit(0);
}

//// =============================== helper Function ==============================================

bool NoCaseLess(const std::string &a, const std::string &b)
{
    return std::lexicographical_compare( a.begin(),a.end(),
            b.begin(),b.end(), case_insensitive_less() );
}