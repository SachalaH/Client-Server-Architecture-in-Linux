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
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

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
void list_dirs_alphabetically(int client_fd);
void search_file_info(char *filename, char *path, int *found, char *details);

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
        fflush(stdout);

        memset(args,0,sizeof(args));
        
        handle_incoming_strings(args, client_fd, &num_args);
        
        // printf("Received command: %s\n",args[0]);
        
        if(strcmp(args[0],"dirlist")==0){
            if(strcmp(args[1],"-a")==0){
                printf("Command received: dirlist -a\n");
                list_dirs_alphabetically(client_fd);
            }else{
                printf("Command received: dirlist -t\n");
                list_dirs_newfirst(client_fd);
            }

        }
        else if(strcmp(args[0],"w24fn")==0){
            printf("Command received: w24fn\n");
            char *filename = args[1];
            char *root = getenv("HOME");
            int found = 0;
            char details[PIPE_BUFFER];
            search_file_info(filename, root, &found, details);
            // If the file was not found, print an error message
            if (!found)
            {
                char *msg = "File not found.\n";
                int bytes_sent = send(client_fd, msg, strlen(msg), 0);
                if (bytes_sent == -1){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                int bytes_sent = send(client_fd, details, strlen(details), 0);
                if (bytes_sent == -1){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
            }

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
        memset(buffer, 0, sizeof(buffer)); // Clear buffer
    }


}

void list_dirs_alphabetically(int client_fd){

    char *ls_command = "sh";
    char *ls_arguments[] = {"sh", "-c", "ls -1d ~/\*/ | sort", NULL};

    char buffer[PIPE_BUFFER] = {0};
    
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
        memset(buffer, 0, sizeof(buffer)); // Clear buffer
    }

}

void search_file_info(char *filename, char *path, int *found, char *details){

    DIR *dir;
    struct dirent *dp;
    struct stat st;
    char buf[MAXSIZE];

    // Open the directory at the specified path
    dir = opendir(path);
    if (dir == NULL){
        perror("opendir");
        return;
    }

    // Traverse the directory
    while ((dp = readdir(dir)) != NULL){
        // Check if the current directory entry is a hidden directory

        if (dp->d_name[0] == '.'){

            continue; // Skip hidden directories

        }
        // Check if the current directory entry is the target file
        if (strcmp(dp->d_name, filename) == 0){
            // Construct the full path to the file
            sprintf(buf, "%s/%s", path, filename);
            // Get the file stats
            if (stat(buf, &st) == 0){
                // Format file information into the result string
                snprintf(details, PIPE_BUFFER, "File Path:%s\nFilename:%s\nFile Size:%ld\nPermissions: %o\nCreate At:%s", path, filename, st.st_size, st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO), ctime(&st.st_mtime));
                // Set the found flag
                *found = 1;
                // Break out of the loop
                break;
            }
        }
        // Recursively search subdirectories
        if (dp->d_type == DT_DIR && strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0)
        {
            // Construct the full path to the subdirectory
            sprintf(buf, "%s/%s", path, dp->d_name);
            // Recursively search the subdirectory
            search_file_info(filename, buf, found, details);
            // If the file is found, break out of the loop
            if (*found)
            {
                break;
            }
        }
    }

    // Close the directory
    closedir(dir);
}