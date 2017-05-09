#include <iostream>
#include <string>
#include <stdlib.h>
#include <vector>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <limits>

using namespace std;

int user_token;
int client_socket;
struct hostent *server_hostent;
struct sockaddr_in their_address;
string server_hostname;
int PORT;

char* send_to_server(char*);
void connect_to_server();

int main(int argc, char* argv[]) {
    if (argc == 3) {
        server_hostname = string(argv[1]);
        PORT = atoi(argv[2]);
        connect_to_server();

        string username;
        cout << "Enter username: ";
        getline(cin, username);
        while (!cin) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Unexpected input detected. Try again.\n";
            cout << "Enter username: ";
            getline(cin, username);
        }

        string _username = "0" + username;

        if (string(send_to_server((char *) _username.c_str())) == "-1") {
            cout << "No matching username on server\n";
            exit(1);
        } else {
            cout << "\nUsername found on server!\n\n";
        }

        string password;
        cout << "Enter password: ";
        getline(cin, password);
        while (!cin) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Unexpected input detected. Try again.\n";
            cout << "Enter password: ";
            getline(cin, username);
        }

        string _password = "1" + password;

        if (string(send_to_server((char *) _password.c_str())) == "-1") {
            cout << "No matching username with that password on server\n";
            exit(1);
        } else {
            cout << "\nPassword found on server!\n\n";
        }

        cout << "\nLogin authentication successful.\nYour validated credentials are as follows:\n"
             << "Username: " << username << endl
             << "Password: " << password << endl;
    } else {
        cout << "Error: bad arguments\n";
    }
    return 0;
}

void connect_to_server() {
    if ((server_hostent = gethostbyname(server_hostname.c_str())) == NULL) { // get the host info
        cout << "Unknown host. Could not connect to the Login Authentication server.\n";
        exit(1);
    }

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    their_address.sin_family = AF_INET; // host byte order is Little Endian
    their_address.sin_port = htons(PORT); // convert to network byte order, which is in Big Endian
    their_address.sin_addr = *((struct in_addr*) server_hostent->h_addr);
    memset(&(their_address.sin_zero), '\0', 8); // zero out the rest of the struct

    if (connect(client_socket, (struct sockaddr*) &their_address, sizeof(struct sockaddr)) == -1) {
        cout << "That host exists, but a Login Authentication service is not running.\n";
        exit(1);
    } else {
        cout << "Connected to the Login Authentication server!\n\n";
    }
}

char* send_to_server(char* data) {
    char* receive_buffer = (char*) calloc(128, sizeof(char));

    for (;;) {
        if ((send(client_socket, data, strlen(data), 0)) == -1) {
            perror("send");
            close(client_socket);
            exit(1);
        }

        if ((recv(client_socket, receive_buffer, 127, 0)) == -1) {
            perror("recv");
            send_to_server((char*) "quit");
            exit(1);
        } else {
            break;
        }
    }

    string s(receive_buffer);
    if (s == "timeout") {
        cout << "Connection to server timed out\n";
        exit(0);
    }

    return receive_buffer;
}