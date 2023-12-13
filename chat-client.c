
/* 
chat client
*/

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <sys/select.h>
#include <pthread.h>
#include <strings.h>

#define BUF_SIZE 4096
static int conn_fd = 0;

void *send_to_server();
void *receive_from_server();

int main(int argc, char *argv[])
{
    if(argc != 3) {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        return 1;
    }

    char *dest_hostname, *dest_port;
    dest_hostname = argv[1];
    dest_port     = argv[2];

    struct addrinfo hints, *res;
    int rc;

    // create a socket 
    if((conn_fd = socket (PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    // find the IP address of the server
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if((rc = getaddrinfo(dest_hostname, dest_port, &hints, &res)) != 0) {
        printf("getaddrinfo failed: %s\n", gai_strerror(rc));
        exit(1);
    }

    // connect to the server 
    if(connect(conn_fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        exit(1);
    }

    printf("Connected\n");

    // Send message and communicate with the server through thread
    pthread_t send_thread;
    if(pthread_create(&send_thread, NULL, *send_to_server, NULL) != 0) {
        perror("pthread_create");
        exit(1);
    }

    // similarly receive message and communicate with the server through thread
    pthread_t recv_thread;
    if(pthread_create(&recv_thread, NULL, *receive_from_server, NULL) != 0) {
        perror("pthread_create");
        exit(1);
    }

    // Waiting for both threads
    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);
    
    close(conn_fd);
    return 0;
}


void *send_to_server() {
    int bytes_read;
    char buf[BUF_SIZE + 1];

    while(1) {
        if((bytes_read = read(STDIN_FILENO, buf, BUF_SIZE)) < 0) {
            perror("read");
            exit(1);
        }

        // Client exits with Ctrl-D
        if(bytes_read == 0) {
            printf("Exiting. \n");
            close(conn_fd);
            exit(1);
        }
        
        // send the inputted data to server
        buf[bytes_read] = '\0';
        send(conn_fd, buf, bytes_read + 1, 0) ;
    }

    return NULL;
}


void *receive_from_server() {
    int bytes_received;
    char buf[BUF_SIZE + 1];
    
    while(1) {
        if((bytes_received = recv(conn_fd, buf, BUF_SIZE, 0)) < 0) {
            perror("recv");
            exit(1);
        }
        // Server exits with Ctrl-C
        if(bytes_received == 0) {
            printf("Connection closed by remote host.\n");
            close(conn_fd);
            exit(1);
        }

        buf[bytes_received] = '\0';
        printf("%s", buf) ;
        fsync(STDOUT_FILENO);
    }
    return NULL;
}