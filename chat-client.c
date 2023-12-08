
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

#define BUF_SIZE 4096

void print_time() {
    time_t curr_time = time(NULL);
    struct tm *tm_info = localtime(&curr_time);
    // get the time
    printf("%02d:%02d:%02d: ", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
}

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
    int conn_fd;
    int n;
    int rc;
    char buf[BUF_SIZE];

    // create a socket 
    conn_fd = socket(PF_INET, SOCK_STREAM, 0);

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
        exit(2);
    }

    printf("Connected\n");

    // infinite loop for reading and recieving data
    // might have to change this since instruction says
    // When a client sends a message, every client connected to the server should receive it and print it out,
    // with proper attribution.
    while(1) {
        fd_set set;
        FD_ZERO(&set);
        // add server connection fd to set
        FD_SET(conn_fd, &set);
        // add stdrd input input fd to set
        FD_SET(STDIN_FILENO, &set);

        // check if any fds ready to read 
        if(select(conn_fd + 1, &set, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        // check if any fds ready to read, recieve data
        if(FD_ISSET(conn_fd, &set)) {
            n = recv(conn_fd, buf, BUF_SIZE, 0);
            // if recv returns 0 or neg number the server closed the connection
            if(n <= 0) {
                printf("Connection closed by remote host.\n");
                break;
            }
            print_time(); 
            // print received message from server from other connections
            printf("%.*s", n, buf);
        }
       
        if(FD_ISSET(STDIN_FILENO, &set)) {
            // read user input from terminal
            n = read(STDIN_FILENO, buf, BUF_SIZE);
            if(n <= 0) {
                printf("Exiting.\n");
                break;
            }
            // check if the message is a nickname change
            if(strncmp(buf, "/nick", 6) == 0) {
                printf("Nickname change command sent.\n");
            }
            // send user input to server
            send(conn_fd, buf, n, 0);
        }
    }
    close(conn_fd);
    return 0;
}

