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
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
// Defining the constants
#define PORT 4409
#define BACKLOG 10
#define ARG_SIZE 5
#define PATH_LEN 1024
#define MAXSIZE 128
#define PIPE_BUFFER 4096
#define MAX_FILES 20
#define MAX_EXT 3
#define FILE_BUFF 1048576
#define SHM_KEY 4409

// initializing the shared global variable of client count
int *client_count;

// function prototypes
void init_shared_memory();
void crequest(int client_fd);
void sigchild_handler(int signo);
void handle_incoming_strings(char *args[], int client_fd, int *num_args);
void list_dirs_newfirst(int client_fd);
void list_dirs_alphabetically(int client_fd);
void search_file_info(char *filename, char *path, int *found, char *details);
void search_files_with_size(char *path, off_t min_size, off_t max_size, char *file_paths[], int *num_files);
void search_files_with_extensions(char *path, char *extensions[], int num_extensions, char *file_paths[], int *num_files);
void search_files_with_date(const char *path, time_t target_date, char *file_paths[], int *count, const char flag);
void create_tar_file(const char *output_file, char *file_paths[], int *num_files);
void compress_tar_file(const char *tar_file);
void send_file_to_client(int client_socket);

// main function starts here
int main()
{
    // first initializing the share memory for client count
    init_shared_memory();

    // registering a signal for sig child sent by children
    // so that processes that exits while running in background gets acknowledged
    signal(SIGCHLD, sigchild_handler);
    struct sockaddr_in server;
    struct sockaddr_in dest;
    int status,socket_fd, client_fd,num;
    socklen_t size;

    char *buff;
    int yes =1;
    // creating the socket
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

    // binding the socket to the port
    if ((bind(socket_fd, (struct sockaddr *)&server, sizeof(struct sockaddr )))== -1)    { //sizeof(struct sockaddr) 
        fprintf(stderr, "Binding Failure\n");
        exit(1);
    }

    // listening for the incoming requests
    if ((listen(socket_fd, BACKLOG))== -1){
        fprintf(stderr, "Listening Failure\n");
        exit(1);
    }

    printf("Server listening on port: %d\n", PORT);

    size = sizeof(struct sockaddr_in);  
    while(1) {

        if ((client_fd = accept(socket_fd, (struct sockaddr *)&dest, &size))==-1) {
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
            crequest(client_fd); // Handle client request in this function
            close(client_fd);
            exit(0);
        } else {
            // Parent process
            close(client_fd); // Close client socket in parent process
        }
        
    }
    return 0;
}

// function to initialize the shared memory
void init_shared_memory(){
    int shmid;
    // Create a new shared memory segment or access an existing one with the specified key
    if ((shmid = shmget(SHM_KEY, sizeof(int), IPC_CREAT | 0666)) < 0)
    {
        perror("shmget");
        exit(1);
    }

    // Attach the shared memory segment to the client's address space
    if ((client_count = shmat(shmid, NULL, 0)) == (int *)-1)
    {
        perror("shmat");
        exit(1);
    }
}

// function to serve the client
void crequest(int client_fd){
    
    // here we shall actually handle the client
    char *args[ARG_SIZE];
    int read_size;
    int num_args;
    // enters an infinite loop to handle the commands
    while(1){
        // flushing the std output
        fflush(stdout);
        // reseting the arguments array
        memset(args,0,sizeof(args));
        // calling function to parse the incoming list of strings from client 
        handle_incoming_strings(args, client_fd, &num_args);

        // handling each command        
        if(strcmp(args[0],"dirlist")==0){
            // checking if the argument is -a or -t
            // and calling the respective functions 
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
            // getting the file name 
            // declaring the root directory
            char *filename = args[1];
            char *root = getenv("HOME");
            // flag to check if file is found or not
            int found = 0;
            // buffer to add file details to
            char details[PIPE_BUFFER];
            // calling the function to search for the file
            search_file_info(filename, root, &found, details);
            // If the file was not found, send an error message
            if (!found)
            {
                char *msg = "File not found.\n";
                int bytes_sent = send(client_fd, msg, strlen(msg), 0);
                if (bytes_sent == -1){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
            }
            // else send its required details 
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
            // getting the root path
            char *dir_path = getenv("HOME");
            // converting the size 
            off_t size_1 = atoll(args[1]);
            off_t size_2 = atoll(args[2]);
            
            // Create an array to store file paths
            char *file_paths[MAX_FILES];
            int num_files = 0;

            // Search for files within the specified size range
            search_files_with_size(dir_path, size_1, size_2, file_paths, &num_files);

            if(num_files){
                // create a tar file path
                char tar_file_path[PATH_LEN];
                // Copy the home path to the complete path buffer
                strncpy(tar_file_path, dir_path, PATH_LEN);
                // Check if the home path ends with a slash "/"
                if (tar_file_path[strlen(tar_file_path) - 1] != '/') {
                    // If not, append a slash
                    strcat(tar_file_path, "/");
                }

                // Append "/temp.tar" to the complete path
                strcat(tar_file_path, "temp.tar");
                // call function to create the tar file
                create_tar_file(tar_file_path, file_paths, &num_files);
                // and zip the tar file
                compress_tar_file(tar_file_path);
                // send it to the client 
                send_file_to_client(client_fd);
                // remove the zip file created as it is sent to the client
                // first create the zip file path name
                char gz_file_path[PATH_LEN];
                // Copy the home path to the complete path buffer
                strncpy(gz_file_path, dir_path, PATH_LEN);
                // Check if the home path ends with a slash "/"
                if (gz_file_path[strlen(gz_file_path) - 1] != '/') {
                    // If not, append a slash
                    strcat(gz_file_path, "/");
                }

                // Append "/temp.tar" to the complete path
                strcat(gz_file_path, "temp.tar.gz");
                // delete the created file to remove it from the server side
                unlink(gz_file_path);
            }else{
                // send the message no files found if number of files is 0
                char *msg = "No file found.\n";
                int bytes_sent = send(client_fd, msg, strlen(msg), 0);
                if (bytes_sent == -1){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
            }


            
        }
        else if(strcmp(args[0],"w24ft")==0){
            // creating the root directory
            char *dir_path = getenv("HOME");
            // initial count of the extension is 1 i.e. min 1
            int ext_count = 1;
            // creating list of extensions
            char *extensions[MAX_EXT];
            // add the extensions received
            while(args[ext_count] != NULL){
                extensions[ext_count-1] = args[ext_count];
                ext_count++;
            }
            // Create an array to store file paths
            char *file_paths[MAX_FILES];
            int num_files = 0;
            // call the function to find respective files 
            search_files_with_extensions(dir_path, extensions, ext_count-1, file_paths, &num_files);
            if(num_files){
                // create a tar file
                char tar_file_path[PATH_LEN];
                // Copy the home path to the complete path buffer
                strncpy(tar_file_path, dir_path, PATH_LEN);
                // Check if the home path ends with a slash "/"
                if (tar_file_path[strlen(tar_file_path) - 1] != '/') {
                    // If not, append a slash
                    strcat(tar_file_path, "/");
                }

                // Append "/temp.tar" to the complete path
                strcat(tar_file_path, "temp.tar");
                // create the tar file
                create_tar_file(tar_file_path, file_paths, &num_files);
                // and zip it 
                compress_tar_file(tar_file_path);
                
                // send it to the client
                send_file_to_client(client_fd);
                // remove the zip file created as it is sent to the client
                char gz_file_path[PATH_LEN];
                // Copy the home path to the complete path buffer
                strncpy(gz_file_path, dir_path, PATH_LEN);
                // Check if the home path ends with a slash "/"
                if (gz_file_path[strlen(gz_file_path) - 1] != '/') {
                    // If not, append a slash
                    strcat(gz_file_path, "/");
                }

                // Append "/temp.tar" to the complete path
                strcat(gz_file_path, "temp.tar.gz");
                // delete the file from the server
                unlink(gz_file_path);
            }else{
                // send the message no files found if count is 0
                char *msg = "No file found.\n";
                int bytes_sent = send(client_fd, msg, strlen(msg), 0);
                if (bytes_sent == -1){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
            }


            
        }
        else if(strcmp(args[0],"w24fda")==0){
            // getting the root dir
            char *dir_path = getenv("HOME");
            const char *string_date = args[1];
            // flag is a meaning after 
            const char flag = 'a';
            // Create a struct tm variable to store the parsed date
            struct tm tm_date = {0};
            // parsing the string date to proper date for comparision
            // Parse the string date into the struct tm
            if (strptime(string_date, "%Y-%m-%d", &tm_date) == NULL) {
                fprintf(stderr, "Error parsing time string\n");
                exit(EXIT_FAILURE);
            }

            // Convert the struct tm to time_t
            time_t converted_date = mktime(&tm_date);

            // Create an array to store file paths
            char *file_paths[MAX_FILES];
            int num_files = 0;
            // calling the function to search the files
            search_files_with_date(dir_path, converted_date, file_paths, &num_files, flag);

            if(num_files){
                // create a tar file
                char tar_file_path[PATH_LEN];
                // Copy the home path to the complete path buffer
                strncpy(tar_file_path, dir_path, PATH_LEN);
                // Check if the home path ends with a slash "/"
                if (tar_file_path[strlen(tar_file_path) - 1] != '/') {
                    // If not, append a slash
                    strcat(tar_file_path, "/");
                }

                // Append "/temp.tar" to the complete path
                strcat(tar_file_path, "temp.tar");
                // create the tar file
                create_tar_file(tar_file_path, file_paths, &num_files);
                // and zip it 
                compress_tar_file(tar_file_path);
                // send it
                send_file_to_client(client_fd);
                // remove the zip file created as it is sent to the client
                char gz_file_path[PATH_LEN];
                // Copy the home path to the complete path buffer
                strncpy(gz_file_path, dir_path, PATH_LEN);
                // Check if the home path ends with a slash "/"
                if (gz_file_path[strlen(gz_file_path) - 1] != '/') {
                    // If not, append a slash
                    strcat(gz_file_path, "/");
                }

                // Append "/temp.tar" to the complete path
                strcat(gz_file_path, "temp.tar.gz");
                // delete it 
                unlink(gz_file_path);
            }else{
                // send the message no files found 
                char *msg = "No file found.\n";
                int bytes_sent = send(client_fd, msg, strlen(msg), 0);
                if (bytes_sent == -1){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
            }


            
        }
        else if(strcmp(args[0],"w24fdb")==0){
            char *dir_path = getenv("HOME");
            const char *string_date = args[1];
            const char flag = 'b';
            // Create a struct tm variable to store the parsed date
            struct tm tm_date = {0};

            // Parse the string date into the struct tm
            if (strptime(string_date, "%Y-%m-%d", &tm_date) == NULL) {
                fprintf(stderr, "Error parsing time string\n");
                exit(EXIT_FAILURE);
            }

            // Convert the struct tm to time_t
            time_t converted_date = mktime(&tm_date);
            
            // Create an array to store file paths
            char *file_paths[MAX_FILES];
            int num_files = 0;

            search_files_with_date(dir_path, converted_date, file_paths, &num_files, flag);

            if(num_files){
                // create a tar file
                char tar_file_path[PATH_LEN];
                // Copy the home path to the complete path buffer
                strncpy(tar_file_path, dir_path, PATH_LEN);
                // Check if the home path ends with a slash "/"
                if (tar_file_path[strlen(tar_file_path) - 1] != '/') {
                    // If not, append a slash
                    strcat(tar_file_path, "/");
                }

                // Append "/temp.tar" to the complete path
                strcat(tar_file_path, "temp.tar");
                create_tar_file(tar_file_path, file_paths, &num_files);
                compress_tar_file(tar_file_path);
                // send it
                send_file_to_client(client_fd);
                // remove the zip file created as it is sent to the client
                char gz_file_path[PATH_LEN];
                // Copy the home path to the complete path buffer
                strncpy(gz_file_path, dir_path, PATH_LEN);
                // Check if the home path ends with a slash "/"
                if (gz_file_path[strlen(gz_file_path) - 1] != '/') {
                    // If not, append a slash
                    strcat(gz_file_path, "/");
                }

                // Append "/temp.tar" to the complete path
                strcat(gz_file_path, "temp.tar.gz");
                unlink(gz_file_path);
            }else{
                // send the message no files found 
                char *msg = "No file found.\n";
                int bytes_sent = send(client_fd, msg, strlen(msg), 0);
                if (bytes_sent == -1){
                    perror("send");
                    exit(EXIT_FAILURE);
                }
            }

            
        }
        else if (strcmp(args[0],"quitc")==0) {
            // just break the loop it will close
            printf("Connection closed\n");
            break;
        }
        
    }

    close(client_fd);
    exit(0);
    
}

// sigchild handler to handle defunct clients
void sigchild_handler(int signo){
    int status;
    waitpid(-1, &status, WNOHANG);
}

// function to handle the incoming strings from client and form an array
void handle_incoming_strings(char *args[], int client_fd, int *num_args){

    // Receive a message from client
    // Receive the number of arguments sent by the client
    if (recv(client_fd, num_args, sizeof(int), 0) < 0) {
        perror("recv failed");
        exit(EXIT_FAILURE);
    }

    // Receive each argument sent by the client
    // put it in the array
    for (int i = 0; i < *num_args; i++) {
        args[i] = malloc(MAXSIZE * sizeof(char));
        if (recv(client_fd, args[i], MAXSIZE * sizeof(char), 0) < 0) {
            perror("recv failed");
            exit(EXIT_FAILURE);
        }
    }


}

// function to list the home directories in modified latest time
void list_dirs_newfirst(int client_fd){
    // creating the ls command
    // since special characters are not recognised by the execvp
    // invoking a shell command to execute the ls command
    char *ls_command = "sh";
    char *ls_arguments[] = {"sh", "-c", "ls -1dt ~/\*/", NULL};

    char buffer[PIPE_BUFFER] = {0};
    // creating the pipe 
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
        // as we just want to write to it
        // redirect the output to the pipe
        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        close(pipe_fd[1]); // Close the original write end of the pipe as output will be written

        // Execute ls command using the 
        if (execvp(ls_command, ls_arguments) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else { // Parent process
        close(pipe_fd[1]); // Close the write end of the pipe as we just want to read
        ssize_t bytes_read;
        while ((bytes_read = read(pipe_fd[0], buffer, PIPE_BUFFER)) > 0) {
            if (send(client_fd, buffer, bytes_read, 0) != bytes_read) {
                perror("send");
                exit(EXIT_FAILURE);
            }
        }
        close(pipe_fd[0]); // Close the read end of the pipe after sending
        memset(buffer, 0, sizeof(buffer)); // Clear buffer
    }


}

// function to send the list of dirs in the home directory 
// sorted alphabetically using the shell command
// similar functionality using the pipe as above
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

// this function searches for the file recursively
// if found then creates the output buffer with details and breaks the recursion
// and if not then just continues the search
// handling the not found using the found flag
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

// function to search for files
// within the size limits recursively
void search_files_with_size(char *path, off_t min_size, off_t max_size, char *file_paths[], int *num_files){
    DIR *dir;
    struct dirent *dp;
    struct stat st;
    char buf[PATH_LEN];

    // Open the directory at the specified path
    dir = opendir(path);
    if (dir == NULL)
    {
        perror("opendir");
        return;
    }

    // Traverse the directory
    while ((dp = readdir(dir)) != NULL)
    {
        // Check if the current directory entry is a hidden directory
        if (dp->d_name[0] == '.')
        {
            continue; // Skip hidden directories
        }

        // Construct the full path to the current entry
        sprintf(buf, "%s/%s", path, dp->d_name);

        // Get the file stats
        if (stat(buf, &st) == 0)
        {
            // Check if the current entry is a regular file
            if (S_ISREG(st.st_mode))
            {
                // Check if the file size is within the specified range and files are in limit
                if (st.st_size >= min_size && st.st_size <= max_size && *num_files < MAX_FILES)
                {
                    // Add the file path to the array
                    file_paths[*num_files] = strdup(buf);
                    (*num_files)++;
                }
            }
            else if (S_ISDIR(st.st_mode))
            {
                // Recursively search subdirectories
                search_files_with_size(buf, min_size, max_size, file_paths, num_files);
            }
        }
    }

    // Close the directory
    closedir(dir);
}

//function to search for files with specified extensions
// list of extensions is passed 
// each file is checked for each extension that is passed 
void search_files_with_extensions(char *path, char *extensions[], int num_extensions, char *file_paths[], int *num_files){
    DIR *dir;
    struct dirent *dp;
    struct stat st;
    char buf[PATH_LEN];

    // Open the directory at the specified path
    dir = opendir(path);
    if (dir == NULL)
    {
        perror("opendir");
        return;
    }

    // Traverse the directory
    while ((dp = readdir(dir)) != NULL)
    {
        // Check if the current directory entry is a hidden directory
        if (dp->d_name[0] == '.')
        {
            continue; // Skip hidden directories
        }

        // Construct the full path to the current entry
        sprintf(buf, "%s/%s", path, dp->d_name);

        // Get the file stats
        if (stat(buf, &st) == 0)
        {
            // Check if the current entry is a regular file
            if (S_ISREG(st.st_mode))
            {
                // Check if the file has one of the specified extensions
                char *file_extension = strrchr(dp->d_name, '.');
                if (file_extension != NULL && *num_files < MAX_FILES)
                {
                    for (int i = 0; i < num_extensions; i++)
                    {
                        if (strcmp(file_extension + 1, extensions[i]) == 0)
                        {
                            // Add the file path to the array
                            file_paths[*num_files] = strdup(buf);
                            (*num_files)++;
                            break;
                        }
                    }
                }
            }
            else if (S_ISDIR(st.st_mode))
            {
                // Recursively search subdirectories
                search_files_with_extensions(buf, extensions, num_extensions, file_paths, num_files);
            }
        }
    }

    // Close the directory
    closedir(dir);
}

// function to search for files before and after date
// before after is specified with the help of the flag
// common function for after and before 
// recursively finds the files and creates the list of files before or after the date
void search_files_with_date(const char *path, time_t target_date, char *file_paths[], int *count, const char flag){
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char full_path[PATH_LEN];

    if (!(dir = opendir(path))) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    
    while ((entry = readdir(dir)) != NULL) {
        // Skip hidden directories
        if (entry->d_name[0] == '.')
            continue;

        snprintf(full_path, PATH_LEN, "%s/%s", path, entry->d_name);

        if (stat(full_path, &statbuf) == -1) {
            perror("stat");
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            
            search_files_with_date(full_path, target_date, file_paths, count, flag);
        } else {
            if(flag == 'a'){
                time_t creation_time = statbuf.st_atime;
                // Calculate the difference in seconds between the two dates
                double difference = difftime(creation_time, target_date);
                // printf("%ld\n",creation_time);
                if (difference > 0 && *count < MAX_FILES) {
                    file_paths[*count] = strdup(full_path);
                    (*count)++;
                }
            }else{

                time_t creation_time = statbuf.st_atime;
                // Calculate the difference in seconds between the two dates
                double difference = difftime(creation_time, target_date);
                // printf("%ld\n",creation_time);
                if (difference < 0 && *count < MAX_FILES) {
                    file_paths[*count] = strdup(full_path);
                    (*count)++;
                }
            }
        }
    }
    closedir(dir);
}

// function to create the tar file using execvp
// first forks and creates the tar argument array
// then executes the same while the parent waits
void create_tar_file(const char *output_file, char *file_paths[], int *num_files){
    pid_t pid;
    int status;

    // Fork a child process
    if ((pid = fork()) < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        // Build the command arguments
        char *args[*num_files + 4];
        args[0] = "tar";
        args[1] = "-cf";
        args[2] = (char *)output_file;
        for (int i = 0; i < *num_files; i++) {
            args[i + 3] = file_paths[i];
        }
        args[*num_files + 3] = NULL;

        // Execute tar command
        execvp("tar", args);

        // execvp only returns if an error occurs
        perror("execvp");
        exit(EXIT_FAILURE);
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

// function to zip the tar file 
// using execvp and parent waits
void compress_tar_file(const char *tar_file){
    pid_t pid;
    int status;

    // Fork a child process
    if ((pid = fork()) < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process

        // Execute gzip command
        execlp("gzip", "gzip", "-f", tar_file, NULL);

        // execlp only returns if an error occurs
        perror("execlp");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        // Wait for the child process to complete
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Error: gzip command failed\n");
            exit(EXIT_FAILURE);
        }
    }
}

// function to send the file to the client
// opens the file reads it and writes to the socket of client
// client opens the file in the requried dir at client side and reads from the socket
void send_file_to_client(int client_fd){

    char gz_file_path[PATH_LEN];
    char *dir_path = getenv("HOME");
    // Copy the home path to the complete path buffer
    strncpy(gz_file_path, dir_path, PATH_LEN);
    // Check if the home path ends with a slash "/"
    if (gz_file_path[strlen(gz_file_path) - 1] != '/') {
        // If not, append a slash
        strcat(gz_file_path, "/");
    }

    // Append "/temp.tar" to the complete path
    strcat(gz_file_path, "temp.tar.gz");

    int fd = open(gz_file_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    char *msg = "temp.tar.gz";
    int bytes_sent = send(client_fd, msg, strlen(msg), 0);
    if (bytes_sent == -1){
        perror("send");
        exit(EXIT_FAILURE);
    }

    // Read the file in chunks and send it to the client
    char buffer[FILE_BUFF] = {0};
    ssize_t bytes_read;
    int flag = 1;

    // while(flag){
        bytes_read = read(fd, buffer, sizeof(buffer));

        if (bytes_read == 0){
            // We're done reading from the file
            return;
        } else if (bytes_read < 0) {
            // handle errors
            perror("read");
            exit(EXIT_FAILURE);
        } else {

            void *p = buffer;
            while (bytes_read > 0) {
                int bytes_written = write(client_fd, p, bytes_read);
                if (bytes_written <= 0) {
                    // handle errors
                }
                bytes_read -= bytes_written;
                p += bytes_written;
            }

        }

    // }

    // Close the file
    close(fd);
}