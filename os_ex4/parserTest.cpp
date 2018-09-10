#include <iostream>
#include "whatsappio.h"

// command
struct Command {
    command_type type;
    std::string name;
    std::string message;
    std::vector<std::string> clients;
    std::string sender;
    int senderSockfd;
};

std::string type(command_type in);

int main() {

    std::string str;

    for(int i =0; i<1000; ++i){



        std::cout << "enter string:\n";

        std::getline(std::cin, str);

        Command cmd;

        cmd.type=INVALID;
        cmd.name="";
        cmd.message="";
        cmd.clients.clear();

        parse_command(str, cmd.type, cmd.name, cmd.message, cmd.clients);

        std::cout << "type is: " << type(cmd.type) <<"\n";
        std::cout << "name is: " << cmd.name <<"\n";
        std::cout << "message is: " << cmd.message <<"\n";
        std::cout << "clients are: " <<"\n";

        if(!cmd.clients.empty()){
            for(auto& name: cmd.clients){
                std::cout << name << ", ";
            }
        }
        std::cout << "\n\n\n\n\n";

    }

}

std::string type(command_type in ){
    switch (in) {
    case CREATE_GROUP:
        return"CREATE_GROUP";
    case SEND:
        return"SEND";
        break;
    case WHO:
        return"WHO";
        break;
    case CONNECT:
        return"CONNECT";
        break;
    case EXIT:
        return"EXIT";
        break;
    case INVALID:
        return"INVALID";
        break;
    case MESSAGE:
        return"MESSAGE";
        break;
    case TERMINATED:
        return"TERMINATED";
        break;
    }
}