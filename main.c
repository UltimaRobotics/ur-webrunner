#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#define DEFAULT_PORT 8080
#define BUFFER_SIZE 8192
#define MAX_COMMAND_SIZE 1024
#define MAX_COMMAND_OUTPUT 4096
#define MAX_HISTORY 20

// Store command history
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

// Execute a command and capture its output
char* execute_command(const char *command, int *exit_status) {
    char *output = malloc(MAX_COMMAND_OUTPUT);
    if (!output) {
        perror("Memory allocation failed");
        return NULL;
    }
    
    // Sanitize the command - prevent certain dangerous operations
    if (strstr(command, "rm -rf") || strstr(command, "mkfs") || 
        strstr(command, "> /dev/") || strstr(command, ":(){:|:&};:")) {
        snprintf(output, MAX_COMMAND_OUTPUT, 
                 "ERROR: Potentially dangerous command blocked for security reasons.\n");
        *exit_status = 1;
        return output;
    }
    
    // Create pipes for capturing command output
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        free(output);
        return NULL;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        free(output);
        return NULL;
    }
    
    if (pid == 0) {  // Child process
        close(pipefd[0]);  // Close reading end
        
        // Redirect stdout and stderr to pipe
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        
        // Execute the command
        execlp("/bin/sh", "sh", "-c", command, NULL);
        
        // If we get here, execlp failed
        perror("execlp");
        exit(1);
    } else {  // Parent process
        close(pipefd[1]);  // Close writing end
        
        // Read output from the child process
        int total_read = 0;
        int bytes_read;
        
        while ((bytes_read = read(pipefd[0], output + total_read, 
                                  MAX_COMMAND_OUTPUT - total_read - 1)) > 0) {
            total_read += bytes_read;
            if (total_read >= MAX_COMMAND_OUTPUT - 1) {
                break;
            }
        }
        
        output[total_read] = '\0';
        close(pipefd[0]);
        
        // Wait for the child process to complete
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            *exit_status = WEXITSTATUS(status);
        } else {
            *exit_status = -1;
        }
    }
    
    return output;
}

// Add command to history
void add_to_history(const char *command) {
    if (history_count >= MAX_HISTORY) {
        // Shift all commands one position back
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(command_history[i], command_history[i + 1]);
        }
        history_count = MAX_HISTORY - 1;
    }
    
    // Add new command at the end
    strncpy(command_history[history_count], command, MAX_COMMAND_SIZE - 1);
    command_history[history_count][MAX_COMMAND_SIZE - 1] = '\0';
    history_count++;
}

// Get system information for OpenWRT
char* get_system_info() {
    static char info[4096];
    int exit_status;
    char *output;
    
    sprintf(info, "<h3>System Information</h3>");
    
    // Get OpenWRT version
    output = execute_command("cat /etc/openwrt_release 2>/dev/null || echo 'OpenWRT version information not available'", &exit_status);
    if (output && exit_status == 0) {
        strcat(info, "<div class='info-section'><strong>OpenWRT Version:</strong><pre>");
        strcat(info, output);
        strcat(info, "</pre></div>");
    }
    free(output);
    
    // Get kernel version
    output = execute_command("uname -a", &exit_status);
    if (output && exit_status == 0) {
        strcat(info, "<div class='info-section'><strong>Kernel:</strong><pre>");
        strcat(info, output);
        strcat(info, "</pre></div>");
    }
    free(output);
    
    // Get system uptime
    output = execute_command("uptime", &exit_status);
    if (output && exit_status == 0) {
        strcat(info, "<div class='info-section'><strong>Uptime:</strong><pre>");
        strcat(info, output);
        strcat(info, "</pre></div>");
    }
    free(output);
    
    // Get memory usage
    output = execute_command("free -h", &exit_status);
    if (output && exit_status == 0) {
        strcat(info, "<div class='info-section'><strong>Memory Usage:</strong><pre>");
        strcat(info, output);
        strcat(info, "</pre></div>");
    }
    free(output);
    
    // Get disk space
    output = execute_command("df -h", &exit_status);
    if (output && exit_status == 0) {
        strcat(info, "<div class='info-section'><strong>Disk Space:</strong><pre>");
        strcat(info, output);
        strcat(info, "</pre></div>");
    }
    free(output);
    
    return info;
}

// Get network information for OpenWRT
char* get_network_info() {
    static char info[4096];
    int exit_status;
    char *output;
    
    sprintf(info, "<h3>Network Status</h3>");
    
    // Get interface information
    output = execute_command("ifconfig", &exit_status);
    if (output && exit_status == 0) {
        strcat(info, "<div class='info-section'><strong>Network Interfaces:</strong><pre>");
        strcat(info, output);
        strcat(info, "</pre></div>");
    }
    free(output);
    
    // Get wireless information
    output = execute_command("iwconfig 2>/dev/null || echo 'Wireless information not available'", &exit_status);
    if (output) {
        strcat(info, "<div class='info-section'><strong>Wireless Status:</strong><pre>");
        strcat(info, output);
        strcat(info, "</pre></div>");
    }
    free(output);
    
    // Get routing table
    output = execute_command("route -n", &exit_status);
    if (output && exit_status == 0) {
        strcat(info, "<div class='info-section'><strong>Routing Table:</strong><pre>");
        strcat(info, output);
        strcat(info, "</pre></div>");
    }
    free(output);
    
    // Get DNS information
    output = execute_command("cat /etc/resolv.conf", &exit_status);
    if (output && exit_status == 0) {
        strcat(info, "<div class='info-section'><strong>DNS Configuration:</strong><pre>");
        strcat(info, output);
        strcat(info, "</pre></div>");
    }
    free(output);
    
    return info;
}

// Function to generate dynamic HTML content with terminal popup
void generate_dynamic_html(char *buffer, const char *client_ip, const char *command, const char *cmd_output, int exit_status) {
    time_t now;
    time(&now);
    
    char *system_info = get_system_info();
    char *network_info = get_network_info();
    
    snprintf(buffer, BUFFER_SIZE,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <title>OpenWRT Management Interface</title>\n"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "    <style>\n"
        "        :root {\n"
        "            --primary-color: #0066cc;\n"
        "            --secondary-color: #f8f9fa;\n"
        "            --accent-color: #e63946;\n"
        "            --text-color: #333;\n"
        "            --light-text: #f8f9fa;\n"
        "            --border-color: #ddd;\n"
        "            --success-color: #4CAF50;\n"
        "            --error-color: #f44336;\n"
        "            --warning-color: #ff9800;\n"
        "        }\n"
        "        \n"
        "        * { box-sizing: border-box; }\n"
        "        \n"
        "        body {\n"
        "            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;\n"
        "            margin: 0;\n"
        "            padding: 0;\n"
        "            background: var(--secondary-color);\n"
        "            color: var(--text-color);\n"
        "        }\n"
        "        \n"
        "        .header {\n"
        "            background: var(--primary-color);\n"
        "            color: white;\n"
        "            padding: 15px 20px;\n"
        "            display: flex;\n"
        "            justify-content: space-between;\n"
        "            align-items: center;\n"
        "            box-shadow: 0 2px 5px rgba(0,0,0,0.1);\n"
        "        }\n"
        "        \n"
        "        .header h1 {\n"
        "            margin: 0;\n"
        "            font-size: 1.5rem;\n"
        "        }\n"
        "        \n"
        "        .main-container {\n"
        "            display: flex;\n"
        "            min-height: calc(100vh - 60px);\n"
        "        }\n"
        "        \n"
        "        .sidebar {\n"
        "            width: 250px;\n"
        "            background: #fff;\n"
        "            border-right: 1px solid var(--border-color);\n"
        "            padding: 20px 0;\n"
        "        }\n"
        "        \n"
        "        .sidebar-menu {\n"
        "            list-style-type: none;\n"
        "            padding: 0;\n"
        "            margin: 0;\n"
        "        }\n"
        "        \n"
        "        .sidebar-menu li {\n"
        "            padding: 10px 20px;\n"
        "            border-bottom: 1px solid var(--border-color);\n"
        "            cursor: pointer;\n"
        "        }\n"
        "        \n"
        "        .sidebar-menu li:hover {\n"
        "            background: var(--secondary-color);\n"
        "        }\n"
        "        \n"
        "        .sidebar-menu li.active {\n"
        "            background: var(--primary-color);\n"
        "            color: white;\n"
        "        }\n"
        "        \n"
        "        .content {\n"
        "            flex: 1;\n"
        "            padding: 20px;\n"
        "            overflow-y: auto;\n"
        "        }\n"
        "        \n"
        "        .card {\n"
        "            background: white;\n"
        "            border-radius: 4px;\n"
        "            box-shadow: 0 2px 5px rgba(0,0,0,0.1);\n"
        "            padding: 20px;\n"
        "            margin-bottom: 20px;\n"
        "        }\n"
        "        \n"
        "        .card h2 {\n"
        "            margin-top: 0;\n"
        "            color: var(--primary-color);\n"
        "            border-bottom: 1px solid var(--border-color);\n"
        "            padding-bottom: 10px;\n"
        "        }\n"
        "        \n"
        "        .info-section {\n"
        "            margin-bottom: 15px;\n"
        "        }\n"
        "        \n"
        "        pre {\n"
        "            background: #f5f5f5;\n"
        "            padding: 10px;\n"
        "            border-radius: 4px;\n"
        "            overflow-x: auto;\n"
        "            font-size: 0.9rem;\n"
        "        }\n"
        "        \n"
        "        .terminal-btn {\n"
        "            background: var(--primary-color);\n"
        "            color: white;\n"
        "            border: none;\n"
        "            padding: 10px 20px;\n"
        "            border-radius: 4px;\n"
        "            cursor: pointer;\n"
        "            font-size: 16px;\n"
        "            transition: background 0.2s;\n"
        "        }\n"
        "        \n"
        "        .terminal-btn:hover {\n"
        "            background: #0055aa;\n"
        "        }\n"
        "        \n"
        "        /* Terminal Popup Styles */\n"
        "        .terminal-popup {\n"
        "            display: none;\n"
        "            position: fixed;\n"
        "            top: 50%%;\n"
        "            left: 50%%;\n"
        "            transform: translate(-50%%, -50%%);\n"
        "            width: 80%%;\n"
        "            max-width: 800px;\n"
        "            height: 80%%;\n"
        "            max-height: 600px;\n"
        "            background: #1e1e1e;\n"
        "            border-radius: 8px;\n"
        "            box-shadow: 0 4px 20px rgba(0,0,0,0.3);\n"
        "            z-index: 1000;\n"
        "            overflow: hidden;\n"
        "            display: flex;\n"
        "            flex-direction: column;\n"
        "        }\n"
        "        \n"
        "        .terminal-header {\n"
        "            background: #333;\n"
        "            color: white;\n"
        "            padding: 10px 15px;\n"
        "            display: flex;\n"
        "            justify-content: space-between;\n"
        "            align-items: center;\n"
        "            user-select: none;\n"
        "        }\n"
        "        \n"
        "        .terminal-title {\n"
        "            margin: 0;\n"
        "            font-size: 14px;\n"
        "        }\n"
        "        \n"
        "        .terminal-controls {\n"
        "            display: flex;\n"
        "            gap: 5px;\n"
        "        }\n"
        "        \n"
        "        .terminal-control {\n"
        "            width: 12px;\n"
        "            height: 12px;\n"
        "            border-radius: 50%%;\n"
        "            cursor: pointer;\n"
        "        }\n"
        "        \n"
        "        .terminal-minimize {\n"
        "            background: #ffbd4c;\n"
        "        }\n"
        "        \n"
        "        .terminal-maximize {\n"
        "            background: #00ca56;\n"
        "        }\n"
        "        \n"
        "        .terminal-close {\n"
        "            background: #ff5f56;\n"
        "        }\n"
        "        \n"
        "        .terminal-body {\n"
        "            flex: 1;\n"
        "            padding: 15px;\n"
        "            color: #f0f0f0;\n"
        "            font-family: 'Consolas', 'Monaco', 'Courier New', monospace;\n"
        "            overflow-y: auto;\n"
        "            background: #1e1e1e;\n"
        "        }\n"
        "        \n"
        "        .terminal-prompt {\n"
        "            color: #4CAF50;\n"
        "            white-space: pre-wrap;\n"
        "            margin-bottom: 5px;\n"
        "        }\n"
        "        \n"
        "        .command-error {\n"
        "            color: #f44336;\n"
        "        }\n"
        "        \n"
        "        .command-success {\n"
        "            color: #4CAF50;\n"
        "        }\n"
        "        \n"
        "        .command-output {\n"
        "            margin-top: 5px;\n"
        "            margin-bottom: 15px;\n"
        "            white-space: pre-wrap;\n"
        "            word-break: break-word;\n"
        "        }\n"
        "        \n"
        "        .terminal-form {\n"
        "            display: flex;\n"
        "            padding: 10px 15px;\n"
        "            background: #2d2d2d;\n"
        "            border-top: 1px solid #444;\n"
        "        }\n"
        "        \n"
        "        .terminal-form input {\n"
        "            flex-grow: 1;\n"
        "            background: #2d2d2d;\n"
        "            border: none;\n"
        "            color: #f0f0f0;\n"
        "            padding: 8px;\n"
        "            font-family: 'Consolas', 'Monaco', 'Courier New', monospace;\n"
        "            font-size: 14px;\n"
        "            outline: none;\n"
        "        }\n"
        "        \n"
        "        .terminal-form button {\n"
        "            background: #4CAF50;\n"
        "            color: white;\n"
        "            border: none;\n"
        "            padding: 8px 15px;\n"
        "            margin-left: 10px;\n"
        "            border-radius: 4px;\n"
        "            cursor: pointer;\n"
        "            transition: background 0.2s;\n"
        "        }\n"
        "        \n"
        "        .terminal-form button:hover {\n"
        "            background: #3d8b40;\n"
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
        "        .hidden {\n"
        "            display: none;\n"
        "        }\n"
        "        \n"
        "        .tab-container {\n"
        "            display: flex;\n"
        "            border-bottom: 1px solid var(--border-color);\n"
        "            margin-bottom: 15px;\n"
        "        }\n"
        "        \n"
        "        .tab {\n"
        "            padding: 10px 20px;\n"
        "            cursor: pointer;\n"
        "            border-bottom: 2px solid transparent;\n"
        "        }\n"
        "        \n"
        "        .tab.active {\n"
        "            border-bottom: 2px solid var(--primary-color);\n"
        "            color: var(--primary-color);\n"
        "        }\n"
        "        \n"
        "        .tab-content {\n"
        "            display: none;\n"
        "        }\n"
        "        \n"
        "        .tab-content.active {\n"
        "            display: block;\n"
        "        }\n"
        "        \n"
        "        .common-commands {\n"
        "            display: flex;\n"
        "            flex-wrap: wrap;\n"
        "            gap: 10px;\n"
        "            margin-bottom: 15px;\n"
        "        }\n"
        "        \n"
        "        .command-chip {\n"
        "            background: var(--secondary-color);\n"
        "            border: 1px solid var(--border-color);\n"
        "            border-radius: 20px;\n"
        "            padding: 5px 15px;\n"
        "            cursor: pointer;\n"
        "            font-size: 0.9rem;\n"
        "            transition: all 0.2s;\n"
        "        }\n"
        "        \n"
        "        .command-chip:hover {\n"
        "            background: var(--primary-color);\n"
        "            color: white;\n"
        "        }\n"
        "        \n"
        "        /* Responsive adjustments */\n"
        "        @media (max-width: 768px) {\n"
        "            .main-container {\n"
        "                flex-direction: column;\n"
        "            }\n"
        "            \n"
        "            .sidebar {\n"
        "                width: 100%%;\n"
        "                border-right: none;\n"
        "                border-bottom: 1px solid var(--border-color);\n"
        "                padding: 10px 0;\n"
        "            }\n"
        "            \n"
        "            .terminal-popup {\n"
        "                width: 95%%;\n"
        "                height: 90%%;\n"
        "            }\n"
        "        }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <div class=\"header\">\n"
        "        <h1>OpenWRT Management Interface</h1>\n"
        "        <button class=\"terminal-btn\" onclick=\"openTerminal()\">Terminal</button>\n"
        "    </div>\n"
        "    \n"
        "    <div class=\"main-container\">\n"
        "        <div class=\"sidebar\">\n"
        "            <ul class=\"sidebar-menu\">\n"
        "                <li class=\"active\" onclick=\"showTab('dashboard')\">Dashboard</li>\n"
        "                <li onclick=\"showTab('network')\">Network</li>\n"
        "                <li onclick=\"showTab('system')\">System</li>\n"
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
        "                    <p>Welcome to the OpenWRT Management Interface. This dashboard provides an overview of your system.</p>\n"
        "                    <p><strong>Client IP:</strong> %s</p>\n"
        "                    <p><strong>Server Time:</strong> %s</p>\n"
        "                    <button class=\"terminal-btn\" onclick=\"openTerminal()\">Open Terminal</button>\n"
        "                </div>\n"
        "                \n"
        "                <div class=\"card\">\n"
        "                    <h2>Quick Status</h2>\n"
        "                    <div class=\"tab-container\">\n"
        "                        <div class=\"tab active\" onclick=\"showQuickTab('quick-system')\">System</div>\n"
        "                        <div class=\"tab\" onclick=\"showQuickTab('quick-network')\">Network</div>\n"
        "                    </div>\n"
        "                    \n"
        "                    <div id=\"quick-system\" class=\"tab-content active\">\n"
        "                        %s\n"
        "                    </div>\n"
        "                    \n"
        "                    <div id=\"quick-network\" class=\"tab-content\">\n"
        "                        %s\n"
        "                    </div>\n"
        "                </div>\n"
        "            </div>\n"
        "            \n"
        "            <div id=\"network\" class=\"tab-content\">\n"
        "                <div class=\"card\">\n"
        "                    <h2>Network Configuration</h2>\n"
        "                    <p>View and manage network interfaces, wireless settings, and routing.</p>\n"
        "                    \n"
        "                    <div class=\"common-commands\">\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('ifconfig')\">ifconfig</div>\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('iwconfig')\">iwconfig</div>\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('route -n')\">route -n</div>\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('cat /etc/config/network')\">network config</div>\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('cat /etc/config/wireless')\">wireless config</div>\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('ping -c 4 8.8.8.8')\">ping test</div>\n"
        "                    </div>\n"
        "                    \n"
        "                    %s\n"
        "                </div>\n"
        "            </div>\n"
        "            \n"
        "            <div id=\"system\" class=\"tab-content\">\n"
        "                <div class=\"card\">\n"
        "                    <h2>System Information</h2>\n"
        "                    <p>View and manage system settings, firmware, and hardware information.</p>\n"
        "                    \n"
        "                    <div class=\"common-commands\">\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('cat /etc/openwrt_release')\">OpenWRT Version</div>\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('uname -a')\">Kernel Info</div>\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('uptime')\">Uptime</div>\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('free -h')\">Memory Info</div>\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('df -h')\">Disk Space</div>\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('top -b -n 1 | head -n 20')\">Process List</div>\n"
        "                    </div>\n"
        "                    \n"
        "                    %s\n"
        "                </div>\n"
        "            </div>\n"
        "            \n"
        "            <div id=\"services\" class=\"tab-content\">\n"
        "                <div class=\"card\">\n"
        "                    <h2>Services Management</h2>\n"
        "                    <p>View and control system services.</p>\n"
        "                    \n"
        "                    <div class=\"common-commands\">\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('/etc/init.d/network status')\">Network Service</div>\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('/etc/init.d/firewall status')\">Firewall Service</div>\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('/etc/init.d/dnsmasq status')\">DHCP/DNS Service</div>\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('ps | grep -v grep | grep -E \"dnsmasq|hostapd\"')\">Network Processes</div>\n"
        "                    </div>\n"
        "                </div>\n"
        "            </div>\n"
        "            \n"
        "            <div id=\"firewall\" class=\"tab-content\">\n"
        "                <div class=\"card\">\n"
        "                    <h2>Firewall Settings</h2>\n"
        "                    <p>View and manage firewall rules and port forwarding.</p>\n"
        "                    \n"
        "                    <div class=\"common-commands\">\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('cat /etc/config/firewall')\">Firewall Config</div>\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('iptables -L -n')\">IPTables Rules</div>\n"
        "                        <div class=\"command-chip\" onclick=\"executeCommand('iptables -t nat -L -n')\">NAT Rules</div>\n"
        "                    </div>\n"
        "                </div>\n"
        "            </div>\n"
        "        </div>\n"
        "    </div>\n"
        "    \n"
        "    <!-- Terminal Popup -->\n"
        "    <div class=\"overlay\" id=\"overlay\"></div>\n"
        "    <div class=\"terminal-popup\" id=\"terminal\">\n"
        "        <div class=\"terminal-header\">\n"
        "            <h3 class=\"terminal-title\">OpenWRT Terminal</h3>\n"
        "            <div class=\"terminal-controls\">\n"
        "                <div class=\"terminal-control terminal-minimize\" onclick=\"minimizeTerminal()\"></div>\n"
        "                <div class=\"terminal-control terminal-maximize\" onclick=\"maximizeTerminal()\"></div>\n"
        "                <div class=\"terminal-control terminal-close\" onclick=\"closeTerminal()\"></div>\n"
        "            </div>\n"
        "        </div>\n"
        "        <div class=\"terminal-body\" id=\"terminal-body\">\n"
        "            <div class=\"terminal-prompt\">Welcome to OpenWRT Terminal</div>\n"
        "            <div class=\"terminal-prompt\">Type 'help' for a list of common commands</div>\n",
        client_ip, ctime(&now), system_info, network_info);
    
    // Add command history if any
    if (history_count > 0) {
        for (int i = 0; i < history_count; i++) {
            char *end = buffer + strlen(buffer);
            snprintf(end, BUFFER_SIZE - (end - buffer),
                "            <div class=\"terminal-prompt\">$ %s</div>\n",
                command_history[i]);
            
            // Add the current command output if this is the most recent command
            if (i == history_count - 1 && command && strcmp(command, command_history[i]) == 0 && cmd_output) {
                snprintf(end + strlen(end), BUFFER_SIZE - (end - buffer + strlen(end)),
                    "            <div class=\"command-output %s\">%s</div>\n",
                    exit_status == 0 ? "command-success" : "command-error",
                    cmd_output);
            }
        }
    }
    
    // Add the command output if command is not in history but was just executed
    if (command && command[0] && cmd_output && 
        (history_count == 0 || strcmp(command, command_history[history_count-1]) != 0)) {
        char *end = buffer + strlen(buffer);
        snprintf(end, BUFFER_SIZE - (end - buffer),
            "            <div class=\"terminal-prompt\">$ %s</div>\n"
            "            <div class=\"command-output %s\">%s</div>\n",
            command, 
            exit_status == 0 ? "command-success" : "command-error",
            cmd_output);
    }
    
    // Add the rest of the HTML
    strcat(buffer,
        "        </div>\n"
        "        <form method=\"GET\" action=\"/\" class=\"terminal-form\">\n"
        "            <input type=\"text\" name=\"command\" id=\"commandInput\" placeholder=\"Enter command...\" autocomplete=\"off\">\n"
        "            <button type=\"submit\">Execute</button>\n"
        "        </form>\n"
        "    </div>\n"
        "    \n"
        "    <script>\n"
        "        // Show initial command output if there's a command\n"
        "        window.onload = function() {\n"
        "            // If there's a command in the URL, open the terminal\n"
        "            var urlParams = new URLSearchParams(window.location.search);\n"
        "            if (urlParams.has('command')) {\n"
        "                openTerminal();\n"
        "                // Scroll terminal to bottom\n"
        "                var terminalBody = document.getElementById('terminal-body');\n"
        "                terminalBody.scrollTop = terminalBody.scrollHeight;\n"
        "            }\n"
        "        };\n"
        "        \n"
        "        function openTerminal() {\n"
        "            document.getElementById('terminal').style.display = 'flex';\n"
        "            document.getElementById('overlay').style.display = 'block';\n"
        "            document.getElementById('commandInput').focus();\n"
        "        }\n"
        "        \n"
        "        function closeTerminal() {\n"
        "            document.getElementById('terminal').style.display = 'none';\n"
        "            document.getElementById('overlay').style.display = 'none';\n"
        "        }\n"
        "        \n"
        "        function minimizeTerminal() {\n"
        "            // Simple minimize effect\n"
        "            var terminal = document.getElementById('terminal');\n"
        "            terminal.style.transform = 'translate(-50%, -50%) scale(0.8)';\n"
        "            setTimeout(function() {\n"
        "                closeTerminal();\n"
        "                terminal.style.transform = 'translate(-50%, -50%)';\n"
        "            }, 200);\n"
        "        }\n"
        "        \n"
        "        function maximizeTerminal() {\n"
        "            var terminal = document.getElementById('terminal');\n"
        "            if (terminal.style.width === '95%' && terminal.style.height === '95%') {\n"
        "                // Restore\n"
        "                terminal.style.width = '80%';\n"
        "                terminal.style.height = '80%';\n"
        "            } else {\n"
        "                // Maximize\n"
        "                terminal.style.width = '95%';\n"
        "                terminal.style.height = '95%';\n"
        "            }\n"
        "        }\n"
        "        \n"
        "        // Close terminal when clicking outside\n"
        "        document.getElementById('overlay').addEventListener('click', closeTerminal);\n"
        "        \n"
        "        // Prevent terminal from closing when clicking inside it\n"
        "        document.getElementById('terminal').addEventListener('click', function(e) {\n"
        "            e.stopPropagation();\n"
        "        });\n"
        "        \n"
        "        // Tab functionality\n"
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
        "            var menuItems = document.getElementsByClassName('sidebar-menu')[0].getElementsByTagName('li');\n"
        "            for (var i = 0; i < menuItems.length; i++) {\n"
        "                menuItems[i].classList.remove('active');\n"
        "                if (menuItems[i].innerText.toLowerCase() === tabId.toLowerCase()) {\n"
        "                    menuItems[i].classList.add('active');\n"
        "                }\n"
        "            }\n"
        "        }\n"
        "        \n"
        "        // Quick tab functionality\n"
        "        function showQuickTab(tabId) {\n"
        "            // Hide all quick tab contents\n"
        "            var quickTabContents = document.querySelectorAll('#quick-system, #quick-network');\n"
        "            for (var i = 0; i < quickTabContents.length; i++) {\n"
        "                quickTabContents[i].classList.remove('active');\n"
        "            }\n"
        "            \n"
        "            // Show selected quick tab content\n"
        "            document.getElementById(tabId).classList.add('active');\n"
        "            \n"
        "            // Update active quick tab\n"
        "            var quickTabs = document.querySelectorAll('.tab-container .tab');\n"
        "            for (var i = 0; i < quickTabs.length; i++) {\n"
        "                quickTabs[i].classList.remove('active');\n"
        "                if (quickTabs[i].innerText.toLowerCase() === tabId.split('-')[1].toLowerCase()) {\n"
        "                    quickTabs[i].classList.add('active');\n"
        "                }\n"
        "            }\n"
        "        }\n"
        "        \n"
        "        // Execute command from UI elements\n"
        "        function executeCommand(command) {\n"
        "            // Set the command in the input field\n"
        "            document.getElementById('commandInput').value = command;\n"
        "            \n"
        "            // Open terminal\n"
        "            openTerminal();\n"
        "            \n"
        "            // Submit the form\n"
        "            document.querySelector('.terminal-form').submit();\n"
        "        }\n"
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
