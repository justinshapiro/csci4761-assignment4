#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <fstream>
#include <vector>
#include <chrono>

using namespace std;
using namespace std::chrono;

typedef high_resolution_clock::time_point Time;

int SERVER_PORT;
int BACKLOG = 10;
int MAX_CLIENTS = 30;
char* INPUT_FILE = (char*) "account";
bool running = true;
vector<pair<string, string>> ACCOUNTS;
vector<Time> socket_times(30);
auto sock_null_val = socket_times[0];

void sigchld_handler(int);
char* get_request(char*, int);
char* handle_request(int, char*);
void load_accounts();

int main(int argc, char* argv[]) {
    if (argc == 2) {
        SERVER_PORT = atoi(argv[1]);
        load_accounts();
        int main_socket, new_socket, client_socket[MAX_CLIENTS];
        int read_val, sd, max_sd;
        fd_set socket_set;
        struct sockaddr_in my_address; // my address information
        struct sockaddr_in thier_address; // connector's address information
        struct timeval timeout; // timeout information
        timeout.tv_sec = 30; // 30 seconds as required
        timeout.tv_usec = 0;
        socklen_t sin_size;
        struct sigaction sa;
        int yes = 1;
        char *receive_buffer;

        // Initialize our array of client sockets to 0
        for (int i = 0; i < MAX_CLIENTS; i++) {
            client_socket[i] = 0;
        }

        // Create the listener socket "main_socket"
        if ((main_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            exit(1);
        }

        // Allow the listener socket to accept multiple connections
        if (setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        my_address.sin_family = AF_INET; // host byte order, which is Little Endian
        my_address.sin_port = htons(SERVER_PORT); // network byte order, which is is Big Endian
        my_address.sin_addr.s_addr = INADDR_ANY; // automatically fill with the server IP address
        memset(&(my_address.sin_zero), '\0', 8); // zero out the rest of the struct

        // Bind the socket to a specific port
        if (bind(main_socket, (struct sockaddr *) &my_address, sizeof(struct sockaddr)) == -1) {
            perror("bind");
            exit(1);
        }

        // Define a maximum of BACKLOG pending connections to the listener socket
        if (listen(main_socket, BACKLOG) == -1) {
            perror("listen");
            exit(1);
        }

        sa.sa_handler = sigchld_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        if (sigaction(SIGCHLD, &sa, NULL) == -1) {
            perror("sigaction");
            exit(1);
        }

        while (1) {
            FD_ZERO(&socket_set); // clear the socket set
            FD_SET(main_socket, &socket_set); // add the listener socket to the socket set
            max_sd = main_socket; // preserve the socket

            // add all client sockets to the set
            for (int i = 0; i < MAX_CLIENTS; i++) {
                sd = client_socket[i];
                if (sd > 0) {
                    FD_SET(client_socket[i], &socket_set);
                }
                if (sd > max_sd) {
                    max_sd = sd;
                }
            }

            // scan ("select") all the sockets for activity
            if ((select(max_sd + 1, &socket_set, NULL, NULL, &timeout) < 0) && (errno != EINTR)) {
                cout << "Concurrency error\n";
            }

            // detect new connection requests
            if (FD_ISSET(main_socket, &socket_set)) {
                sin_size = sizeof(struct sockaddr_in);
                new_socket = accept(main_socket, (struct sockaddr *) &thier_address, &sin_size);
                if (new_socket == -1) {
                    perror("accept");
                    continue;
                }

                printf("Client at IP address %s has connected.\n", inet_ntoa(thier_address.sin_addr));

                // add the new socket, which is the identified connection, to our socket list
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (client_socket[i] == 0) {
                        client_socket[i] = new_socket;
                        socket_times[i] = high_resolution_clock::now();
                        break;
                    }
                }
            }

            // handle requests from existing connections
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (socket_times[i] != sock_null_val) {
                    sd = client_socket[i];
                    Time now = high_resolution_clock::now();
                    int socket_time = duration_cast<seconds>(now - socket_times[i]).count();
                    if (socket_time >= 30) {
                        send(sd, (char *) "timeout", strlen((char *) "timeout"), 0);
                        close(sd);
                        cout << "Client on socket " << i << " has timed out.\n";
                        client_socket[i] = 0;
                        socket_times[i] = sock_null_val;
                    } else if (FD_ISSET(sd, &socket_set)) {
                        if (string(get_request(receive_buffer, sd)) == "quit") {
                            close(sd);
                            cout << "Client on socket " << i << " has disconnected.\n";
                            client_socket[i] = 0;
                            socket_times[i] = sock_null_val;
                        } else {
                            socket_times[i] = high_resolution_clock::now();
                        }

                    }
                }
            }
            if (!running) {
                break;
            }
        }
    } else {
        cout << "Error: bad arguments\n";
    }

    return 0;
}

char* get_request(char* receive_buffer, int child_socket) {
    receive_buffer = (char *) calloc(128, sizeof(char));
    int num_bytes = recv(child_socket, receive_buffer, 128, 0);
    if (num_bytes <= 0) {
        return (char*) "quit";
    }
    printf("Received from client on socket %d: %s\n", child_socket, receive_buffer);

    int request_id;
    char *data_segment = (char *) calloc(128, sizeof(char));
    int i;
    for (i = 0; receive_buffer[i] != '\0'; i++) {
        if (i == 0) {
            request_id = receive_buffer[i] - '0';
        } else {
            data_segment[i - 1] = receive_buffer[i];
        }
    }
    data_segment[i + 1] = '\0';

    char *response_buffer = handle_request(request_id, data_segment);

    if (send(child_socket, response_buffer, strlen(response_buffer), 0) == -1) {
        perror("send");
        close(child_socket);
        exit(1);
    }

    printf("Sent to client on socket %d: %s\n", child_socket, response_buffer);

    return response_buffer;
}

char* handle_request(int request_id, char* data_segment) {
    char* response_buffer = (char*) calloc(128, sizeof(char));
    string data = string(data_segment);

    bool found = false;
    for (int i = 0; i < ACCOUNTS.size(); i++) {
        if (request_id == 0) {
            if (ACCOUNTS[i].first == data) {
                found = true;
                break;
            }
        } else if (request_id == 1) {
            if (ACCOUNTS[i].second == data) {
                found = true;
                break;
            }
        }
    }

    if (found) {
        response_buffer = (char*) "0";
    } else {
        response_buffer = (char*) "-1";
    }

    return response_buffer;
}

void load_accounts() {
    string curr_line = "";
    ifstream infile(INPUT_FILE);

    if (infile.good()) {
        while (getline(infile, curr_line)) {
            string s = "";
            pair<string, string> account;
            for (int i = 0; i < curr_line.length(); i++) {
                if (curr_line[i] != '\t') {
                    s += curr_line[i];
                } else {
                    account.first = s;
                    s = "";
                }
            }
            account.second = s;
            ACCOUNTS.push_back(account);
        }
    } else {
        cout << "File named \"account\" not found in server directory\n";
    }

    infile.close();
}

void sigchld_handler(int s) {
    while(wait(NULL) > 0);
}