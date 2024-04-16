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
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>


#define PORT 4409
#define MAXSIZE 1024
#define ARG_SIZE 5
#define MAXLEN 128
#define PIPE_BUFFER 4096
#define FILE_BUFF 1048576

// TODO: Check date format here
int parse_ip(char *client_ip, char **args){
    // this function will parse the client ip
    // with respect to a space and return the count of the arguments
    // tokenize the input and split with " "
    char *token;
    token = strtok(client_ip, " ");
    // create an argument array until we reach \0 character
    int i = 0;

    while(token != NULL && i<ARG_SIZE){
        args[i] = token;
        token = strtok(NULL, " ");
        i++;
    }
    args[i] = NULL;

    return i;
}

int validate_args(char **args, int arg_count){
    if((strcmp(args[0],"dirlist")==0 && arg_count==2)&&(strcmp(args[1],"-t")==0 || strcmp(args[1],"-a")==0)){
        return 1;
    }
    else if(strcmp(args[0],"w24fn")==0 && arg_count==2){
        return 1;
    }
    else if(strcmp(args[0],"w24fz")==0 && arg_count==3){
        off_t size_1 = atoll(args[1]);
        off_t size_2 = atoll(args[2]);
        if(size_1 >= 0 && size_2 >= 0 && size_1 <= size_2){
            return 1;
        }else{
            return 0;
        }
    }
    else if(strcmp(args[0],"w24ft")==0 && arg_count>=2 && arg_count<=4){
        return 1;
    }
    else if(strcmp(args[0],"w24fda")==0 && arg_count==2){
        return 1;
    }
    else if(strcmp(args[0],"w24fdb")==0 && arg_count==2){
        return 1;
    }
    else if(strcmp(args[0],"quitc")==0 && arg_count==1){
        return 1;
    }
    else{
        return 0;
    }
}

void send_parsed_input(int socket_fd, char **args, int count){
    int i = 0;
    int len;
    while(i < count && args[i]!=NULL){
        len = strlen(args[i]);
        if ((send(socket_fd,args[i], len,0)) == -1) {
            fprintf(stderr, "Failure Sending Message\n");
            close(socket_fd);
            exit(1);
        }
        i++;
    }
}

void create_file_dir(char *storage_dir){
    pid_t pid;
    int status;

    // Fork a child process
    if ((pid = fork()) < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Check if the directory exists
        struct stat st;
        if (stat(storage_dir, &st) == 0) {
            // Directory exists
            exit(EXIT_SUCCESS);
        }else{
            // Child process
            // Build the command arguments
            char *args[3];
            args[0] = "mkdir";
            args[1] = storage_dir;
            args[2] = NULL;
            
            // Execute command
            execvp("mkdir", args);

            // execvp only returns if an error occurs
            perror("execvp");
            exit(EXIT_FAILURE);
        }
        
    } else {
        // Parent process
        // Wait for the child process to complete
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Error: tar command failed\n");
            exit(EXIT_FAILURE);
        }
    }

}

void receive_file_from_server(int socket_fd, const char *output_file) {
    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // Receive the file in chunks and write it to the output file
    char buffer[FILE_BUFF] = {0};
    ssize_t bytes_received, bytes_written;
    int flag = 1;
    // while(flag){
        bytes_received = read(socket_fd, buffer, sizeof(buffer));
        if (bytes_received == 0){
            // We're done reading from the socket
            // break;
            // return;
            flag = 0;
        } else if (bytes_received < 0) {
            // handle errors
            perror("read");
            exit(EXIT_FAILURE);
        } else {

            void *p = buffer;
            while (bytes_received > 0) {
                int bytes_written = write(fd, p, bytes_received);
                if (bytes_written <= 0) {
                    // handle errors
                }
                bytes_received -= bytes_written;
                p += bytes_written;
            }
        }

    // }
    // printf("I am outside the loop.\n")

    // Close the file
    close(fd);

}


// count of files received
int file_count = 1;
int main(int argc, char *argv[])
{
    struct sockaddr_in server_info;
    struct hostent *he;
    int socket_fd,num;
    char buffer[1024];

    char buff[1024];

    if (argc != 2) {
        fprintf(stderr, "Usage: client hostname\n");
        exit(1);
    }

    if ((he = gethostbyname(argv[1]))==NULL) {
        fprintf(stderr, "Cannot get host name\n");
        exit(1);
    }

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0))== -1) {
        fprintf(stderr, "Socket Failure!!\n");
        exit(1);
    }

    memset(&server_info, 0, sizeof(server_info));
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(PORT);
    server_info.sin_addr = *((struct in_addr *)he->h_addr);
    if (connect(socket_fd, (struct sockaddr *)&server_info, sizeof(struct sockaddr))<0) {
        //fprintf(stderr, "Connection Failure\n");
        perror("connect");
        exit(1);
    }

    //buffer = "Hello World!! Lets have fun\n";
    while(1) {
        fflush(stdout);
        memset(buffer, 0 , sizeof(buffer));
        // prompting client to enter the command
        write(STDOUT_FILENO, "Enter a command: ",17);
        
        long int read_bytes = read(STDIN_FILENO, buffer, MAXSIZE-1);

        if(read_bytes == -1){
            printf("Error reading the command.\n");
            exit(1);
        }else if (read_bytes == 1){
            // if user just presses an enter
            continue;
        }

        // the last character will be \n we should null terminate it
        buffer[read_bytes-1] = '\0';
        // parse the input that we get
        // creating the command array
        char *args[ARG_SIZE];
        int arg_count = parse_ip(buffer, args);
        // validate the input
        int validate_res = validate_args(args, arg_count);

        // if validation is done correctly we shall send it to the server
        if(validate_res){
            // send the command args to the server
            // call function to send the parsed input to server
            // send_parsed_input(socket_fd, args, arg_count);

            // Send the number of arguments to the server
            if (send(socket_fd, &arg_count, sizeof(int), 0) < 0) {
                perror("send failed");
                return -1;
            }


            // Send each argument to the server
            for (int i = 0; i < arg_count; i++) {
                if (send(socket_fd, args[i], MAXLEN * sizeof(char), 0) < 0) {
                    perror("send failed");
                    return -1;
                }
            }

            // 
            char buffer[PIPE_BUFFER];
            ssize_t bytes_received;

            bytes_received = recv(socket_fd, buffer, PIPE_BUFFER - 1, 0);
            if (bytes_received == 0) {
                // Server closed the connection
                printf("Server closed the connection\n");
                break; // Exit the loop
            } else if (bytes_received < 0) {
                // Error receiving data
                perror("recv failed");
                exit(EXIT_FAILURE);
            } else {
                // Null-terminate the received data to treat it as a string
                buffer[bytes_received] = '\0';
                // Print received data
                if(strcmp(buffer, "temp.tar.gz")==0)
                {
                    if(file_count == 1){
                        char *dir_path = getenv("HOME");
                        char storage_dir[MAXLEN];
                        // Copy the home path to the complete path buffer
                        strncpy(storage_dir, dir_path, MAXLEN);
                        // Check if the home path ends with a slash "/"
                        if (storage_dir[strlen(storage_dir) - 1] != '/') {
                            // If not, append a slash
                            strcat(storage_dir, "/");
                        }

                        // Append "/temp.tar" to the complete path
                        strcat(storage_dir, "w24project");
                        // create the directory
                        create_file_dir(storage_dir);
                    }
                    // use dynamic naming
                    char *home = getenv("HOME");
                    char filename[MAXLEN];

                    // Format the file name string with the count variable
                    sprintf(filename, "%s/w24project/temp-%d.tar.gz", home, file_count);

                    receive_file_from_server(socket_fd,filename);
                    file_count++;
                    printf("File received successfully.\n");

                }
                else
                {
                    printf("%s", buffer);

                }
            }



            

        }else{
            // dont send instead prompt client with proper sytanx of the commands
            printf("Invalid command.\nList of commands accepted.\n");
            printf("1. dirlist -a\n");
            printf("2. dirlist -t\n");
            printf("3. w24fn <filename>\n");
            printf("4. w24fz <size 1> <size 2>\n");
            printf("5. w24ft <extension 1> <extension 2> <extension 3> (2 and 3 are optional)\n");
            printf("6. w24fda <date> (format YYYY-MM-DD)\n");
            printf("7. w24fdb <date> (format YYYY-MM-DD)\n");
            printf("8. quitc\n");


        }

        
    }   

    close(socket_fd);   
    return 0;
}
