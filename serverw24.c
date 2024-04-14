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
#define MAXSIZE 128
#define PIPE_BUFFER 4096





// TODO Cleanup the array
// TODO look into code of dirlist -t

void crequest(int client_fd);
void sigchild_handler(int signo);
void handle_incoming_strings(char *args[], int client_fd, int *num_args);
void list_dirs_newfirst(int client_fd);

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

    size = sizeof(struct sockaddr_in);  
    while(1) {

        if ((client_fd = accept(socket_fd, (struct sockaddr *)&dest, &size))==-1) {
            //fprintf(stderr,"Accept Failure\n");
            perror("accept");
            exit(1);
        }
        printf("Server got connection from client %s\n", inet_ntoa(dest.sin_addr));

        // Fork a child process to handle client request
        int pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) {
            // Child process
            close(socket_fd); // Close server socket in child process
            crequest(client_fd); // Handle client request
            close(client_fd);
            exit(0);
        } else {
            // Parent process
            close(client_fd); // Close client socket in parent process
        }


        // crequest(client_fd);
        
    }
    return 0;
}



void crequest(int client_fd){
    
    // here we shall actually handle the client
    // enter an infinite loop to handle the commands
    char *args[ARG_SIZE];
    int read_size;
    int num_args;
    // char buffer[10241];
    while(1){
        
        handle_incoming_strings(args, client_fd, &num_args);
        
        // printf("Received command: %s\n",args[0]);
        
        if(strcmp(args[0],"dirlist")==0){
            if(strcmp(args[1],"-a")==0){
                //list_dirs_alphabetically(client_fd);
            }else{
                list_dirs_newfirst(client_fd);
            }

        }
        else if(strcmp(args[0],"w24fn")==0){

        }
        else if(strcmp(args[0],"w24fz")==0){
            
        }
        else if(strcmp(args[0],"w24ft")==0){
            
        }
        else if(strcmp(args[0],"w24fda")==0){
            
        }
        else if(strcmp(args[0],"w24fdb")==0){
            
        }
        else if (strcmp(args[0],"quitc")==0) {
            printf("Connection closed\n");
            break;
        }
        else{

        }

    }

    close(client_fd);
    exit(0);
    
}

void sigchild_handler(int signo){
    int status;
    waitpid(-1, &status, WNOHANG);
}

void handle_incoming_strings(char *args[], int client_fd, int *num_args){

    // Receive a message from client
    // Receive the number of arguments sent by the client
    if (recv(client_fd, num_args, sizeof(int), 0) < 0) {
        perror("recv failed");
        exit(EXIT_FAILURE);
    }

    // printf("Received number of arguments: %d\n", num_args);

    // Receive each argument sent by the client
    for (int i = 0; i < *num_args; i++) {
        args[i] = malloc(MAXSIZE * sizeof(char));
        if (recv(client_fd, args[i], MAXSIZE * sizeof(char), 0) < 0) {
            perror("recv failed");
            exit(EXIT_FAILURE);
        }
        // printf("Received argument %d: %s\n", i + 1, args[i]);
    }


}

void list_dirs_newfirst(int client_fd){

    char *ls_command = "sh";
    char *ls_arguments[] = {"sh", "-c", "ls -1dt ~/\*/", NULL};

    char buffer[PIPE_BUFFER] = {0};
    int cp_op = dup(STDOUT_FILENO);
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } 
    else if (pid == 0) { 
        // Child process
        close(pipe_fd[0]); // Close the read end of the pipe
        // int cp_out = dup(STDOUT_FILENO);
        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        close(pipe_fd[1]); // Close the original write end of the pipe

        // Execute ls command
        if (execvp(ls_command, ls_arguments) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else { // Parent process
        close(pipe_fd[1]); // Close the write end of the pipe
        ssize_t bytes_read;
        while ((bytes_read = read(pipe_fd[0], buffer, PIPE_BUFFER)) > 0) {
            if (send(client_fd, buffer, bytes_read, 0) != bytes_read) {
                perror("send");
                exit(EXIT_FAILURE);
            }
        }
        close(pipe_fd[0]); // Close the read end of the pipe
        dup2(cp_op, STDOUT_FILENO);
        memset(buffer, 0, sizeof(buffer)); // Clear buffer
    }


}