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

#define PORT 8000
#define MAXSIZE 1024
#define ARG_SIZE 5
#define MAXLEN 128
#define PIPE_BUFFER 4096

// TODO Handle quitc on the client side
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
        return 1;
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


            

        }else{
            // dont send instead prompt client with proper sytanx of the commands

        }



        
        // // Receive data from server
        // char *res[PIPE_BUFFER] = {0};
        // ssize_t bytes_received;
        // while ((bytes_received = recv(socket_fd, res, PIPE_BUFFER, 0)) > 0) {
        //     // Print received data
        //     printf("%s", res);
        // }
        // if (bytes_received < 0) {
        //     perror("recv failed");
        //     exit(EXIT_FAILURE);
        // }

        // memset(res, 0, sizeof(res)); // Clear buffer
        // Receive data from server
        // Receive data from server
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
            printf("%s", buffer);
        }



        
        
    }   

    close(socket_fd);   
    return 0;
}
