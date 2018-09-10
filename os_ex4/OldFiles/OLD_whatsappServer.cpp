
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <list>
#include <map>
#include <iostream>
#include <algorithm>
#include <errno.h>
#include "../whatsappio.h"

//// ============================   Constants =====================================================
static const int MAX_QUEUE = 10;
static const int MAX_HOST_NAME_LEN = 30;
static const int MAX_GROUP_NAME_LEN = 30;
static const int BUFF_SIZE = 256;
static const int MAX_MESSAGE_LENGTH = 256;
static const int FUCK = 1; // todo exit val?


//// ===========================  Structs & Typedefs ==============================================


// client
struct Client {
    std::string name;
    int sockfd;
};

// clients Group
struct Group {
    std::string name;
    std::map<std::string, Client> groupMembers;
};

// command
struct Command { //todo make init?
    command_type type;
    std::string name;
    std::string message;
    std::vector<std::string> clients;
    std::string sender;
};

typedef std::pair<std::string, Client> ClientPair;
typedef std::pair<std::string, Group> GroupPair;


//todo  ===========================   TODOS ===============================================


//// ============================  Class Declarations =============================================

/**
 * Class representing running instance of Server.
 */
class Server {

    std::string serverName;
    unsigned short serverPort;
    int welcomeSocket;

    std::string commandStr;
    Command c;

    std::map<std::string, Client> clients;
    std::map<std::string, Group> groups;

public:

    //// C-tor
    explicit void Server(unsigned short portNumber);

    //// server actions
    void selectPhase();
    int connectNewClient(int welcomeSocket);
    void serverStdInput();
    void handleClientRequest(int sockfd);

private:

    //// send/recv
    void prepSize(uint64_t size, Client client);
    void strToClient(const std::string& str, const std::string& clientName);

    //// DB queries
    bool isClient(std::string& name);
    bool isGroup(std::string& name);
    int getMaxfd();
    Client clientByfd(int sockfd);

    //// request handling
    void createGroup(Command c);
    void send(Command c);
    void who(Command c);
    void clientExit(Command c);
    void registerClient(Command c, int sockfd);

    //// name legality
    bool isLegalName(std::string& name);
    bool isTakenName(std::string& name);
    bool isAlNumString(std::string& str);

    void killServer();

};

//// ===============================  Forward Declarations ============================================

//// input checking
int parsePortNum(int argc, char** argv);

//// errors
void errCheck(int& retVal, const std::string& funcName, int successVal = 0);

//// ===============================  Class Server ============================================

//// server actions

void Server::Server(unsigned short portNumber) {

    char srvName[MAX_HOST_NAME_LEN+1];
    struct sockaddr_in sa;  // sin_port and sin_addr must be in Network Byte order.
    struct hostent* hostEnt;

    int retVal = gethostname(srvName, MAX_HOST_NAME_LEN);
    if (retVal<0) {
        print_error("gethostname", errno);
        exit(FUCK); //todo should we exit?
    }

    bzero(&sa, sizeof(struct sockaddr_in));
    hostEnt = gethostbyname(srvName);
    if (hostEnt==nullptr) {
        print_error("gethostbyname", errno);
        exit(FUCK); //todo should we exit?
    }

    memset(&sa, 0, sizeof(struct sockaddr_in));
    sa.sin_family = hostEnt->h_addrtype;
    memcpy(&sa.sin_addr, hostEnt->h_addr, hostEnt->h_length);
    sa.sin_port = htons(portNumber);

    welcomeSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (welcomeSocket<0) { print_error("socket", errno); }

    retVal = bind(welcomeSocket, (struct sockaddr*) &sa, sizeof(struct sockaddr_in));
    if (retVal<0) {
        print_error("bind", errno);
    }

    retVal = listen(welcomeSocket, MAX_QUEUE);
    if (retVal<0) { print_error("listen", errno); }

    strcpy(serverName, srvName);
    serverPort = portNumber;
}

void Server::selectPhase() {
    fd_set clientsfds;
    fd_set readfds; //Represent a set of file descriptors.
    FD_ZERO(&clientsfds);   //Initializes the file descriptor set fdset to have zero bits for all file descriptors

    FD_SET(welcomeSocket,
            &clientsfds);  //Sets the bit for the file descriptor fd in the file descriptor set fdset.


    FD_SET(STDIN_FILENO, &clientsfds);
    int retVal;
    int maxfds;
    while (true) {
        readfds = clientsfds;
        maxfds = this->getMaxfd();
        retVal = select(maxfds+1, &readfds, nullptr, nullptr, nullptr);
        if (retVal==-1) {
            print_error("select", errno);
            continue;
            //todo VALIDATE THAT SERVER NEVER TERMINATES??? terminate server and return -1;
        }
        else if (retVal==0) {
            continue;
        }
        //Returns a non-zero value if the bit for the file descriptor fd is set in the file descriptor set pointed to by fdset, and 0 otherwise
        if (FD_ISSET(welcomeSocket, &readfds)) {
            //will also add the client to the clientsfds
//          int connectionSocket = connectNewClient(welcomeSocket);
            int connectionSocket = accept(welcomeSocket, nullptr, nullptr);
            if (connectionSocket<0) {
                print_error("accept", errno);
            }
            else {
                FD_SET(connectionSocket, &clientsfds);
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            //msg from stdin
            serverStdInput();
        }

        else {
            //will check each client if it’s in readfds /todo what does this mean??
            //and then receive a message from him
            handleClientRequest(); // todo give sockfd
        }
        break;//todo remove this!!!!!!! J: why?
    }
}

int Server::connectNewClient(int welcomeSocket) {
    int connectionSocket;
    connectionSocket = accept(welcomeSocket, nullptr, nullptr);
    if (connectionSocket<0) {
        print_error("accept", errno);
        //todo exit?
    }
    else {
        return connectionSocket;
    }
}

void Server::serverStdInput() {
    std::string serverInput;
    getline(std::cin, serverInput);
    if (serverInput=="EXIT") {
        print_exit();
        killServer();

    }
    else {
        print_invalid_input();
    }
}

void Server::killServer() {

    print_exit();

    //close sockets
    for (auto& pair: clients) {
        if (close(pair.second.sockfd)!=0) {
            print_error("close", errno);
        }
    }
    exit(0);

}

void Server::handleClientRequest(int sockfd) {
    // get socket (may be unregistered)
    Client client = clientByfd(sockfd);

    int64_t msgLen;

    // read from socket
    ssize_t n = read(sockfd, &msgLen, sizeof(msgLen));
    if (n<0) {
        print_error("read", errno);
    }

    // read string from command
    n = read(sockfd, &commandStr, msgLen);
    if (n<0) {
        print_error("read", errno);
    }

    parse_command(commandStr, c.type, c.name, c.message, c.clients);
    c.sender = client.name;

    switch (c.type) {
    case CREATE_GROUP:createGroup(c);
        break;

    case SEND:send(c);
        break;

    case WHO:who(c);
        break;

    case CONNECT:registerClient(c, sockfd);
        break;

    case EXIT:clientExit(c);
        break;

    case INVALID:print_invalid_input();
        break;

    }

};

//// send/recv

void Server::prepSize(uint64_t size, Client client) {
    uint64_t datalen = size;
    ssize_t written = write(client.sockfd, &datalen, sizeof(uint64_t));
    if (written!=c.sender.size()) {
        print_error("write", errno);
    }
}

void Server::strToClient(const std::string& str, const std::string& clientName) {
    Client client = clients[clientName];

    // send string size
    prepSize(str.size(), client);

    // send string
    ssize_t written = write(client.sockfd, &str, str.size());
    if (written!=str.size()) { //todo J is correct error checking?
        print_error("write", errno);
    }
}


//// DB modify

void Server::registerClient(Command c, int sockfd) {

    if (!isLegalName(c.name)) {

        // notify if name is duplicate
        std::string taken = isTakenName(c.name) ? "D" : "F";

        // notify failure to client
        strToClient("connect "+taken, c.name);

        // todo make sure no server print is necessary
    }

    Client newClient = {c.name, sockfd};

    clients[c.name] = newClient;

    // print success on server
    printf("%s: Connected Successfully.\n", c.name);

    // notify sucess to client
    strToClient("connect T", c.name);

}

//// DB queries

bool Server::isClient(std::string& name) {
    return ((bool) clients.count(name)); // (count is zero (false) if not there.
}

bool Server::isGroup(std::string& name) {
    return ((bool) groups.count(name)); // (count is zero (false) if not there.
}

int Server::getMaxfd() {
    int max = this->welcomeSocket;
    for (auto client :clients) {
        if (client.second.sockfd>max) {
            max = client.second.sockfd;
        }
    }
    return max;
}

// todo (improvement) make client map by sockfd
Client Server::clientByfd(int sockfd) {
    // give sockfd back so that register works in any case
    Client unregistered{"not_registered", sockfd};

    for (auto& pair: clients) {
        if (pair.second.sockfd==sockfd) {
            return pair.second;
        }
    }

    return unregistered;
}


//// request handling

void Server::createGroup(Command c) {

    // ensure group name legal & unique (not taken)
    if (!isLegalName(c.name)) {
        //print failure on server
        print_create_group(true, false, c.sender, c.name);

        //report failure to client
        strToClient("create_group F", c.name);
    }

    // make set of clients (allowing duplicates) and ensuring all clients exists
    Group newGroup;

    newGroup.name = c.name;


    // add each client
    for (std::string strName : c.clients) {
        // if a client not in group
        if (newGroup.groupMembers.count(strName)) {


            // ensure client exists in server
            if (!isClient(strName)) {

                //print failure on server
                print_create_group(true, false, c.sender, c.name);
                //report failure to client
                strToClient("create_group F", c.sender);
            }

            //add it to group
            newGroup.groupMembers[strName] = clients[strName];
        }
    }

    // add sender to set even if unspecified
    newGroup.groupMembers[c.sender] = clients[c.sender];

    // ensure group has at least 2 members (including creating client)
    if (newGroup.groupMembers.size()<2) {

        //print failure on server
        print_create_group(true, false, c.sender, c.name);
        //report failure to client
        strToClient("create_group F", c.sender);
    }

    // add this group to DB
    groups[newGroup.name] = newGroup;

    //print success on server
    print_create_group(true, true, c.sender, c.name);
    //report success to client
    strToClient("create_group T", c.sender);

}

void Server::send(Command c) {

    std::string message = c.sender+": "+c.message;

    // if name in clients
    if (isClient(c.name)) {

        // ensure recipient is not sender
        if (c.name==c.sender) {
            // notify sender of failure
            strToClient("send F", c.sender);
        }

        // message the recipient client
        strToClient("message sender "+message, c.name);

        // notify sender of success
        strToClient("send T", c.sender);
        // print success server
        print_send(true, true, c.sender, c.name, message);

    }
        // if name in groups
    else if (isGroup(c.name)) {

        // ensure caller is in this group
        if (!groups.count(c.name)) {
            // notify sender of failure
            strToClient("send F", c.sender);
        }

        // send to all in group except caller
        for (std::pair& pair : groups.at(c.name).groupMembers) {
            // if not sender
            if (pair.first!=c.sender) {
                // message each recipient client
                strToClient("message sender "+message, pair.first);
            }
        }

        // notify sender of success
        strToClient("send T", c.sender);
        // print success server
        print_send(true, true, c.sender, c.name, message);

    }
    else {
        // else error

        // notify sender of failure
        strToClient("send F", c.sender);
        // print failure server
        print_send(true, false, c.sender, c.name, message);
    }

}

void Server::who(Command c) {
    //// order and return names
    std::vector<std::string> namesVec;

    // get all names
    for (std::pair& pair : clients) {
        namesVec.push_back(pair.first);
    }

    std::sort(namesVec.front(), namesVec.back());

    std::string list;

    for (std::string name: namesVec) {
        list += name+",";
    }

    //send list to printing
    strToClient("who "+list, c.sender);

}

void Server::clientExit(Command c) {

    // remove sender from all groups
    for (auto& pair : groups) {

        // remove sender from members of group (if he is there)
        Group group = pair.second;
        group.groupMembers.erase(c.sender);
    }

    //print success to server
    print_exit(true, c.sender);
    // send success to client
    strToClient("exit T ", c.sender);

    //close sockets
    if (close(clients[c.sender].sockfd)!=0) {
        print_error("close", errno);
    }

    // remove sender from server (after reported success)
    clients.erase(c.sender);

}

//// name legality

bool Server::isLegalName(std::string& name) {
    // ensure alphanumeric only and name not taken.
    return (isAlNumString(name) && !isClient(name) && !isGroup(name));
}

bool Server::isTakenName(std::string& name) {
    // ensure alphanumeric only and name not taken.
    return (isClient(name) || !isGroup(name));
}

bool Server::isAlNumString(std::string& str) {
    for (char c: str) {
        if (!isalnum(c)) return false;
    }
    return true;
}


//// ===============================  Helper Functions ============================================

//// input checking
int parsePortNum(int argc, char** argv) {

    //// check args
    if (argc!=2) {
        print_server_usage();  //todo server shouldnt crash upon receiving illegal requests?
        exit(1);
    }

    int portNumber = atoi(argv[1]);

    if (portNumber<0 || portNumber>65535) {
        print_server_usage();
        exit(1);
    }

    return portNumber;
}


//// =============================== Main Function ================================================

int main(int argc, char* argv[]) {

    //// --- Init  ---
    int portNumber = parsePortNum(argc, argv);

    //// init Server
    Server server((unsigned short) portNumber);  // todo J is conversion ok? maybe cast inside parse

    //// --- Setup  ---
    //// create socket
    //// bind to an ip adress and port

    //// --- Wait  ---
    //// listen
    //// accept
    server.selectPhase();

    //// --- Get Request  ---
    //// read
    //// parse

    //// --- Process  ---
    //// write
    //// close

    //// --- Repeat  ---
}