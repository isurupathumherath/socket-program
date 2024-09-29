#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <time.h>
#include <dirent.h>

#define BUFSIZE 1024
#define SERVER_IP "127.0.0.1"  // Server's IP address
#define SERVER_PORT 2138       // Port number
#define DIRECTORY_PATH "./sharedfolder/"  // Directory containing files
#define MAX_FILENAME_LENGTH 512  // Maximum length for filenames

char *fileNames[100];  // Array to store file names
int fileCount = 0;     // Number of files in the directory

// Function to log client details and file request to a log file
void log_client(char *clientIP, int clientPort, char *filename) {
    FILE *logFile = fopen("log_srv9788.txt", "a");
    time_t now = time(0);
    char *dt = ctime(&now);
    fprintf(logFile, "Client IP: %s, Port: %d, File: %s, Date: %s\n", clientIP, clientPort, filename, dt);
    fclose(logFile);
}

// Function to list files in the directory and store them in fileNames array
void list_files() {
    struct dirent *de;
    DIR *dr = opendir(DIRECTORY_PATH);
    if (dr == NULL) {
        perror("Could not open directory");
        exit(1);
    }

    fileCount = 0;
    while ((de = readdir(dr)) != NULL) {
        if (de->d_type == DT_REG) {  // Regular files only
            fileNames[fileCount] = malloc(MAX_FILENAME_LENGTH);
            snprintf(fileNames[fileCount], MAX_FILENAME_LENGTH, "%s", de->d_name);
            fileCount++;
        }
    }
    closedir(dr);
}

// Function to send the list of files to the client
void send_file_list(int clntSock) {
    char fileList[BUFSIZE * 2] = {0};
    for (int i = 0; i < fileCount; i++) {
        char temp[MAX_FILENAME_LENGTH];
        snprintf(temp, sizeof(temp), "%d. %s\n", i + 1, fileNames[i]);  // Numbered file list
        strcat(fileList, temp);
    }
    send(clntSock, fileList, strlen(fileList), 0);  // Send the file list to the client
}

// Function to handle the client request
void handle_client(int clntSock, struct sockaddr_in *clientAddr) {
    char buffer[BUFSIZE];
    ssize_t bytesRead;
    int fileIndex;

    // Log the client connection
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr->sin_addr), clientIP, INET_ADDRSTRLEN);
    int clientPort = ntohs(clientAddr->sin_port);
    printf("Client connection from IP %s, Port:%d\n", clientIP, clientPort);

    // Send the list of files to the client
    send_file_list(clntSock);

    // Receive the file number request from the client
    bytesRead = recv(clntSock, buffer, sizeof(buffer), 0);
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        fileIndex = atoi(buffer) - 1;  // Convert the received number to an index
        if (fileIndex < 0 || fileIndex >= fileCount) {
            perror("Invalid file selection");
            close(clntSock);
            return;
        }
        printf("Client selected file: %s\n", fileNames[fileIndex]);
    } else {
        perror("recv() failed");
        close(clntSock);
        return;
    }

    // Step 1: Send the file name
    send(clntSock, fileNames[fileIndex], strlen(fileNames[fileIndex]), 0);

    // Step 2: Wait for acknowledgment from the client before sending the content
    recv(clntSock, buffer, sizeof(buffer), 0);

    // Step 3: Open and send the requested file content
    char filePath[MAX_FILENAME_LENGTH + BUFSIZE] = {0};
    snprintf(filePath, sizeof(filePath), "%s%s", DIRECTORY_PATH, fileNames[fileIndex]);
    int fileDescriptor = open(filePath, O_RDONLY);
    if (fileDescriptor < 0) {
        perror("File open failed");
        close(clntSock);
        return;
    }

    while ((bytesRead = read(fileDescriptor, buffer, sizeof(buffer))) > 0) {
        if (send(clntSock, buffer, bytesRead, 0) != bytesRead) {
            perror("send() failed");
            exit(1);
        }
    }

    close(fileDescriptor);
    close(clntSock);

    // Log the transaction to the log file
    log_client(clientIP, clientPort, fileNames[fileIndex]);
}

int main() {
    int servSock, clntSock;
    struct sockaddr_in servAddr, clientAddr;
    socklen_t clntLen = sizeof(clientAddr);

    servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (servSock < 0) {
        perror("socket() failed");
        exit(1);
    }

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(SERVER_IP);  // Bind to 127.0.0.1
    servAddr.sin_port = htons(SERVER_PORT);           // Use port 2138

    if (bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
        perror("bind() failed");
        close(servSock);
        exit(1);
    }

    if (listen(servSock, 100) < 0) {
        perror("listen() failed");
        close(servSock);
        exit(1);
    }

    // List the files at startup
    list_files();

    // Print server start message
    printf("Server started on IP: %s, Port: %d\n", SERVER_IP, SERVER_PORT);
    printf("Waiting for client connections...\n");

    for (;;) {
        clntSock = accept(servSock, (struct sockaddr *)&clientAddr, &clntLen);
        if (clntSock < 0) {
            perror("accept() failed");
            continue;
        }

        if (fork() == 0) {
            close(servSock);
            handle_client(clntSock, &clientAddr);
            exit(0);
        }
        close(clntSock);
    }

    return 0;
}

