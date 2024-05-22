#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

// Splits a string into an array of tokens
StringList splitString(const char* str, const char* delim) {
    StringList result;
    result.tokens = (char**)malloc(MAX_TOKENS * sizeof(char*));
    if (result.tokens == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    result.count = 0;

    // Make a copy of the input string to avoid modifying the original string
    char* str_copy = strdup(str);
    if (str_copy == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    char* saveptr;
    char* token = strtok_r(str_copy, delim, &saveptr);

    while (token != NULL && result.count < MAX_TOKENS) {
        result.tokens[result.count++] = strdup(token);
        token = strtok_r(NULL, delim, &saveptr);
    }

    free(str_copy);

    return result;
}

void freeStringList(StringList* result) {
    for (int i = 0; i < result->count; i++) {
        free(result->tokens[i]);
    }
    free(result->tokens);
}

// Reads all data from a file into a buffer
char* readFile(const char* filename) {
    FILE* fptr = fopen(filename, "r");
    if (fptr == NULL) {
        return NULL;
    }

    fseek(fptr, 0, SEEK_END);
    long fileSize = ftell(fptr);
    fseek(fptr, 0, SEEK_SET);

    char* fileData = (char*)malloc((fileSize + 1) * sizeof(char));
    if (fileData == NULL) {
        fclose(fptr);
        return NULL;
    }

    size_t bytesRead = fread(fileData, sizeof(char), fileSize, fptr);
    if (bytesRead != fileSize) {
        free(fileData);
        fclose(fptr);
        return NULL;
    }
    fileData[fileSize] = '\0';

    fclose(fptr);
    return fileData;
}

void freeFile(char* fileData) {
    free(fileData);
}

// Check if address is IPV4 or IPV6
void* getInAddr(struct sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char const* argv[]) {
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
    if (addrStatus != 0) {
        fprintf(stderr, "Failed to get address information\n");
        exit(EXIT_FAILURE);
    }

    struct addrinfo* i;
    for (i = results; i != NULL; i = i->ai_next) {
        serverSocket = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
        if (serverSocket == -1) {
            continue;
        }

        int yes = 1;
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            fprintf(stderr, "Failed to set socket reuse addr option\n");
            exit(EXIT_FAILURE);
        }

        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        if (setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
            fprintf(stderr, "Failed to set socket timeout option\n");
            exit(EXIT_FAILURE);
        }

        if (bind(serverSocket, i->ai_addr, i->ai_addrlen) == -1) {
            close(serverSocket);
            fprintf(stderr, "Failed to bind server socket\n");
            continue;
        }
        break;
    }

    freeaddrinfo(results);

    if (i == NULL) {
        fprintf(stderr, "Failed to create server socket\n");
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket, 10) == -1) {
        fprintf(stderr, "Failed to listen on server socket\n");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for connection\n");

    while (1) {
        socklen_t addr_size = sizeof(inboundAddr);
        clientSocket = accept(serverSocket, (struct sockaddr*)&inboundAddr, &addr_size);

        if (clientSocket == -1) {
            fprintf(stderr, "Failed to accept connection\n");
            continue;
        }

        char addrString[INET6_ADDRSTRLEN];
        inet_ntop(inboundAddr.ss_family, getInAddr((struct sockaddr*)&inboundAddr), addrString, sizeof(addrString));
        printf("Connection Successful with %s\n", addrString);

        char buffer[MAX_BUFFER];
        int messageSize = recv(clientSocket, buffer, MAX_BUFFER - 1, 0);
        if (messageSize == -1) {
            fprintf(stderr, "Did not receive anything\n");
            continue;
        }
        buffer[messageSize] = '\0';
        printf("%s\n", buffer);

        StringList headers = splitString(buffer, "\n");
        if (headers.count < 1) {
            freeStringList(&headers);
            continue;
        }
        StringList request = splitString(headers.tokens[0], " ");
        if (request.count < 1) {
            freeStringList(&headers);
            freeStringList(&request);
            continue;
        }
        printf("Request = %s\n", request.tokens[0]);

        if (!((strcmp(request.tokens[0], "GET") == 0) || (strcmp(request.tokens[0], "POST") == 0))) {
            //[400] Bad Request
            printf("[400] Bad Request\n");
            char header[] = "HTTP/1.1 400 Bad Request\n\n";
            char* fileData = readFile("400.html");
            if (fileData != NULL) {
                size_t responseSize = strlen(header) + strlen(fileData) + 1;
                char* response = (char*)malloc(responseSize);
                if (response == NULL) {
                    fprintf(stderr, "Memory allocation failed\n");
                    free(fileData);
                    freeStringList(&headers);
                    freeStringList(&request);
                    close(clientSocket);
                    continue;
                }
                snprintf(response, responseSize, "%s%s", header, fileData);

                if (send(clientSocket, response, strlen(response), 0) == -1) {
                    fprintf(stderr, "Failed to send message\n");
                }
                free(response);
                freeFile(fileData);
            }
        }
        else {
            printf("File to open = %s\n", request.tokens[1]);

            char* fileData;
            if (strcmp(request.tokens[1], "/") == 0) {
                fileData = readFile("200.html");
            }
            else {
                char* fileName = strdup(request.tokens[1] + 1);
                if (fileName == NULL) {
                    fprintf(stderr, "Memory allocation failed\n");
                    freeStringList(&headers);
                    freeStringList(&request);
                    close(clientSocket);
                    continue;
                }
                fileData = readFile(fileName);
                free(fileName);
            }

            if (fileData != NULL) {
                char header[] = "HTTP/1.1 200 OK\n\n";
                size_t responseSize = strlen(header) + strlen(fileData) + 1;
                char* response = (char*)malloc(responseSize);
                if (response == NULL) {
                    fprintf(stderr, "Memory allocation failed\n");
                    free(fileData);
                    freeStringList(&headers);
                    freeStringList(&request);
                    close(clientSocket);
                    continue;
                }
                snprintf(response, responseSize, "%s%s", header, fileData);

                if (send(clientSocket, response, strlen(response), 0) == -1) {
                    fprintf(stderr, "Failed to send message\n");
                }
                free(response);
                freeFile(fileData);
            }
            else {
                char header[] = "HTTP/1.1 404 Not Found\n\n";
                if (send(clientSocket, header, strlen(header), 0) == -1) {
                    fprintf(stderr, "Failed to send message\n");
                }
            }
        }

        freeStringList(&headers);
        freeStringList(&request);

        if (shutdown(clientSocket, SHUT_RDWR) == -1) {
            fprintf(stderr, "Failed to close client socket\n");
        }
    }

    if (shutdown(serverSocket, SHUT_RDWR) == -1) {
        fprintf(stderr, "Failed to close server socket\n");
    }
    return 0;
}
