#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <ctype.h>

#define DEFAULT_PORT 5000
#define BUFFER_SIZE 65536  // Larger buffer for complex HTML
#define MAX_COMMAND_SIZE 2048
#define MAX_HISTORY 10

// Global command history
char command_history[MAX_HISTORY][MAX_COMMAND_SIZE];
int history_count = 0;

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

// Add a command to history
void add_to_history(const char *command) {
    // Move all commands down to make room for the new one
    if (history_count == MAX_HISTORY) {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(command_history[i], command_history[i + 1]);
        }
        history_count--;
    }
    
    // Add the new command to history
    strcpy(command_history[history_count], command);
    history_count++;
}

// Function to execute a command and capture output
char* execute_command(const char *command, int *exit_status) {
    char cmd[MAX_COMMAND_SIZE + 100];
    sprintf(cmd, "(%s) 2>&1", command); // Redirect stderr to stdout
    
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        *exit_status = -1;
        return strdup("Error executing command");
    }
    
    // Allocate memory for output buffer, with room to grow
    size_t output_size = 4096;
    char *output = malloc(output_size);
    if (!output) {
        pclose(fp);
        *exit_status = -1;
        return strdup("Memory allocation error");
    }
    output[0] = '\0';
    
    // Read command output
    char buffer[4096];
    size_t total_read = 0;
    
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        size_t buffer_len = strlen(buffer);
        
        // Check if we need to grow the output buffer
        if (total_read + buffer_len + 1 > output_size) {
            output_size *= 2;
            char *new_output = realloc(output, output_size);
            if (!new_output) {
                free(output);
                pclose(fp);
                *exit_status = -1;
                return strdup("Memory allocation error during output capture");
            }
            output = new_output;
        }
        
        // Append this chunk to output
        strcat(output, buffer);
        total_read += buffer_len;
    }
    
    // Get command exit status
    int status = pclose(fp);
    *exit_status = WEXITSTATUS(status);
    
    return output;
}

// Get system information for OpenWRT
char* get_system_info() {
    static char info[4096];
    
    sprintf(info, "<h3>System Information</h3>");
    
    // Add placeholder system info for now
    strcat(info, "<p>This is a placeholder for OpenWRT system information</p>");
    strcat(info, "<p>In a real implementation, this would show CPU, memory, and other hardware details.</p>");
    
    return info;
}

// Get network information for OpenWRT
char* get_network_info() {
    static char info[4096];
    
    sprintf(info, "<h3>Network Information</h3>");
    
    // Add placeholder network info for now
    strcat(info, "<p>This is a placeholder for OpenWRT network information</p>");
    strcat(info, "<p>In a real implementation, this would show interfaces, IP addresses, etc.</p>");
    
    return info;
}

// Function to generate dynamic HTML content with terminal popup
void generate_dynamic_html(char *buffer, const char *client_ip, const char *command, const char *cmd_output, int exit_status) {
    time_t now;
    time(&now);
    
    char *system_info = get_system_info();
    char *network_info = get_network_info();
    
    // Start with HTTP headers and basic HTML
    int offset = sprintf(buffer, 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <title>OpenWRT Management Interface</title>\n"
        "    <style>\n"
        "        body { font-family: Arial, sans-serif; margin: 0; padding: 0; background: #f5f5f5; }\n"
        "        .header { background: #0066cc; color: white; padding: 15px 20px; display: flex; justify-content: space-between; align-items: center; }\n"
        "        .header h1 { margin: 0; }\n"
        "        .container { display: flex; min-height: calc(100vh - 60px); }\n"
        "        .sidebar { width: 250px; background: white; border-right: 1px solid #ddd; }\n"
        "        .sidebar ul { list-style-type: none; padding: 0; margin: 0; }\n"
        "        .sidebar li { padding: 10px 20px; border-bottom: 1px solid #ddd; cursor: pointer; }\n"
        "        .sidebar li:hover { background: #f8f9fa; }\n"
        "        .sidebar li.active { background: #0066cc; color: white; }\n"
        "        .content { flex: 1; padding: 20px; }\n"
        "        .card { background: white; border-radius: 4px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }\n"
        "        .card h2 { margin-top: 0; color: #0066cc; border-bottom: 1px solid #ddd; padding-bottom: 10px; }\n"
        "        .terminal-btn { background: #0066cc; color: white; border: none; padding: 10px 20px; border-radius: 4px; cursor: pointer; }\n"
        "        .tab-content { display: none; }\n"
        "        .tab-content.active { display: block; }\n"
        "        .terminal-popup { display: none; position: fixed; top: 50%%; left: 50%%; transform: translate(-50%%, -50%%); width: 80%%; height: 80%%; background: #1e1e1e; border-radius: 8px; box-shadow: 0 4px 20px rgba(0,0,0,0.3); overflow: hidden; z-index: 1000; }\n"
        "        .terminal-header { background: #333; color: white; padding: 10px 15px; display: flex; justify-content: space-between; align-items: center; }\n"
        "        .terminal-controls { display: flex; gap: 5px; }\n"
        "        .terminal-control { width: 12px; height: 12px; border-radius: 50%%; cursor: pointer; }\n"
        "        .terminal-minimize { background: #ffbd4c; }\n"
        "        .terminal-maximize { background: #00ca56; }\n"
        "        .terminal-close { background: #ff5f56; }\n"
        "        .terminal-body { padding: 15px; color: #f0f0f0; font-family: monospace; height: calc(100%% - 120px); overflow-y: auto; }\n"
        "        .terminal-prompt { color: #4CAF50; white-space: pre-wrap; margin-bottom: 5px; }\n"
        "        .terminal-form { display: flex; padding: 10px 15px; background: #2d2d2d; border-top: 1px solid #444; }\n"
        "        .terminal-form input { flex-grow: 1; background: #2d2d2d; border: none; color: #f0f0f0; padding: 8px; font-family: monospace; }\n"
        "        .overlay { display: none; position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.5); z-index: 999; }\n"
        "        .command-output { margin-top: 5px; margin-bottom: 15px; white-space: pre-wrap; }\n"
        "        .command-success { color: #4CAF50; }\n"
        "        .command-error { color: #f44336; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <div class=\"header\">\n"
        "        <h1>OpenWRT Management Interface</h1>\n"
        "        <button class=\"terminal-btn\" onclick=\"openTerminal()\">Terminal</button>\n"
        "    </div>\n"
        "    \n"
        "    <div class=\"container\">\n"
        "        <div class=\"sidebar\">\n"
        "            <ul>\n"
        "                <li class=\"active\" onclick=\"showTab('dashboard')\">Dashboard</li>\n"
        "                <li onclick=\"showTab('system')\">System</li>\n"
        "                <li onclick=\"showTab('network')\">Network</li>\n"
        "                <li onclick=\"showTab('services')\">Services</li>\n"
        "                <li onclick=\"showTab('firewall')\">Firewall</li>\n"
        "                <li onclick=\"openTerminal()\">Terminal</li>\n"
        "            </ul>\n"
        "        </div>\n"
        "        \n"
        "        <div class=\"content\">\n"
        "            <div id=\"dashboard\" class=\"tab-content active\">\n"
        "                <div class=\"card\">\n"
        "                    <h2>Dashboard</h2>\n"
        "                    <p>Welcome to the OpenWRT Management Interface.</p>\n"
        "                    <p><strong>Client IP:</strong> %s</p>\n"
        "                    <p><strong>Server Time:</strong> %s</p>\n"
        "                    <button class=\"terminal-btn\" onclick=\"openTerminal()\">Open Terminal</button>\n"
        "                </div>\n"
        "                \n"
        "                <div class=\"card\">\n"
        "                    <h2>System Status</h2>\n"
        "                    %s\n"
        "                </div>\n"
        "                \n"
        "                <div class=\"card\">\n"
        "                    <h2>Network Status</h2>\n"
        "                    %s\n"
        "                </div>\n"
        "            </div>\n"
        "            \n"
        "            <div id=\"system\" class=\"tab-content\">\n"
        "                <div class=\"card\">\n"
        "                    <h2>System Information</h2>\n"
        "                    <p>System information and management would be shown here.</p>\n"
        "                    %s\n"
        "                </div>\n"
        "            </div>\n"
        "            \n"
        "            <div id=\"network\" class=\"tab-content\">\n"
        "                <div class=\"card\">\n"
        "                    <h2>Network Configuration</h2>\n"
        "                    <p>Network configuration would be shown here.</p>\n"
        "                    %s\n"
        "                </div>\n"
        "            </div>\n"
        "            \n"
        "            <div id=\"services\" class=\"tab-content\">\n"
        "                <div class=\"card\">\n"
        "                    <h2>Services Management</h2>\n"
        "                    <p>Services management would be shown here.</p>\n"
        "                </div>\n"
        "            </div>\n"
        "            \n"
        "            <div id=\"firewall\" class=\"tab-content\">\n"
        "                <div class=\"card\">\n"
        "                    <h2>Firewall Settings</h2>\n"
        "                    <p>Firewall settings would be shown here.</p>\n"
        "                </div>\n"
        "            </div>\n"
        "        </div>\n"
        "    </div>\n"
        "    \n"
        "    <!-- Terminal Popup -->\n"
        "    <div class=\"overlay\" id=\"overlay\"></div>\n"
        "    <div class=\"terminal-popup\" id=\"terminal\">\n"
        "        <div class=\"terminal-header\">\n"
        "            <h3>OpenWRT Terminal</h3>\n"
        "            <div class=\"terminal-controls\">\n"
        "                <div class=\"terminal-control terminal-minimize\" onclick=\"minimizeTerminal()\"></div>\n"
        "                <div class=\"terminal-control terminal-maximize\" onclick=\"maximizeTerminal()\"></div>\n"
        "                <div class=\"terminal-control terminal-close\" onclick=\"closeTerminal()\"></div>\n"
        "            </div>\n"
        "        </div>\n"
        "        <div class=\"terminal-body\" id=\"terminal-body\">\n"
        "            <div class=\"terminal-prompt\">Welcome to OpenWRT Terminal</div>\n"
        "            <div class=\"terminal-prompt\">Type 'help' for a list of common commands</div>\n",
        client_ip, ctime(&now), system_info, network_info, system_info, network_info);
    
    // Add command history if any
    if (history_count > 0) {
        for (int i = 0; i < history_count; i++) {
            offset += sprintf(buffer + offset,
                "            <div class=\"terminal-prompt\">$ %s</div>\n",
                command_history[i]);
            
            // Add the current command output if this is the most recent command
            if (i == history_count - 1 && command && strcmp(command, command_history[i]) == 0 && cmd_output) {
                offset += sprintf(buffer + offset,
                    "            <div class=\"command-output %s\">%s</div>\n",
                    exit_status == 0 ? "command-success" : "command-error",
                    cmd_output);
            }
        }
    }
    
    // Add the command output if command is not in history but was just executed
    if (command && command[0] && cmd_output && 
        (history_count == 0 || strcmp(command, command_history[history_count-1]) != 0)) {
        offset += sprintf(buffer + offset,
            "            <div class=\"terminal-prompt\">$ %s</div>\n"
            "            <div class=\"command-output %s\">%s</div>\n",
            command, 
            exit_status == 0 ? "command-success" : "command-error",
            cmd_output);
    }
    
    // Add the rest of the HTML
    sprintf(buffer + offset,
        "        </div>\n"
        "        <form method=\"GET\" action=\"/\" class=\"terminal-form\">\n"
        "            <input type=\"text\" name=\"command\" id=\"commandInput\" placeholder=\"Enter command...\" autocomplete=\"off\">\n"
        "            <button type=\"submit\">Execute</button>\n"
        "        </form>\n"
        "    </div>\n"
        "    \n"
        "    <script>\n"
        "        // Show initial tab\n"
        "        function showTab(tabId) {\n"
        "            // Hide all tab contents\n"
        "            var tabContents = document.getElementsByClassName('tab-content');\n"
        "            for (var i = 0; i < tabContents.length; i++) {\n"
        "                tabContents[i].classList.remove('active');\n"
        "            }\n"
        "            \n"
        "            // Show selected tab content\n"
        "            document.getElementById(tabId).classList.add('active');\n"
        "            \n"
        "            // Update active menu item\n"
        "            var menuItems = document.querySelectorAll('.sidebar li');\n"
        "            for (var i = 0; i < menuItems.length; i++) {\n"
        "                menuItems[i].classList.remove('active');\n"
        "                if (menuItems[i].innerText.toLowerCase() === tabId.toLowerCase()) {\n"
        "                    menuItems[i].classList.add('active');\n"
        "                }\n"
        "            }\n"
        "        }\n"
        "        \n"
        "        // Terminal functions\n"
        "        function openTerminal() {\n"
        "            document.getElementById('terminal').style.display = 'block';\n"
        "            document.getElementById('overlay').style.display = 'block';\n"
        "            document.getElementById('commandInput').focus();\n"
        "            var terminalBody = document.getElementById('terminal-body');\n"
        "            terminalBody.scrollTop = terminalBody.scrollHeight;\n"
        "        }\n"
        "        \n"
        "        function closeTerminal() {\n"
        "            document.getElementById('terminal').style.display = 'none';\n"
        "            document.getElementById('overlay').style.display = 'none';\n"
        "        }\n"
        "        \n"
        "        function minimizeTerminal() {\n"
        "            closeTerminal();\n"
        "        }\n"
        "        \n"
        "        function maximizeTerminal() {\n"
        "            var terminal = document.getElementById('terminal');\n"
        "            if (terminal.style.width === '95%%' && terminal.style.height === '95%%') {\n"
        "                terminal.style.width = '80%%';\n"
        "                terminal.style.height = '80%%';\n"
        "            } else {\n"
        "                terminal.style.width = '95%%';\n"
        "                terminal.style.height = '95%%';\n"
        "            }\n"
        "        }\n"
        "        \n"
        "        // Show terminal if there's a command in the URL\n"
        "        window.onload = function() {\n"
        "            var urlParams = new URLSearchParams(window.location.search);\n"
        "            if (urlParams.has('command')) {\n"
        "                openTerminal();\n"
        "            }\n"
        "        };\n"
        "        \n"
        "        // Close terminal when clicking overlay\n"
        "        document.getElementById('overlay').addEventListener('click', closeTerminal);\n"
        "        \n"
        "        // Prevent terminal from closing when clicking inside\n"
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
    
    printf("OpenWRT Management Interface running on http://%s:%d\n", 
           strcmp(ip_address, "0.0.0.0") == 0 ? "*" : ip_address, port);
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
        char command[MAX_COMMAND_SIZE] = {0};
        char *cmd_output = NULL;
        int exit_status = 0;
        
        if (strncmp(buffer, "GET /?", 6) == 0) {
            char *query_start = buffer + 6;
            char *query_end = strchr(query_start, ' ');
            if (query_end) {
                *query_end = '\0';
                parse_query_params(query_start, command, sizeof(command));
                *query_end = ' ';
            }
        }
        
        // If a command was submitted, execute it
        if (command[0]) {
            // Special handling for 'help' command
            if (strcmp(command, "help") == 0) {
                cmd_output = strdup(
                    "Common OpenWRT Commands:\n\n"
                    "System Information:\n"
                    "  cat /etc/openwrt_release    - Show OpenWRT version\n"
                    "  uname -a                    - Show kernel information\n"
                    "  uptime                      - Show system uptime\n"
                    "  top                         - Show running processes\n"
                    "  free                        - Show memory usage\n"
                    "  df -h                       - Show disk usage\n\n"
                    
                    "Network Commands:\n"
                    "  ifconfig                    - Show network interfaces\n"
                    "  iwconfig                    - Show wireless interfaces\n"
                    "  route -n                    - Show routing table\n"
                    "  ip addr                     - Show IP addresses\n"
                    "  cat /etc/config/network     - Show network configuration\n"
                    "  cat /etc/config/wireless    - Show wireless configuration\n"
                    "  ping [host]                 - Test network connectivity\n\n"
                    
                    "Service Management:\n"
                    "  /etc/init.d/[service] [start|stop|restart|status]\n"
                    "  Examples: /etc/init.d/network restart, /etc/init.d/firewall status\n\n"
                    
                    "Firewall:\n"
                    "  iptables -L -n              - List firewall rules\n"
                    "  cat /etc/config/firewall    - Show firewall configuration\n\n"
                    
                    "Advanced:\n"
                    "  logread                     - Show system logs\n"
                    "  ps                          - List running processes\n"
                );
                exit_status = 0;
            } else {
                // Execute the command and capture output
                cmd_output = execute_command(command, &exit_status);
                
                // Add command to history
                add_to_history(command);
            }
        }
        
        // Generate dynamic HTML with terminal popup
        generate_dynamic_html(buffer, client_ip, command[0] ? command : NULL, cmd_output, exit_status);
        
        // Free command output memory
        if (cmd_output) {
            free(cmd_output);
            cmd_output = NULL;
        }
        
        // Send the HTML response
        send(new_socket, buffer, strlen(buffer), 0);
        
        // Close the connection
        close(new_socket);
    }
    
    return 0;
}