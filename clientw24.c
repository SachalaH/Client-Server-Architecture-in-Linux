// ASP Project W24
// Team members 
// Harsh Sachala - 110124409
// Rahul Patel - 110128309
// Including the necessary headers
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
#include <sys/ipc.h>
#include <sys/shm.h>

// defining the constants
#define MAXSIZE 1024
#define ARG_SIZE 5
#define MAXLEN 128
#define PIPE_BUFFER 4096
#define FILE_BUFF 1048576
#define SERVER_P 4409
#define SERVER_HOST "delta.cs.uwindsor.ca"
#define MIRROR1_P 4410
#define MIRROR1_HOST "delta.cs.uwindsor.ca"
#define MIRROR2_P 4411
#define MIRROR2_HOST "delta.cs.uwindsor.ca"
#define SHM_KEY 4409

// global shared variable to keep track of clients
int *client_count;

// function to initialize shared memory for communication
// between other processes
void init_shared_memory() {
    // variable to hold the shared memory identifier
    int shmid;
    // Try to access the shared memory segment without creating it.
    // Check if the shared memory segment with the specified key exists.
    if ((shmid = shmget(SHM_KEY, sizeof(int), 0666)) < 0) {
        perror("Client: shmget failed. Ensure the server, mirror1, or mirror2 is started first.");
        exit(1);
    }
    // Attach the shared memory segment to the client's address space.
    // This allows the client to access the shared memory segment.
    if ((client_count = shmat(shmid, NULL, 0)) == (int *)-1) {
        perror("Client: shmat failed");
        exit(1);
    }
}

// this function is used to check if the string date
// is in the format of yyyy-mm-dd
int is_valid_date_format(const char *date) {
    // Check if the length is correct
    if (strlen(date) != 10) // "yyyy-mm-dd" has 10 characters
        return 0;

    // Check if the separators are in the correct positions
    if (date[4] != '-' || date[7] != '-')
        return 0;

    // Check if year, month, and day are numeric
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7)
            continue; // Skip separators
        if (date[i] < '0' || date[i] > '9')
            return 0; // Not a digit
    }

    // Check if the values are within the valid ranges
    int year, month, day;
    if (sscanf(date, "%4d-%2d-%2d", &year, &month, &day) != 3)
        return 0;

    if (year < 0 || month < 1 || month > 12 || day < 1 || day > 31)
        return 0;

    // Valid date format if it reaches here after all the checks
    return 1; 
}

// this function is used to parse the client input string
int parse_ip(char *client_ip, char **args){
    // this function will parse the client ip
    // with respect to a space and return the count of the arguments
    // tokenize the input and split with " "
    char *token;
    token = strtok(client_ip, " ");
    // create an argument array until we reach \0 character
    int i = 0;
    // tokenize the input string
    while(token != NULL && i<ARG_SIZE){
        args[i] = token;
        token = strtok(NULL, " ");
        i++;
    }
    // null terminate the array
    args[i] = NULL;
    // i represents the total argument count
    return i;
}

// this function is used to validate the arguments thoroughly
// before sending it to the server
int validate_args(char **args, int arg_count){
    // this function consists of various checks related to the command specification
    // dirlist has 2 options -t and -a
    // hence arg count = 2
    if((strcmp(args[0],"dirlist")==0 && arg_count==2)&&(strcmp(args[1],"-t")==0 || strcmp(args[1],"-a")==0)){
        return 1;
    }
    // file display command has 2 arguments as well
    else if(strcmp(args[0],"w24fn")==0 && arg_count==2){
        return 1;
    }
    // file size command has 3 arguments
    // we first check if its 3 then convert each size to long int
    // then check if they are in the limit i.e. each >=0
    // and size 1 <= size 2
    else if(strcmp(args[0],"w24fz")==0 && arg_count==3){
        off_t size_1 = atoll(args[1]);
        off_t size_2 = atoll(args[2]);
        if(size_1 >= 0 && size_2 >= 0 && size_1 <= size_2){
            return 1;
        }else{
            return 0;
        }
    }
    // extension command has list of extensions
    // upto 3 hence argument count varies from 2 to 4
    else if(strcmp(args[0],"w24ft")==0 && arg_count>=2 && arg_count<=4){
        return 1;
    }
    // date comands are similar in respect to checking
    // first checking argument count
    // then individually checking date format
    else if(strcmp(args[0],"w24fda")==0 && arg_count==2 && is_valid_date_format(args[1])){
        return 1;
    }
    else if(strcmp(args[0],"w24fdb")==0 && arg_count==2 && is_valid_date_format(args[1])){
        return 1;
    }
    // normal check for quitc
    else if(strcmp(args[0],"quitc")==0 && arg_count==1){
        return 1;
    }
    else{
        return 0;
    }
}

// this function is used to send the parsed input to the server 
// after validation with the count as well
void send_parsed_input(int socket_fd, char **args, int count){
    int i = 0;
    int len;
    // sending individual string to the server 
    // count is just to make sure we dont send null value
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

// this function is used to create w24project directory
// to store the temp.tar.gz file in it
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
            // Directory exists then just exit
            exit(EXIT_SUCCESS);
        }else{
            // Child process
            // if no then create here
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

// function to receive the file from the server
void receive_file_from_server(int socket_fd, const char *output_file) {
    // first open the file whose path is passed in create or write only or overwrite mode
    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // Receive the file in chunks and write it to the output file
    char buffer[FILE_BUFF] = {0};
    ssize_t bytes_received, bytes_written;
    // receiving the file in bytes reading it in the static buffer
    bytes_received = read(socket_fd, buffer, sizeof(buffer));
    if (bytes_received == 0){
        // just return if done receiving the file
        return;
    } else if (bytes_received < 0) {
        // handle errors
        perror("read");
        exit(EXIT_FAILURE);
    } else {
        // else just point to the buffer
        void *p = buffer;
        // loop until we have reach the end of the bytes received
        while (bytes_received > 0) {
            // write each byte into the opened file
            int bytes_written = write(fd, p, bytes_received);
            if (bytes_written <= 0) {
                // handle errors
            }
            // subtract the bytes that were written
            bytes_received -= bytes_written;
            // advance the buffer pointer
            p += bytes_written;
        }
    }

    // Close the file
    close(fd);

}

// function to connect to the server
int connect_to_server(const char *hostname, int port){

    // Structure to hold server information
    struct sockaddr_in server_info;
    // Structure to hold host information
    struct hostent *he;
    // Socket file descriptor
    int socket_fd;
    // Resolve the hostname to an IP address
    if ((he = gethostbyname(hostname))==NULL) {
        fprintf(stderr, "Cannot get host name\n");
        exit(1);
    }
    // create the socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0))== -1) {
        fprintf(stderr, "Socket Failure!!\n");
        exit(1);
    }
    // Initialize server_info structure with server details
    memset(&server_info, 0, sizeof(server_info));
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(port);
    server_info.sin_addr = *((struct in_addr *)he->h_addr);
    // connect to the server
    if (connect(socket_fd, (struct sockaddr *)&server_info, sizeof(struct sockaddr))<0) {
        //fprintf(stderr, "Connection Failure\n");
        perror("connect");
        exit(1);
    }

    // return the socket file desc
    return socket_fd;

}

// count of files received
int file_count = 1;

// this is where the client starts 
int main(int argc, char *argv[]) {

    int socket_fd;
    char buffer[1024];
    // initializing the shared memory
    init_shared_memory();

    // Increment the global client count.
    (*client_count)++;
    // basic check for first 3
    if (*client_count <= 3 || (*client_count > 9 && *client_count % 3 == 1)) {
        // Try connecting to the server.
        socket_fd = connect_to_server(SERVER_HOST, SERVER_P);
        if (socket_fd != -1) {
            printf("Connected to server %s:%d\n", SERVER_HOST, SERVER_P);
        }
    } 
    // check for next 3 i.e. 4 to 6
    else if (*client_count <= 6 || (*client_count > 9 && *client_count % 3 == 2)){
        socket_fd = connect_to_server(MIRROR1_HOST, MIRROR1_P);
        if (socket_fd != -1) {
            printf("Connected to mirror 1 %s:%d\n", MIRROR1_HOST, MIRROR1_P);
        }

    }
    // else its 7 to 9 
    else {
        socket_fd = connect_to_server(MIRROR2_HOST, MIRROR2_P);
        if (socket_fd != -1) {
            printf("Connected to mirror 2 %s:%d\n", MIRROR2_HOST, MIRROR2_P);
        }
    }

    // main loop starts
    while(1) {
        // flushing the std output
        fflush(stdout);
        // flushing the buffer
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
        // creating the command array
        char *args[ARG_SIZE];
        // parse the input that we get
        int arg_count = parse_ip(buffer, args);
        // validate the input
        int validate_res = validate_args(args, arg_count);

        // if validation is done correctly we shall send it to the server
        if(validate_res){

            // Send the number of arguments to the server
            if (send(socket_fd, &arg_count, sizeof(int), 0) < 0) {
                perror("send failed");
                return -1;
            }


            // call function to send the parsed input to server
            // send the command args to the server
            // Send each argument to the server
            for (int i = 0; i < arg_count; i++) {
                if (send(socket_fd, args[i], MAXLEN * sizeof(char), 0) < 0) {
                    perror("send failed");
                    return -1;
                }
            }

            // buffer to recieve from server
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
                // check if we are getting the file
                if(strcmp(buffer, "temp.tar.gz")==0)
                {   
                    // check the file count
                    // high chances that dir does not exists
                    if(file_count == 1){
                        // create the dir
                        char *dir_path = getenv("HOME");
                        char storage_dir[MAXLEN];
                        // Copy the home path to the complete path buffer
                        strncpy(storage_dir, dir_path, MAXLEN);
                        // Check if the home path ends with a slash "/"
                        if (storage_dir[strlen(storage_dir) - 1] != '/') {
                            // If not, append a slash
                            strcat(storage_dir, "/");
                        }

                        // Append foldername to the complete path
                        strcat(storage_dir, "w24project");
                        // create the directory
                        create_file_dir(storage_dir);
                    }
                    // use dynamic naming for file
                    char *home = getenv("HOME");
                    char filename[MAXLEN];

                    // Format the file name string with the count variable
                    sprintf(filename, "%s/w24project/temp-%d.tar.gz", home, file_count);
                    // call the function to recieve the file from the server
                    receive_file_from_server(socket_fd,filename);
                    file_count++;
                    printf("File received successfully.\n");

                }
                else
                {   
                    // just print the message from the server
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
    // close the socket
    close(socket_fd);   
    return 0;
}
