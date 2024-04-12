#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>

#define PORT 8000
#define BACKLOG 20
#define ARG_SIZE 5
#define BUFFER_SIZE 32


// TODO: Register a handler for sigchild to prevent zombie process
// TODO: handle the incoming parsed vector
// TODO: Call appropriate function to handle the command

void crequest(int client_fd);
void sigchild_handler(int signo);
void handle_incoming_strings(char **args, int client_fd);

int main()
{

    // registering a signal for sig child sent by children
    // so that processes that exits while running in background gets acknowledged
    signal(SIGCHLD, sigchild_handler);
    struct sockaddr_in server;
    struct sockaddr_in dest;
    int status,socket_fd, client_fd,num;
    socklen_t size;

    char *buff;
//  memset(buffer,0,sizeof(buffer));
    int yes =1;



    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0))== -1) {
        fprintf(stderr, "Socket failure!!\n");
        exit(1);
    }

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }
    memset(&server, 0, sizeof(server));
    memset(&dest,0,sizeof(dest));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY; 
    if ((bind(socket_fd, (struct sockaddr *)&server, sizeof(struct sockaddr )))== -1)    { //sizeof(struct sockaddr) 
        fprintf(stderr, "Binding Failure\n");
        exit(1);
    }

    if ((listen(socket_fd, BACKLOG))== -1){
        fprintf(stderr, "Listening Failure\n");
        exit(1);
    }

    printf("Server listening on port: %d\n", PORT);

    while(1) {
        size = sizeof(struct sockaddr_in);  

        if ((client_fd = accept(socket_fd, (struct sockaddr *)&dest, &size))==-1) {
            //fprintf(stderr,"Accept Failure\n");
            perror("accept");
            exit(1);
        }
        printf("Server got connection from client %s\n", inet_ntoa(dest.sin_addr));
        //buffer = "Hello World!! I am networking!!\n";

        crequest(client_fd);
        
    }
    return 0;
}



void crequest(int client_fd){
    pid_t pid;

    // Fork a child process
    pid = fork();
    if (pid < 0) { // Error forking
        fprintf(stderr, "Error forking\n");
        exit(EXIT_FAILURE);
    }else if(pid == 0){
        // child process
        // here we shall actually handle the client
        // enter an infinite loop to handle the commands
        char *args[ARG_SIZE];
        // int num;
        // char buffer[10241];
        while(1){



            handle_incoming_strings(args, client_fd);
            // if ((num = recv(client_fd, buffer, 10240,0))== -1) {
            //     //fprintf(stderr,"Error in receiving message!!\n");
            //     perror("recv");
            //     exit(1);
            // }   
            if (strcmp(args[0],"quitc")==0) {
                printf("Connection closed\n");
                break;
            }
            else{
                int i = 0;
                while(args[i]!=NULL){
                    printf("%s\n",args[i]);
                    i++;
                }
            }
        //  num = recv(client_fd, buffer, sizeof(buffer),0);
            // buffer[num] = '\0';
            // printf("Message received: %s\n", buffer);            
            



        }

        close(client_fd);
        exit(0);

    }else{
        // this is the parent here we shall do nothing 
        // just close the client side socket 
        // because we dont want parent to interfere with the child's communication
        // close(socket_fd);        
        close(client_fd);
    }
    
}

void sigchild_handler(int signo){
    int status;
    waitpid(-1, &status, WNOHANG);
}

void handle_incoming_strings(char **args, int client_fd){
    char buffer[BUFFER_SIZE];
    int num_bytes;
    int count = 0;
    // Read and handle each string received from the client
    while ((num_bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0 && count < ARG_SIZE) {
        buffer[num_bytes] = '\0';  // Null-terminate the received data
        args[count] = strdup(buffer);

        if(args[count] == NULL) {
            perror("Error allocating memory for string");
            exit(EXIT_FAILURE);
        }

        count++;
        
    }

    args[count] = NULL;

    if (num_bytes == 0) {
        // Client closed the connection
        printf("Client closed the connection\n");
    } else if (num_bytes == -1) {
        // Error receiving data
        perror("Error receiving data from client");
        exit(EXIT_FAILURE);
    }

}
