
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

struct message {
	char source[50];
	char target[50]; 
	char msg[200]; // message body
};

void terminate(int sig) {
	printf("Exiting....\n");
	fflush(stdout);
	exit(0);
}

int main() {
    int server;
    int target_fd;
    int dummyfd;
    struct message req;
    ssize_t read_bytes;
    char target_fifo_path[64];
    
    signal(SIGPIPE,SIG_IGN);
    signal(SIGINT,terminate);

    
    server = open("serverFIFO",O_RDONLY);
    if (server < 0) {
        perror("Error opening server FIFO for read");
        exit(1);
    }
    dummyfd = open("serverFIFO",O_WRONLY);
    if (dummyfd < 0) {
        perror("Error opening server FIFO dummy write");
        close(server);
        exit(1);
    }

    printf("Server listening for messages...\n");

    while (1) {
        
        read_bytes = read(server, &req, sizeof(struct message));

        if (read_bytes == sizeof(struct message)) {
            printf("Received a request from %s to send the message \"%s\" to %s.\n",req.source,req.msg,req.target);

        
            sprintf(target_fifo_path, "rsh_%s", req.target);
            
            
            target_fd = open(target_fifo_path, O_WRONLY);
            
            if (target_fd < 0) {
                perror("Error opening target FIFO for write (Client likely offline)");
            } else {
                if (write(target_fd, &req, sizeof(struct message)) < 0) {
                    perror("Error writing message to target FIFO");
                } else {
                    printf("Message successfully routed to %s.\n", req.target);
                }
                
                close(target_fd);
            }
            
        } else if (read_bytes == 0) {
            printf("Server FIFO closed. Re-opening...\n");
            close(server);
            close(dummyfd);
            server = open("serverFIFO", O_RDONLY);
            dummyfd = open("serverFIFO", O_WRONLY);
            if (server < 0 || dummyfd < 0) {
                perror("Error re-opening server FIFO");
                exit(1);
            }
        } else if (read_bytes < 0) {
            perror("Error reading from server FIFO");
            break; 
        }
    }
    
    close(server);
    close(dummyfd);
    return 0;
}

