
/* 
chat server
*/

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
#include <errno.h>
#include <signal.h>
#include <time.h>

#define BACKLOG 10
#define BUF_SIZE 80
// number of characters in a nick request = 6 (/nick )
#define NICK_REQUEST_SIZE 6
static struct Client *head = NULL;
static pthread_mutex_t list_mutex;
// used to handle large server data
#define MAX_MESSAGE_LEN 80

struct Client {
    int conn_fd;
    char *name;
    char *ip;
    uint16_t port;
    struct Client* next; 
    struct Client* prev;
};

void *handle_client(void *arg);
void print_time();
void broadcast_message(char *message, size_t message_length);
void *create_client(int conn_fd, char *ip, uint16_t port);
void *delete_client(struct Client *client);
void close_handler(int sig);


int main(int argc, char *argv[]) {
    char *listen_port, *remote_ip;
    int listen_fd, rc, conn_fd;
    uint16_t remote_port;
    socklen_t addrlen;
    struct addrinfo hints, *res;
    struct sigaction sa;
    struct sockaddr_in remote_sa;

    // Initialize mutex for linked-list
    pthread_mutex_init(&list_mutex, NULL);

    // error check for mutex
    if(pthread_mutex_init(&list_mutex, NULL) != 0) {
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }
    
    // Set up sigaction struct that defines what to do on receipt of a signal
    memset (&sa, '\0', sizeof (sa));
    sa.sa_handler = close_handler;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = 0;

    // associate that action with a particular signal - SIGINT (ctrl-C)
    if(sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
        exit(1);
    }

    listen_port = argv[1];

    // create a socket 
    if((listen_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    // bind it to a port 
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if((rc = getaddrinfo(NULL, listen_port, &hints, &res)) != 0) {
        close(listen_fd);
        printf("getaddrinfo failed: %s\n", gai_strerror(rc));
        exit(1);
    }

    if((bind(listen_fd, res->ai_addr, res->ai_addrlen)) < 0) {
        close(listen_fd);
        perror("bind");
        exit(1);
    }

    // start listening 
    if(listen(listen_fd, BACKLOG) < 0) {
        close(listen_fd);
        perror("listen");
        exit(1);
    }

    // infinite loop of accepting new connections and handling them 
    // new thread for each connected client
    while(1) {
        addrlen = sizeof(remote_sa);
        if((conn_fd = accept(listen_fd, (struct sockaddr *) &remote_sa, &addrlen)) < 0) {
            perror("accept");
            close(conn_fd);
            continue;
        }

        // our communication partner
        remote_ip = inet_ntoa( remote_sa.sin_addr);
        remote_port = ntohs(remote_sa.sin_port);
        printf("new connection from %s:%d\n", remote_ip, remote_port);
        
        // Create new Client and its thread.
        pthread_t thread_id;
        struct Client* new_client;
        
        pthread_mutex_lock(&list_mutex);
        if((new_client = create_client(conn_fd, remote_ip, remote_port)) == NULL) {
            printf("Failed to create new client.\n");
            close(conn_fd);
            // unlock mutex if creation fails
            pthread_mutex_unlock(&list_mutex);
        
        } else {
            // unlock after adding client to list
            pthread_mutex_unlock(&list_mutex);

            if(pthread_create(&thread_id, NULL, &handle_client, (void *) new_client) != 0) {
                perror("pthread_create");
                delete_client(new_client);
                // should lock again if we delete the client
                pthread_mutex_lock(&list_mutex);
            }
        }
        // detach the thread to handle the client 
        pthread_mutex_unlock(&list_mutex);
    }
    
    // Close socket and destroy its mutex
    close(listen_fd);
    pthread_mutex_destroy(&list_mutex);
    return 0;
}


void *handle_client(void *arg) {
    struct Client *client = (struct Client *) arg;
    char buf[BUF_SIZE], server_message[BUF_SIZE]; 
    int bytes_received;
    int message_length = 0;
    char* get_curr_time();
    // buffer to check messages 
    char message[MAX_MESSAGE_LEN] = {0};
    int buf_message_len = 0;

    while((bytes_received = recv(client->conn_fd, buf, BUF_SIZE, 0)) > 0) {
        // check if appending this will exceed the buffer
        // if it does then exit user and send error
        if(buf_message_len + bytes_received >= MAX_MESSAGE_LEN) {
            const char* error_message = "Error: Your message is too long.\n";
            send(client->conn_fd, error_message, strlen(error_message), 0);
            printf("Error: Received message exceeds maximum allowed length.\n");

            //cleanup 
            close(client->conn_fd);
            pthread_mutex_lock(&list_mutex);
            delete_client(client);
            pthread_mutex_unlock(&list_mutex);
            return NULL;
        }

        // append the data to buf for any messages
        memcpy(message + buf_message_len, buf, bytes_received);
        buf_message_len += bytes_received;
        message[buf_message_len] = '\0';

        // check for regular messages 
        if(strchr(message, '\n') != NULL) {
            // Check if the user message wants to change their nickname 
            if(strncmp(buf, "/nick ", NICK_REQUEST_SIZE) == 0) {
                strtok(message, " ");
                char *new_name = strtok(NULL, "\n");

                if(new_name != NULL) {
                    char *old_name = client->name;
                    client->name = strdup(new_name);

                    // error check for strdup
                    if(client->name == NULL) {
                        perror("strdup");

                        // should close client connection and clean up
                        close(client->conn_fd);
                        pthread_mutex_lock(&list_mutex);
                        delete_client(client);
                        pthread_mutex_unlock(&list_mutex);
                        return NULL;
                    }

                    if(old_name) {
                        message_length = snprintf(server_message, MAX_MESSAGE_LEN, "User %s (%s:%d) is now known as %s.\n", old_name, client->ip, client->port, client->name);
                        // free becaise it is not NULL
                        free(old_name);
                    } else {
                        message_length = snprintf(server_message, MAX_MESSAGE_LEN, "User unknown (%s:%d) is now known as %s.\n", client->ip, client->port, client->name);
                    }
                    // Broadcast the nickname change to server
                    printf("%s", server_message);

                    // Broadcast the nickname change to all clients
                    broadcast_message(server_message, message_length);
                }
                } else {
                    // regular chat message 
                    if(client->name == NULL) {
                        message_length = snprintf(server_message, MAX_MESSAGE_LEN, "unknown: %s", message);
                    } else {
                        message_length = snprintf(server_message, MAX_MESSAGE_LEN, "%s: %s", client->name, message);
                    }

                    // broadcast nickname change to all clients
                    broadcast_message(server_message, message_length);
                }
                // reset the buf for next message 
                buf_message_len = 0;
                message[0] = '\0';
            }
    }

    // handle disconnections and revc error
    if(bytes_received == 0) {
        message_length = snprintf(server_message, BUF_SIZE, "User %s (%s:%d) has disconnected.\n", client->name ? client->name : "Unknown", client->ip, client->port);
        // boradcast disconnection to all other clients
        broadcast_message(server_message, message_length);
        printf("Lost connection from %s\n", client->name ? client->name : "Unknown");

    } else if(bytes_received < 0) {
        perror("recv");
    }
    // cleanup
    pthread_mutex_lock(&list_mutex);
    delete_client(client);
    pthread_mutex_unlock(&list_mutex);

    return NULL;
}


void print_time() {
    time_t curr_time = time(NULL);
    struct tm *tm_info = localtime(&curr_time);
    // get the time
    printf("%02d:%02d:%02d: ", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
}


char* get_curr_time() {
    static char buffer[30];
    time_t curr_time = time(NULL);
    struct tm *tm_info = localtime(&curr_time);

    // format 
    strftime(buffer, sizeof(buffer), "%H:%M:%S", tm_info);
    return buffer;
}


// Broadcast all other messages to all clients
void broadcast_message(char *message, size_t message_length) {
    pthread_mutex_lock(&list_mutex);
    struct Client *current_client = head;

    char time_stamp_message[BUF_SIZE];
    char *time_stamp = get_curr_time();
    snprintf(time_stamp_message, BUF_SIZE, "%s %s", time_stamp, message);
    
    while(current_client != NULL) {
        if (send(current_client->conn_fd, time_stamp_message, strlen(time_stamp_message), 0) < 0) {
            perror("send");
        }
        current_client = current_client->next;
    }
    pthread_mutex_unlock(&list_mutex);
}


void *create_client(int conn_fd, char *ip, uint16_t port) {
    struct Client *new_client;
    struct Client *curr_client;

    // Allocate memory for Client struct and initialize
    if((new_client = (struct Client *) malloc(sizeof(struct Client))) == NULL) {
        perror("malloc");
        pthread_mutex_unlock(&list_mutex);
        return NULL;
    }

    new_client->conn_fd = conn_fd;
    new_client->ip = ip;
    new_client->port = port;
    new_client->next = NULL;
    new_client->name = NULL;

    // If the Client list is empty, set as the first Client
    if(head == NULL) {
        new_client->prev = NULL;
        head = new_client;
        return new_client;
    } else {
        // if client list not empty append client to end 
        curr_client = head;
        while(curr_client->next != NULL) {
            curr_client = curr_client->next;
        }
        curr_client->next = new_client;
        new_client->prev = curr_client;
    }

    pthread_mutex_unlock(&list_mutex);
    return new_client;
}


void *delete_client(struct Client *client) {
    close(client->conn_fd);
    
    if(head == NULL) {
        return NULL;
    }

    // Check if first client in list
    if(head == client) {
        head = client->next;
    } else {
        client->prev->next = client->next;
    }
    
    // If it is not the last client on the list
    if(client->next != NULL) {
        client->next->prev = client->prev;
    }

    free(client);
    return NULL;
}


// Closes all connections to the server and frees all clients
void close_handler(int sig) {
    pthread_mutex_lock(&list_mutex);
    struct Client *curr_client = head;
    struct Client *next_client;

    while(curr_client != NULL) {
        // save next client before freeing the current
        next_client = curr_client->next; 

        // close client conection 
        close(curr_client->conn_fd);
        free(curr_client->name);
        free(curr_client);
        // move on to the next client 
        curr_client = next_client;
    }
    head = NULL;
    pthread_mutex_unlock(&list_mutex);
    pthread_mutex_destroy(&list_mutex);
    exit(0);
}