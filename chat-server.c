
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>

void broadcast_message(const char *sender_nickname, const char *message);
void broadcast_nickname_change(const char *old_nick, const char *new_nick, const char *client_ip, uint16_t client_port);
void broadcast_disconnection(const char *nickname, const char *client_ip, uint16_t client_port);
void *handle_client(void *arg);

#define BACKLOG 10
#define BUF_SIZE 4096
#define NAME_SIZE 50
// number of characters in a nick request = 6 (/nick )
#define NICK_REQUEST_SIZE 6

struct Client {
    int conn_fd;
    char nickname[NAME_SIZE];
    char client_ip[INET_ADDRSTRLEN];
    uint16_t client_port;
};

// Array to store connected clients
struct Client clients[BACKLOG];
// Number of connected clients
int num_clients = 0; 

int main(int argc, char *argv[]) {
    char *listen_port;
    int listen_fd;
    struct addrinfo hints, *res;
    int rc;

    listen_port = argv[1];

    // create a socket 
    listen_fd = socket(PF_INET, SOCK_STREAM, 0);

    // bind it to a port 
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if((rc = getaddrinfo(NULL, listen_port, &hints, &res)) != 0) {
        printf("getaddrinfo failed: %s\n", gai_strerror(rc));
        exit(1);
    }

    bind(listen_fd, res->ai_addr, res->ai_addrlen);

    // start listening 
    listen(listen_fd, BACKLOG);

    // infinite loop of accepting new connections and handling them 
    // new thread for each connected client
    while(1) {
        int *conn_fd = malloc(sizeof(int));
        *conn_fd = accept(listen_fd, NULL, NULL);
        if(*conn_fd < 0) {
            perror("accept failed");
            free(conn_fd);
            continue;
        }

        pthread_t thread;
        if(pthread_create(&thread, NULL, handle_client, conn_fd) != 0) {
            perror("Failed to create thread");
            close(*conn_fd);
            free(conn_fd);
            continue;
        }

        // detach the thread to handle the client 
        pthread_detach(thread);
    }
    return 0;
}

void broadcast_message(const char *sender_nickname, const char *message) {
    char msg_with_name[BUF_SIZE + NAME_SIZE];
    snprintf(msg_with_name, sizeof(msg_with_name), "%s: %s", sender_nickname, message);
    
    // broadcast all other messages to all client
    for (int i = 0; i < num_clients; ++i) {
        send(clients[i].conn_fd, msg_with_name, strlen(msg_with_name) + 1, 0);
    }
}

void broadcast_nickname_change(const char *old_nick, const char *new_nick, const char *client_ip, uint16_t client_port) {
    char msg[BUF_SIZE];
    snprintf(msg, sizeof(msg), "User %s (%s:%d) is now known as %s.\n", old_nick, client_ip, client_port, new_nick);

    // Broadcast the nickname change to all clients
    for (int i = 0; i < num_clients; ++i) {
        send(clients[i].conn_fd, msg, strlen(msg), 0);
    }
}

void broadcast_disconnection(const char *nickname, const char *client_ip, uint16_t client_port) {
    char msg[BUF_SIZE];
    snprintf(msg, sizeof(msg), "User %s (%s:%d) has disconnected.\n", nickname, client_ip, client_port);

    // Broadcast the disconnection message to all clients except the disconnected one
    for (int i = 0; i < num_clients; ++i) {
        if (strcmp(clients[i].nickname, nickname) != 0) {
            send(clients[i].conn_fd, msg, strlen(msg), 0);
        }
    }
}

void *handle_client(void *arg) {
    int conn_fd = *((int*)arg);
    // free dynamically allocated mem from the fd 
    free(arg);

    char client_ip[INET_ADDRSTRLEN];
    uint16_t client_port;
    struct sockaddr_in remote_sa;
    socklen_t addrlen = sizeof(remote_sa);
    getpeername(conn_fd, (struct sockaddr*)&remote_sa, &addrlen);
    strcpy(client_ip, inet_ntoa(remote_sa.sin_addr));
    client_port = ntohs(remote_sa.sin_port);

    struct Client current_client;
    current_client.conn_fd = conn_fd;
    strcpy(current_client.client_ip, inet_ntoa(remote_sa.sin_addr));
    current_client.client_port = ntohs(remote_sa.sin_port);
    snprintf(current_client.nickname, sizeof(current_client.nickname), "User %s:%d", current_client.client_ip, current_client.client_port);

    printf("new connection from %s:%d\n", client_ip, client_port);

    clients[num_clients++] = current_client;

    char buf[BUF_SIZE];
    char nickname[NAME_SIZE] = "unknown";
    int bytes_received; 

    while((bytes_received = recv(conn_fd, buf, BUF_SIZE, 0)) > 0) {
        // Null terminate the recieved data
        buf[bytes_received] = '\0';

        // Check if the user message wants to change their nickname 
        if(strncmp(buf, "/nick ", NICK_REQUEST_SIZE) == 0) {
            // copy old nickname to new nickname
            char old_nick[NAME_SIZE];
            strncpy(old_nick, nickname, sizeof(old_nick) - 1); 
            old_nick[sizeof(old_nick) - 1] = '\0';
            
            // Extract the new nickname from the command, removing newline character
            char *newline_pos = strchr(buf + NICK_REQUEST_SIZE, '\n');
            size_t copy_length = (newline_pos != NULL) ? (size_t)(newline_pos - (buf + NICK_REQUEST_SIZE)) : (size_t)(bytes_received - NICK_REQUEST_SIZE);
            strncpy(nickname, buf + NICK_REQUEST_SIZE, copy_length);
            nickname[copy_length] = '\0';

            printf("User %s (%s:%d) is now known as %s\n", old_nick, client_ip, client_port, nickname);

            // Broadcast the nickname change to all clients
            broadcast_nickname_change(old_nick, nickname, client_ip, client_port);
            continue;
        }

        // Broadcast the message to all other clients
        broadcast_message(nickname, buf);
        
        send(conn_fd, buf, bytes_received, 0);
    }

    printf("Lost connection from %s\n", nickname);
    broadcast_disconnection(nickname, client_ip, client_port);

    // Remove the disconnected client from the list
    for (int i = 0; i < num_clients; ++i) {
        if (clients[i].conn_fd == conn_fd) {
            for (int j = i; j < num_clients - 1; ++j) {
                clients[j] = clients[j + 1];
            }
            --num_clients;
            break;
        }
    }

    close(conn_fd);
    return NULL;
}