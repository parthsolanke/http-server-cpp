#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

void handle_client(int client_fd) {
    std::string request(1024, '\0');
    int bytes_received = recv(client_fd, &request[0], request.size(), 0);
    if (bytes_received < 0) {
        std::cerr << "recv failed\n";
        close(client_fd);
        return;
    }
    request.resize(bytes_received);

    std::cerr << "Client Request: " << request << std::endl;

    std::string response;
    if (request.find("GET / HTTP/1.1\r\n") == 0) {
        response = "HTTP/1.1 200 OK\r\n\r\n";
    } else if (request.find("GET /echo/") == 0) {
        size_t start_pos = 10; // length of "GET /echo/"
        size_t end_pos = request.find(' ', start_pos);
        std::string echo_str = request.substr(start_pos, end_pos - start_pos);

        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "Content-Length: " + std::to_string(echo_str.length()) + "\r\n";
        response += "\r\n";
        response += echo_str;
    } else if (request.find("GET /user-agent HTTP/1.1\r\n") == 0) {
        size_t user_agent_pos = request.find("User-Agent: ");
        if (user_agent_pos != std::string::npos) {
            size_t user_agent_end = request.find("\r\n", user_agent_pos);
            std::string user_agent = request.substr(user_agent_pos + 12, user_agent_end - user_agent_pos - 12);

            response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: text/plain\r\n";
            response += "Content-Length: " + std::to_string(user_agent.length()) + "\r\n";
            response += "\r\n";
            response += user_agent;
        } else {
            response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        }
    } else {
        response = "HTTP/1.1 404 Not Found\r\n\r\n";
    }

    int bytes_sent = send(client_fd, response.c_str(), response.size(), 0);
    if (bytes_sent < 0) {
        std::cerr << "send failed\n";
    } else {
        std::cout << "Response sent\n";
    }

    close(client_fd);
}

int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(4221);

    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        std::cerr << "Failed to bind to port 4221\n";
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        std::cerr << "listen failed\n";
        return 1;
    }

    std::cout << "Waiting for clients to connect...\n";

    while (true) {
        struct sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
        if (client_fd < 0) {
            std::cerr << "accept failed\n";
            continue;
        }

        std::cout << "Client connected\n";

        pid_t pid = fork();
        if (pid == 0) { // Child process
            close(server_fd); // Close the listening socket in the child process
            handle_client(client_fd);
            exit(0); // Exit the child process
        } else if (pid > 0) { // Parent process
            close(client_fd); // Close the connected socket in the parent process
        } else {
            std::cerr << "fork failed\n";
            close(client_fd);
        }
    }

    close(server_fd);
    return 0;
}
