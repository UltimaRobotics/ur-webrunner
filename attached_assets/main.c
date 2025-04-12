#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <ctype.h>

#define DEFAULT_PORT 8080
#define BUFFER_SIZE 4096

// Function to URL decode a string
void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') && (a = src[1]) && (b = src[2]) && isxdigit(a) && isxdigit(b)) {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}

// Function to parse query parameters
void parse_query_params(const char *query, char *command, size_t cmd_len) {
    const char *ptr = query;
    char *temp = malloc(strlen(query) + 1);
    char *decoded = malloc(strlen(query) + 1);
    
    // Initialize output
    command[0] = '\0';
    
    while (*ptr) {
        if (strncmp(ptr, "command=", 8) == 0) {
            ptr += 8;
            char *end = strchr(ptr, '&');
            size_t len = end ? (size_t)(end - ptr) : strlen(ptr);
            strncpy(temp, ptr, len);
            temp[len] = '\0';
            url_decode(decoded, temp);
            strncpy(command, decoded, cmd_len - 1);
            command[cmd_len - 1] = '\0';
            break;
        }
        ptr++;
    }
    
    free(temp);
    free(decoded);
}

// Function to generate dynamic HTML content with terminal popup
void generate_dynamic_html(char *buffer, const char *client_ip, const char *command) {
    time_t now;
    time(&now);
    
    snprintf(buffer, BUFFER_SIZE,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <title>C Web Server with Terminal</title>\n"
        "    <style>\n"
        "        body { font-family: Arial, sans-serif; margin: 0; padding: 0; background: #f5f5f5; }\n"
        "        .header { background: #333; color: white; padding: 20px; }\n"
        "        .container { padding: 20px; }\n"
        "        .server-info { background: white; padding: 20px; border-radius: 5px; margin-bottom: 20px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }\n"
        "        \n"
        "        /* Terminal Popup Styles */\n"
        "        .terminal-btn { background: #4CAF50; color: white; border: none; padding: 10px 20px; border-radius: 4px; cursor: pointer; font-size: 16px; }\n"
        "        .terminal-btn:hover { background: #45a049; }\n"
        "        \n"
        "        .terminal-popup {\n"
        "            display: none;\n"
        "            position: fixed;\n"
        "            top: 50%;\n"
        "            left: 50%;\n"
        "            transform: translate(-50%, -50%);\n"
        "            width: 600px;\n"
        "            background: #1e1e1e;\n"
        "            border-radius: 8px;\n"
        "            box-shadow: 0 4px 8px rgba(0,0,0,0.3);\n"
        "            z-index: 1000;\n"
        "            overflow: hidden;\n"
        "        }\n"
        "        \n"
        "        .terminal-header {\n"
        "            background: #333;\n"
        "            color: white;\n"
        "            padding: 10px 15px;\n"
        "            display: flex;\n"
        "            justify-content: space-between;\n"
        "            align-items: center;\n"
        "        }\n"
        "        \n"
        "        .terminal-title { margin: 0; font-size: 14px; }\n"
        "        \n"
        "        .terminal-close {\n"
        "            background: #ff5f56;\n"
        "            width: 12px;\n"
        "            height: 12px;\n"
        "            border-radius: 50%;\n"
        "            cursor: pointer;\n"
        "        }\n"
        "        \n"
        "        .terminal-body {\n"
        "            padding: 15px;\n"
        "            color: #f0f0f0;\n"
        "            font-family: 'Courier New', monospace;\n"
        "            height: 300px;\n"
        "            overflow-y: auto;\n"
        "        }\n"
        "        \n"
        "        .terminal-prompt {\n"
        "            color: #4CAF50;\n"
        "        }\n"
        "        \n"
        "        .terminal-input {\n"
        "            display: flex;\n"
        "            padding: 0 15px 15px;\n"
        "        }\n"
        "        \n"
        "        .terminal-input input {\n"
        "            flex-grow: 1;\n"
        "            background: #2d2d2d;\n"
        "            border: 1px solid #444;\n"
        "            color: #f0f0f0;\n"
        "            padding: 8px;\n"
        "            font-family: 'Courier New', monospace;\n"
        "            border-radius: 4px;\n"
        "        }\n"
        "        \n"
        "        .terminal-input button {\n"
        "            background: #4CAF50;\n"
        "            color: white;\n"
        "            border: none;\n"
        "            padding: 8px 15px;\n"
        "            margin-left: 10px;\n"
        "            border-radius: 4px;\n"
        "            cursor: pointer;\n"
        "        }\n"
        "        \n"
        "        .overlay {\n"
        "            display: none;\n"
        "            position: fixed;\n"
        "            top: 0;\n"
        "            left: 0;\n"
        "            right: 0;\n"
        "            bottom: 0;\n"
        "            background: rgba(0,0,0,0.5);\n"
        "            z-index: 999;\n"
        "        }\n"
        "        \n"
        "        .command-output {\n"
        "            margin-top: 10px;\n"
        "            padding: 10px;\n"
        "            background: #2d2d2d;\n"
        "            border-radius: 4px;\n"
        "        }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <div class=\"header\">\n"
        "        <h1>C Web Server with Terminal Popup</h1>\n"
        "    </div>\n"
        "    \n"
        "    <div class=\"container\">\n"
        "        <div class=\"server-info\">\n"
        "            <h2>Server Information</h2>\n"
        "            <p><strong>Client IP:</strong> %s</p>\n"
        "            <p><strong>Server Time:</strong> %s</p>\n"
        "            <button class=\"terminal-btn\" onclick=\"openTerminal()\">Open Terminal</button>\n"
        "        </div>\n"
        "    </div>\n"
        "    \n"
        "    <!-- Terminal Popup -->\n"
        "    <div class=\"overlay\" id=\"overlay\"></div>\n"
        "    <div class=\"terminal-popup\" id=\"terminal\">\n"
        "        <div class=\"terminal-header\">\n"
        "            <h3 class=\"terminal-title\">Web Terminal</h3>\n"
        "            <div class=\"terminal-close\" onclick=\"closeTerminal()\"></div>\n"
        "        </div>\n"
        "        <div class=\"terminal-body\" id=\"terminal-body\">\n"
        "            <div class=\"terminal-prompt\">$ Welcome to the web terminal</div>\n",
        client_ip, ctime(&now));
    
    // Add command output if a command was submitted
    if (command && command[0]) {
        char *end = buffer + strlen(buffer);
        snprintf(end, BUFFER_SIZE - (end - buffer),
            "            <div class=\"terminal-prompt\">$ %s</div>\n"
            "            <div class=\"command-output\">\n"
            "                Command received by server: \"%s\"\n"
            "            </div>\n",
            command, command);
    }
    
    // Add the rest of the HTML
    strcat(buffer,
        "        </div>\n"
        "        <form method=\"GET\" action=\"/\" class=\"terminal-input\">\n"
        "            <input type=\"text\" name=\"command\" placeholder=\"Enter command...\" autocomplete=\"off\">\n"
        "            <button type=\"submit\">Execute</button>\n"
        "        </form>\n"
        "    </div>\n"
        "    \n"
        "    <script>\n"
        "        function openTerminal() {\n"
        "            document.getElementById('terminal').style.display = 'block';\n"
        "            document.getElementById('overlay').style.display = 'block';\n"
        "            document.querySelector('.terminal-input input').focus();\n"
        "        }\n"
        "        \n"
        "        function closeTerminal() {\n"
        "            document.getElementById('terminal').style.display = 'none';\n"
        "            document.getElementById('overlay').style.display = 'none';\n"
        "        }\n"
        "        \n"
        "        // Close terminal when clicking outside\n"
        "        document.getElementById('overlay').addEventListener('click', closeTerminal);\n"
        "        \n"
        "        // Prevent terminal from closing when clicking inside it\n"
        "        document.getElementById('terminal').addEventListener('click', function(e) {\n"
        "            e.stopPropagation();\n"
        "        });\n"
        "    </script>\n"
        "</body>\n"
        "</html>\n");
}

int main(int argc, char *argv[]) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    char client_ip[INET_ADDRSTRLEN];
    
    // Parse command line arguments for IP and port
    char *ip_address = "0.0.0.0"; // Default to all interfaces
    int port = DEFAULT_PORT;
    
    if (argc > 1) {
        ip_address = argv[1];
        if (argc > 2) {
            port = atoi(argv[2]);
        }
    }
    
    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip_address);
    address.sin_port = htons(port);
    
    // Bind the socket to the specified IP and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Start listening
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Server running on http://%s:%d\n", ip_address, port);
    printf("Press Ctrl+C to stop the server\n");
    
    while (1) {
        // Accept incoming connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        // Get client IP address
        inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
        
        // Read client request
        ssize_t bytes_read = read(new_socket, buffer, BUFFER_SIZE - 1);
        if (bytes_read < 0) {
            perror("read");
            close(new_socket);
            continue;
        }
        buffer[bytes_read] = '\0';
        
        // Parse command if this is a GET request with command parameter
        char command[1024] = {0};
        
        if (strncmp(buffer, "GET /?", 6) == 0) {
            char *query_start = buffer + 6;
            char *query_end = strchr(query_start, ' ');
            if (query_end) {
                *query_end = '\0';
                parse_query_params(query_start, command, sizeof(command));
            }
        }
        
        // Generate dynamic HTML with terminal popup
        generate_dynamic_html(buffer, client_ip, command[0] ? command : NULL);
        
        // Send the HTML response
        send(new_socket, buffer, strlen(buffer), 0);
        
        // Close the connection
        close(new_socket);
    }
    
    return 0;
}
