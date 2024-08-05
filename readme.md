# Basic HTTP Server in C

This basic HTTP server can handle `GET`, `POST`, and `HEAD` requests. It serves files from the local directory and responds with appropriate HTTP status codes. The server listens on port `8080` and supports both IPv4 and IPv6 addresses.

## Features

- Handles `GET`, `POST`, and `HEAD` requests.
- Serves static files from the local directory.
- Returns `404 Not Found` for missing files.
- Returns `400 Bad Request` for unsupported request methods.
- Supports IPv4 and IPv6.

## Code Overview

- **main.c**: The main source file containing the server implementation.
  - **splitString**: Splits a string into an array of tokens.
  - **freeStringList**: Frees the memory allocated for a StringList.
  - **readFile**: Reads the contents of a file into a buffer.
  - **freeFile**: Frees the memory allocated for a file buffer.
  - **getInAddr**: Determines if an address is IPv4 or IPv6.
  - **handleGetRequest**: Processes GET requests and sends the requested file as a response.
  - **handleHttpRequest**: Parses HTTP headers and handles the request.
  - **main**: Initializes the server, listens for incoming connections, and handles requests.

## Usage

1. Compile the server:
    ```sh
    gcc -o http_server main.c
    ```

2. Run the server:
    ```sh
    ./http_server
    ```

3. Open your web browser and navigate to `http://localhost:8080`.
