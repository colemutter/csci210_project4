#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>

#define N 13

extern char **environ;
char uName[20];

char *allowed[N] = {"cp","touch","mkdir","ls","pwd","cat","grep","chmod","diff","cd","exit","help","sendmsg"};

struct message {
	char source[50];
	char target[50]; 
	char msg[200];
};

void terminate(int sig) {
        printf("Exiting....\n");
        fflush(stdout);
        exit(0);
}

void sendmsg (char *user, char *target, char *msg) {
    // TODO:
    // Send a request to the server to send the message (msg) to the target user (target)
    // by creating the message structure and writing it to server's FIFO
    
    char *server_fifo = "serverFIFO";
    int fd;
    struct message message_struct;

    
    fd = open(server_fifo, O_WRONLY);
    if (fd < 0) {
        perror("Error opening server FIFO");
        return;
    }

    strncpy(message_struct.source, user, sizeof(message_struct.source) - 1);
    message_struct.source[sizeof(message_struct.source) - 1] = '\0';
    
    strncpy(message_struct.target, target, sizeof(message_struct.target) - 1);
    message_struct.target[sizeof(message_struct.target) - 1] = '\0';
    
    strncpy(message_struct.msg, msg, sizeof(message_struct.msg) - 1);
    message_struct.msg[sizeof(message_struct.msg) - 1] = '\0';

    if (write(fd, &message_struct, sizeof(struct message)) < 0) {
        perror("Error writing to server FIFO");
    } else {
        
    }

    close(fd);
}

void* messageListener(void *arg) {
    // TODO:
    // Read user's own FIFO in an infinite loop for incoming messages
    // The logic is similar to a server listening to requests
    // print the incoming message to the standard output in the
    // following format
    // Incoming message from [source]: [message]
    // put an end of line at the end of the message

    // 1. Construct the user's private FIFO path (e.g., /tmp/rsh_username)
    char fifo_path[64];
    sprintf(fifo_path, "rsh_%s", (char*)arg); // uName is passed as arg

    // 2. Create the FIFO if it doesn't exist
    if (mkfifo(fifo_path, 0666) < 0) {
        // mkfifo returns -1 if it already exists, which is fine, 
        // but an actual error should be handled.
        if (errno != EEXIST) {
            perror("mkfifo error");
            pthread_exit((void*)-1);
        }
    }
    
    // 3. Open the FIFO for reading (blocking open is standard for listeners)
    int fd = open(fifo_path, O_RDONLY);
    if (fd < 0) {
        perror("Error opening user FIFO for reading");
        pthread_exit((void*)-1);
    }
    
    // Use O_RDWR to prevent EOF when the writer closes the FIFO. 
    // If O_RDONLY is used, the loop breaks when the server closes its write-end.
    // Let's open another descriptor for O_WRONLY to keep the FIFO open (a common trick)
    int fd_keep_open = open(fifo_path, O_WRONLY); 
    if (fd_keep_open < 0) {
        perror("Error opening user FIFO for keeping it open");
        close(fd);
        pthread_exit((void*)-1);
    }

    struct message incoming_msg;
    int read_bytes;

    while (1) {
        
        read_bytes = read(fd, &incoming_msg, sizeof(struct message));

        if (read_bytes == sizeof(struct message)) {
            
            printf("\nIncoming message from %s: %s\n", incoming_msg.source, incoming_msg.msg);
            
            fprintf(stderr,"rsh>");
            fflush(stdout); 
        } else if (read_bytes == 0) {
            
            close(fd);
            fd = open(fifo_path, O_RDONLY);
            if (fd < 0) {
                 perror("Error re-opening user FIFO");
                 break; 
            }
        } else if (read_bytes < 0) {
            perror("Error reading from user FIFO");
            break; 
        }
    }

    close(fd);
    close(fd_keep_open);

    pthread_exit((void*)0);
}

int isAllowed(const char*cmd) {
	int i;
	for (i=0;i<N;i++) {
		if (strcmp(cmd,allowed[i])==0) {
			return 1;
		}
	}
	return 0;
}

int main(int argc, char **argv) {
    pid_t pid;
    char **cargv; 
    char *path;
    char line[256];
    int status;
    posix_spawnattr_t attr;
	pthread_t listener_thread;

    if (argc!=2) {
	printf("Usage: ./rsh <username>\n");
	exit(1);
    }
    signal(SIGINT,terminate);

    strcpy(uName,argv[1]);

    
	if (pthread_create(&listener_thread, NULL, messageListener, (void*)uName) != 0) {
        perror("Error creating message listener thread");
        exit(1);
    }




    while (1) {

	fprintf(stderr,"rsh>");

	if (fgets(line,256,stdin)==NULL) continue;

	if (strcmp(line,"\n")==0) continue;

	line[strlen(line)-1]='\0';

	char cmd[256];
	char line2[256];
	strcpy(line2,line);
	strcpy(cmd,strtok(line," "));

	if (!isAllowed(cmd)) {
		printf("NOT ALLOWED!\n");
		continue;
	}

	if (strcmp(cmd,"sendmsg")==0) {
        // TODO: Create the target user and
        // the message string and call the sendmsg function

        // NOTE: The message itself can contain spaces
        // If the user types: "sendmsg user1 hello there"
        // target should be "user1" 
        // and the message should be "hello there"
        
        char *ptr = line2 + strlen(cmd); 
        
        while (*ptr == ' ') ptr++;
        
        char *target_start = ptr;
        
        char *target_end = strchr(target_start, ' ');

        if (target_end == NULL) {
            printf("sendmsg: you have to specify a message\n");
            continue;
        }
        
        *target_end = '\0';
        char *target = target_start;

        if (strlen(target) == 0) {
            printf("sendmsg: you have to specify target user\n");
            continue;
        }

        char *msg_start = target_end + 1;
        while (*msg_start == ' ') msg_start++;

        if (strlen(msg_start) == 0) {
            printf("sendmsg: you have to specify a message\n");
            continue;
        }

        sendmsg(uName, target, msg_start);

        continue;
    }

	if (strcmp(cmd,"exit")==0) break;

	if (strcmp(cmd,"cd")==0) {
		char *targetDir=strtok(NULL," ");
		if (strtok(NULL," ")!=NULL) {
			printf("-rsh: cd: too many arguments\n");
		}
		else {
			chdir(targetDir);
		}
		continue;
	}

	if (strcmp(cmd,"help")==0) {
		printf("The allowed commands are:\n");
		for (int i=0;i<N;i++) {
			printf("%d: %s\n",i+1,allowed[i]);
		}
		continue;
	}

	cargv = (char**)malloc(sizeof(char*));
	cargv[0] = (char *)malloc(strlen(cmd)+1);
	path = (char *)malloc(9+strlen(cmd)+1);
	strcpy(path,cmd);
	strcpy(cargv[0],cmd);

	char *attrToken = strtok(line2," "); /* skip cargv[0] which is completed already */
	attrToken = strtok(NULL, " ");
	int n = 1;
	while (attrToken!=NULL) {
		n++;
		cargv = (char**)realloc(cargv,sizeof(char*)*n);
		cargv[n-1] = (char *)malloc(strlen(attrToken)+1);
		strcpy(cargv[n-1],attrToken);
		attrToken = strtok(NULL, " ");
	}
	cargv = (char**)realloc(cargv,sizeof(char*)*(n+1));
	cargv[n] = NULL;

	// Initialize spawn attributes
	posix_spawnattr_init(&attr);

	// Spawn a new process
	if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0) {
		perror("spawn failed");
		exit(EXIT_FAILURE);
	}

	// Wait for the spawned process to terminate
	if (waitpid(pid, &status, 0) == -1) {
		perror("waitpid failed");
		exit(EXIT_FAILURE);
	}

	// Destroy spawn attributes
	posix_spawnattr_destroy(&attr);

    }
    return 0;
}
