#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SERVER_PORT "8080"
#define MAX_BUFFER 1024
#define MAX_TOKENS 100

typedef struct {
    char** tokens;
    int count;
} StringList;

//Splits a string into an array of tokens
StringList splitString(const char* str, const char* delim)
{
    StringList result;
    result.tokens = (char**)malloc(MAX_TOKENS * sizeof(char*));
    if (result.tokens == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    result.count = 0;

    // Make a copy of the input string to avoid modifying the original string
    char* str_copy = strdup(str);
    if (str_copy == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    char* saveptr;
    char* token = strtok_r(str_copy, delim, &saveptr);

    while (token != NULL && result.count < MAX_TOKENS)
    {
        result.tokens[result.count++] = strdup(token);
        token = strtok_r(NULL, delim, &saveptr);
    }

    // Free the copied string as it's no longer needed
    free(str_copy);

    return result;
}

void freeStringList(StringList* result)
{
    for (int i = 0; i < result->count; i++)
    {
        free(result->tokens[i]);
    }
    free(result->tokens);
}

//Reads all data from a file into a buffer
char* readFile(char* filename)
{
    FILE* fptr = fopen(filename, "r");
    if (fptr == NULL)
    {
        return NULL;
    }

    //Get the size of the file
    fseek(fptr, 0, SEEK_END);
    long fileSize = ftell(fptr);
    fseek(fptr, 0, SEEK_SET);

    //Read our fileData properly so that it is null terminated and ensure we read the whole file
    char* fileData = (char*)malloc((fileSize + 1) * sizeof(char));
    size_t bytesRead = fread(fileData, sizeof(char), fileSize, fptr);

    if (bytesRead != fileSize) {
        return NULL;
    }
    fileData[fileSize] = '\0';

    //Close file and return
    fclose(fptr);
    return fileData;
}

void freeFile(char* fileData)
{
    free(fileData);
}

// Check if address is IPV4 or IPV6
void* getInAddr(struct sockaddr* sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char const* argv[])
{
    int addrStatus = 0;
    int serverSocket = 0;
    int clientSocket = 0;
    struct sockaddr_storage inboundAddr;
    struct addrinfo aInfo;
    struct addrinfo* results;

    memset(&aInfo, 0, sizeof(aInfo));
    aInfo.ai_family = AF_UNSPEC;
    aInfo.ai_socktype = SOCK_STREAM;
    aInfo.ai_flags = AI_PASSIVE;

    addrStatus = getaddrinfo(NULL, SERVER_PORT, &aInfo, &results);
    if (addrStatus != 0)
    {
        fprintf(stderr, "Failed to get address information\n");
        exit(1);
    }

    // Struct to hold results from addrinfo and go through the linked list without overwriting results
    struct addrinfo* i;
    // Look for free socket to use
    for (i = results; i != NULL; i = i->ai_next)
    {
        serverSocket = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
        if (serverSocket == -1)
        {
            continue;
        }

        // Setup the socket to reuse local address
        int yes = 1;
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            fprintf(stderr, "Failed to set socket reuse addr option\n");
            exit(1);
        }

        //Set a timeout for the socket
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        if (setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
        {
            fprintf(stderr, "Failed to set socket timeout option\n");
            exit(1);
        }

        // Bind socket
        if (bind(serverSocket, results->ai_addr, results->ai_addrlen) == -1)
        {
            shutdown(serverSocket, 0);
            fprintf(stderr, "Failed to bind server socket\n");
            continue;
        }
        break;
    }

    //Free up the addrinfo that is now no longer needed
    freeaddrinfo(results);

    if (i == NULL)
    {
        fprintf(stderr, "Failed to create server socket\n");
        exit(1);
    }

    // Socket for the server to bind to
    if (listen(serverSocket, 10) == -1)
    {
        fprintf(stderr, "Failed to listen on server socket\n");
    }

    printf("Waiting for connection\n");

    while (1)
    {
        // Accept the connection
        socklen_t addr_size = sizeof(inboundAddr);
        clientSocket = accept(serverSocket, (struct sockaddr*)&inboundAddr, &addr_size);

        if (clientSocket == -1)
        {
            fprintf(stderr, "Failed to accept connection\n");
            //exit(1);
            continue;
        }

        // Show who connected
        char addrString[INET6_ADDRSTRLEN];

        inet_ntop(inboundAddr.ss_family, getInAddr((struct sockaddr*)&inboundAddr), addrString, sizeof(addrString));

        printf("Connection Successful with %s\n", addrString);

        int messageSize = 0;
        char buffer[MAX_BUFFER];

        //Receive the http1.1 request
        messageSize = recv(clientSocket, buffer, MAX_BUFFER - 1, 0);
        if (messageSize == -1)
        {
            fprintf(stderr, "Did not receive anything\n");
            continue;
        }
        buffer[messageSize] = '\0';
        printf("%s\n", buffer);
        //Look through headers to determine the type of request
        StringList headers = splitString(buffer, "\n");
        if(headers.count < 1) {
            break;
        }
        StringList request = splitString(headers.tokens[0], " ");
        if(request.count < 1) {
            break;
        }
        printf("Request = %s\n", request.tokens[0]);

        //Handle GET request
        if (strcmp(request.tokens[0], "GET") == 0)
        {
            printf("File to open = %s\n", request.tokens[1]);

            //strip the "/" off the file name and open the file.
            char* fileData;
            if (strcmp(request.tokens[1], "/") == 0)
            {
                char fileName[] = "200.html";
                fileData = readFile(fileName);
            }
            else
            {
                char* fileName = (char*)malloc((strlen(request.tokens[1]) + 1) * sizeof(char));
                strcpy(fileName, request.tokens[1] + 1);
                printf("File to open = %s\n", fileName);
                fileData = readFile(fileName);
                free(fileName);
            }

            //If the file exists send it to the client otherwise 404
            if (fileData != NULL)
            {
                int fileLength = strlen(fileData);
                printf("Found file\n");
                char header[] = "HTTP/1.1 200 OK\n\n";
                char* response = (char*)malloc(strlen(header) + 1 + fileLength);
                strcpy(response, "\0");
                strcat(response, header);
                strcat(response, fileData);

                int sendCode = send(clientSocket, response, strlen(response), 0);
                if (sendCode == -1)
                {
                    free(response);
                    fprintf(stderr, "Failed to send message\n");
                    exit(1);
                }
                free(response);
                freeFile(fileData);
            }
            else
            {
                //Return 404 when file is not found
                char header[MAX_BUFFER] = "HTTP/1.1 404 Not Found\n\n";
                printf("404: File not found\n");
                int sendCode = send(clientSocket, &header, MAX_BUFFER - 1, 0);
                if (sendCode == -1)
                {
                    fprintf(stderr, "Failed to send message\n");
                    exit(1);
                }
            }
        }

        //Free up or string lists
        freeStringList(&headers);
        freeStringList(&request);

        // Exit gracefully
        int fCode = shutdown(clientSocket, 2);
        printf("Done: %d\n", fCode);
        //break;
    }

    int closeCode = shutdown(serverSocket, 2);
    return 0;
}
