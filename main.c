#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>

#define DEFAULT_PORT 5000
#define BUFFER_SIZE 65536  // Larger buffer for complex HTML
#define MAX_COMMAND_SIZE 2048
#define MAX_HISTORY 10
#define MAX_PATH_LENGTH 256
#define TEMPLATE_MAX_SIZE 65536
#define ULTIMA_SERVER "example.ultimarobotics.com"

// Global command history
char command_history[MAX_HISTORY][MAX_COMMAND_SIZE];
int history_count = 0;

// Content types for different file extensions
typedef struct {
    const char *extension;
    const char *mime_type;
} content_type_mapping;

content_type_mapping content_types[] = {
    {".html", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".ico", "image/x-icon"},
    {NULL, NULL}
};

// Structure for system metrics
typedef struct {
    // CPU
    float cpu_usage;
    
    // Memory
    unsigned long total_memory;
    unsigned long used_memory;
    float memory_usage;
    
    // Storage
    unsigned long total_storage;
    unsigned long free_storage;
    unsigned long used_storage;
    float storage_usage;
    
    // Bandwidth (in KB/s)
    float download_rate;
    float upload_rate;
    unsigned long rx_bytes;
    unsigned long tx_bytes;
    unsigned long last_rx_bytes;
    unsigned long last_tx_bytes;
    time_t last_bandwidth_check;
    
    // Connection status
    int internet_connected;
    int ultima_server_connected;
} system_metrics;

// Global metrics storage
system_metrics metrics;

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

// Function to check connectivity to internet
int check_internet_connectivity() {
    struct hostent *host = gethostbyname("google.com");
    return (host != NULL);
}

// Function to check connectivity to Ultima Robotics server
int check_ultima_server_connectivity() {
    struct hostent *host = gethostbyname(ULTIMA_SERVER);
    return (host != NULL);
}

// Function to get CPU usage
float get_cpu_usage() {
    FILE *fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        return 0.0;
    }
    
    static unsigned long long int prev_total = 0, prev_idle = 0;
    unsigned long long int user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
    char cpu[10];
    
    if (fscanf(fp, "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
               cpu, &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice) != 11) {
        fclose(fp);
        return 0.0;
    }
    
    fclose(fp);
    
    unsigned long long int total = user + nice + system + idle + iowait + irq + softirq + steal;
    unsigned long long int total_diff = total - prev_total;
    unsigned long long int idle_diff = idle - prev_idle;
    
    float cpu_usage = 0.0;
    if (total_diff != 0) {
        cpu_usage = 100.0 * (1.0 - ((float)idle_diff / (float)total_diff));
    }
    
    prev_total = total;
    prev_idle = idle;
    
    return cpu_usage;
}

// Function to get memory usage
void get_memory_usage(unsigned long *total, unsigned long *used, float *percentage) {
    struct sysinfo info;
    
    if (sysinfo(&info) != 0) {
        *total = 0;
        *used = 0;
        *percentage = 0.0;
        return;
    }
    
    *total = info.totalram / 1024 / 1024;  // Convert to MB
    *used = (info.totalram - info.freeram) / 1024 / 1024;  // Convert to MB
    *percentage = 100.0 * ((float)*used / (float)*total);
}

// Function to get storage usage
void get_storage_usage(unsigned long *total, unsigned long *free, unsigned long *used, float *percentage) {
    struct statvfs fs_info;
    
    if (statvfs("/", &fs_info) != 0) {
        *total = 0;
        *free = 0;
        *used = 0;
        *percentage = 0.0;
        return;
    }
    
    *total = (fs_info.f_blocks * fs_info.f_frsize) / 1024 / 1024;  // Convert to MB
    *free = (fs_info.f_bfree * fs_info.f_frsize) / 1024 / 1024;    // Convert to MB
    *used = *total - *free;
    *percentage = 100.0 * ((float)*used / (float)*total);
}

// Function to update network bandwidth
void update_bandwidth() {
    time_t now = time(NULL);
    FILE *fp = fopen("/proc/net/dev", "r");
    
    if (fp == NULL) {
        metrics.download_rate = 0;
        metrics.upload_rate = 0;
        return;
    }
    
    // Skip the first two lines (headers)
    char line[256];
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);
    
    unsigned long rx_bytes = 0;
    unsigned long tx_bytes = 0;
    
    // Read network interfaces
    while (fgets(line, sizeof(line), fp)) {
        char *pos = strchr(line, ':');
        if (pos) {
            pos++;
            unsigned long interface_rx, interface_tx;
            if (sscanf(pos, "%lu %*u %*u %*u %*u %*u %*u %*u %lu", &interface_rx, &interface_tx) == 2) {
                // Skip loopback interface
                if (strstr(line, "lo:") == NULL) {
                    rx_bytes += interface_rx;
                    tx_bytes += interface_tx;
                }
            }
        }
    }
    
    fclose(fp);
    
    // Calculate bandwidth if we have previous values
    if (metrics.last_bandwidth_check > 0) {
        double time_diff = difftime(now, metrics.last_bandwidth_check);
        
        if (time_diff > 0) {
            // Calculate download rate (KB/s)
            if (rx_bytes >= metrics.last_rx_bytes) {
                metrics.download_rate = (rx_bytes - metrics.last_rx_bytes) / 1024.0 / time_diff;
            } else {
                // Counter reset
                metrics.download_rate = 0;
            }
            
            // Calculate upload rate (KB/s)
            if (tx_bytes >= metrics.last_tx_bytes) {
                metrics.upload_rate = (tx_bytes - metrics.last_tx_bytes) / 1024.0 / time_diff;
            } else {
                // Counter reset
                metrics.upload_rate = 0;
            }
        }
    }
    
    // Update the stored values
    metrics.last_rx_bytes = rx_bytes;
    metrics.last_tx_bytes = tx_bytes;
    metrics.last_bandwidth_check = now;
    metrics.rx_bytes = rx_bytes;
    metrics.tx_bytes = tx_bytes;
}

// Function to update all system metrics
void update_metrics() {
    // Update CPU usage
    metrics.cpu_usage = get_cpu_usage();
    
    // Update memory usage
    get_memory_usage(&metrics.total_memory, &metrics.used_memory, &metrics.memory_usage);
    
    // Update storage usage
    get_storage_usage(&metrics.total_storage, &metrics.free_storage, &metrics.used_storage, &metrics.storage_usage);
    
    // Update bandwidth
    update_bandwidth();
    
    // Update connection status
    metrics.internet_connected = check_internet_connectivity();
    metrics.ultima_server_connected = check_ultima_server_connectivity();
}

// Function to escape JSON strings
char* json_escape_string(const char *str) {
    if (!str) return strdup("");
    
    size_t escaped_len = 0;
    const char *p;
    
    // Calculate the size needed for the escaped string
    for (p = str; *p; p++) {
        switch (*p) {
            case '\"': case '\\': case '/': case '\b': case '\f': case '\n': case '\r': case '\t':
                escaped_len += 2;
                break;
            default:
                escaped_len++;
                break;
        }
    }
    
    // Allocate memory for the escaped string
    char *escaped = malloc(escaped_len + 1);
    if (!escaped) return strdup("");
    
    // Copy and escape the string
    char *q = escaped;
    for (p = str; *p; p++) {
        switch (*p) {
            case '\"': *q++ = '\\'; *q++ = '\"'; break;
            case '\\': *q++ = '\\'; *q++ = '\\'; break;
            case '/': *q++ = '\\'; *q++ = '/'; break;
            case '\b': *q++ = '\\'; *q++ = 'b'; break;
            case '\f': *q++ = '\\'; *q++ = 'f'; break;
            case '\n': *q++ = '\\'; *q++ = 'n'; break;
            case '\r': *q++ = '\\'; *q++ = 'r'; break;
            case '\t': *q++ = '\\'; *q++ = 't'; break;
            default: *q++ = *p;
        }
    }
    *q = '\0';
    
    return escaped;
}

// Function to generate JSON metrics
char* generate_metrics_json() {
    // Update metrics before generating JSON
    update_metrics();
    
    // Format storage numbers for human readability
    char storage_used_formatted[32];
    char storage_total_formatted[32];
    
    if (metrics.used_storage < 1024) {
        sprintf(storage_used_formatted, "%lu MB", metrics.used_storage);
    } else {
        sprintf(storage_used_formatted, "%.1f GB", metrics.used_storage / 1024.0);
    }
    
    if (metrics.total_storage < 1024) {
        sprintf(storage_total_formatted, "%lu MB", metrics.total_storage);
    } else {
        sprintf(storage_total_formatted, "%.1f GB", metrics.total_storage / 1024.0);
    }
    
    // Build JSON response
    char *json = malloc(4096);
    if (!json) return NULL;
    
    sprintf(json, 
        "{\n"
        "  \"cpu\": {\n"
        "    \"usage\": %.1f\n"
        "  },\n"
        "  \"memory\": {\n"
        "    \"total\": %lu,\n"
        "    \"used\": %lu,\n"
        "    \"usage\": %.1f\n"
        "  },\n"
        "  \"storage\": {\n"
        "    \"total\": %lu,\n"
        "    \"used\": %lu,\n"
        "    \"free\": %lu,\n"
        "    \"usage\": %.1f,\n"
        "    \"used_formatted\": \"%s\",\n"
        "    \"total_formatted\": \"%s\"\n"
        "  },\n"
        "  \"bandwidth\": {\n"
        "    \"download\": %.1f,\n"
        "    \"upload\": %.1f\n"
        "  },\n"
        "  \"internet\": {\n"
        "    \"connected\": %s\n"
        "  },\n"
        "  \"ultima_server\": {\n"
        "    \"connected\": %s\n"
        "  }\n"
        "}",
        metrics.cpu_usage,
        metrics.total_memory, metrics.used_memory, metrics.memory_usage,
        metrics.total_storage, metrics.used_storage, metrics.free_storage, metrics.storage_usage,
        storage_used_formatted, storage_total_formatted,
        metrics.download_rate, metrics.upload_rate,
        metrics.internet_connected ? "true" : "false",
        metrics.ultima_server_connected ? "true" : "false"
    );
    
    return json;
}

// Function to get system information for OpenWRT
char* get_system_info() {
    static char info[4096];
    int exit_status;
    
    // Try to get OpenWRT version
    char *version_output = execute_command("cat /etc/openwrt_release 2>/dev/null || echo 'OpenWRT version information not available'", &exit_status);
    char *kernel_output = execute_command("uname -a", &exit_status);
    char *uptime_output = execute_command("uptime", &exit_status);
    char *cpu_info = execute_command("cat /proc/cpuinfo | grep 'model name' | head -1 || cat /proc/cpuinfo | grep 'cpu model' | head -1", &exit_status);
    
    // Format the info
    snprintf(info, sizeof(info),
        "<div class=\"system-info\">"
        "<h3>System Details</h3>"
        "<pre>%s</pre>"
        "<h3>Kernel Information</h3>"
        "<pre>%s</pre>"
        "<h3>Uptime</h3>"
        "<pre>%s</pre>"
        "<h3>CPU Information</h3>"
        "<pre>%s</pre>"
        "</div>",
        version_output ? version_output : "Could not retrieve version information",
        kernel_output ? kernel_output : "Could not retrieve kernel information",
        uptime_output ? uptime_output : "Could not retrieve uptime information",
        cpu_info ? cpu_info : "Could not retrieve CPU information"
    );
    
    // Free memory
    if (version_output) free(version_output);
    if (kernel_output) free(kernel_output);
    if (uptime_output) free(uptime_output);
    if (cpu_info) free(cpu_info);
    
    return info;
}

// Function to get network information for OpenWRT
char* get_network_info() {
    static char info[8192];
    int exit_status;
    
    // Get network interfaces
    char *interfaces = execute_command("ifconfig", &exit_status);
    char *wireless = execute_command("iwconfig 2>/dev/null || echo 'No wireless interfaces found'", &exit_status);
    char *routing = execute_command("route -n", &exit_status);
    char *dns = execute_command("cat /etc/resolv.conf", &exit_status);
    
    // Format the info
    snprintf(info, sizeof(info),
        "<div class=\"network-info\">"
        "<h3>Network Interfaces</h3>"
        "<pre>%s</pre>"
        "<h3>Wireless Interfaces</h3>"
        "<pre>%s</pre>"
        "<h3>Routing Table</h3>"
        "<pre>%s</pre>"
        "<h3>DNS Configuration</h3>"
        "<pre>%s</pre>"
        "</div>",
        interfaces ? interfaces : "Could not retrieve interface information",
        wireless ? wireless : "Could not retrieve wireless information",
        routing ? routing : "Could not retrieve routing information",
        dns ? dns : "Could not retrieve DNS information"
    );
    
    // Free memory
    if (interfaces) free(interfaces);
    if (wireless) free(wireless);
    if (routing) free(routing);
    if (dns) free(dns);
    
    return info;
}

// Function to get the OpenWRT version
char* get_openwrt_version() {
    static char version[128] = "Unknown";
    int exit_status;
    
    char *output = execute_command("cat /etc/openwrt_release 2>/dev/null | grep DISTRIB_RELEASE | cut -d \"'\" -f 2", &exit_status);
    if (output && strlen(output) > 0) {
        // Remove trailing newline if present
        char *newline = strchr(output, '\n');
        if (newline) *newline = '\0';
        
        snprintf(version, sizeof(version), "%s", output);
        free(output);
    }
    
    return version;
}

// Function to get the kernel version
char* get_kernel_version() {
    static char version[128] = "Unknown";
    int exit_status;
    
    char *output = execute_command("uname -r", &exit_status);
    if (output && strlen(output) > 0) {
        // Remove trailing newline if present
        char *newline = strchr(output, '\n');
        if (newline) *newline = '\0';
        
        snprintf(version, sizeof(version), "%s", output);
        free(output);
    }
    
    return version;
}

// Function to get the system uptime
char* get_uptime() {
    static char uptime_str[128] = "Unknown";
    int exit_status;
    
    char *output = execute_command("uptime -p 2>/dev/null || uptime", &exit_status);
    if (output && strlen(output) > 0) {
        // Remove trailing newline if present
        char *newline = strchr(output, '\n');
        if (newline) *newline = '\0';
        
        snprintf(uptime_str, sizeof(uptime_str), "%s", output);
        free(output);
    }
    
    return uptime_str;
}

// Determine the content type based on the file extension
const char* get_content_type(const char *path) {
    const char *extension = strrchr(path, '.');
    if (!extension) return "text/plain";
    
    for (int i = 0; content_types[i].extension; i++) {
        if (strcmp(extension, content_types[i].extension) == 0) {
            return content_types[i].mime_type;
        }
    }
    
    return "text/plain";
}

// Read a file into a buffer
char* read_file(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *buffer = malloc(*size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    size_t bytes_read = fread(buffer, 1, *size, file);
    fclose(file);
    
    if (bytes_read != *size) {
        free(buffer);
        return NULL;
    }
    
    buffer[*size] = '\0';
    return buffer;
}

// Function to check if a file exists and is readable
int file_exists(const char *path) {
    return access(path, R_OK) == 0;
}

// Function to handle static file requests
void handle_static_file(int socket, const char *path) {
    char file_path[MAX_PATH_LENGTH];
    
    // Skip leading / in path if present
    if (path[0] == '/') path++;
    
    // Build the file path
    snprintf(file_path, MAX_PATH_LENGTH, "%s%s", path[0] ? "public/" : "public/index.html", path);
    
    // Special case for root path
    if (strcmp(path, "") == 0 || strcmp(path, "/") == 0) {
        strcpy(file_path, "templates/index.html");
    }
    
    // Try to open the file
    size_t file_size;
    char *file_content = read_file(file_path, &file_size);
    
    if (!file_content) {
        // File not found or error reading
        char not_found[] = 
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 121\r\n"
            "\r\n"
            "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><p>The requested file was not found.</p></body></html>";
        
        send(socket, not_found, strlen(not_found), 0);
        return;
    }
    
    // Determine the content type
    const char *content_type = get_content_type(file_path);
    
    // Prepare the response header
    char header[256];
    snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "\r\n", content_type, file_size);
    
    // Send the header
    send(socket, header, strlen(header), 0);
    
    // Send the file content
    send(socket, file_content, file_size, 0);
    
    // Clean up
    free(file_content);
}

// Function to render HTML template with data
void render_template(char *buffer, const char *client_ip, const char *command, const char *cmd_output, int exit_status) {
    // Read the template file
    size_t template_size;
    char *template = read_file("templates/index.html", &template_size);
    
    if (!template) {
        // Template not found, use a basic HTML response
        sprintf(buffer,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "\r\n"
            "<html><head><title>Error</title></head><body>"
            "<h1>Template Error</h1>"
            "<p>Could not load the template file.</p>"
            "</body></html>"
        );
        return;
    }
    
    // Get current time for server time display
    time_t now;
    time(&now);
    char *time_str = ctime(&now);
    // Remove trailing newline from time string
    if (time_str[strlen(time_str) - 1] == '\n') {
        time_str[strlen(time_str) - 1] = '\0';
    }
    
    // Get system information
    char *system_info = get_system_info();
    char *network_info = get_network_info();
    char *openwrt_version = get_openwrt_version();
    char *kernel_version = get_kernel_version();
    char *uptime = get_uptime();
    
    // Create a new string buffer for the processed template
    char *processed = malloc(TEMPLATE_MAX_SIZE);
    if (!processed) {
        free(template);
        sprintf(buffer, 
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "Memory allocation error"
        );
        return;
    }
    
    // Simple template replacement (not a full template engine)
    char *pos = template;
    char *write_pos = processed;
    int remaining = TEMPLATE_MAX_SIZE - 1;  // -1 for null terminator
    
    while (*pos && remaining > 0) {
        // Look for placeholder tags
        if (*pos == '{' && *(pos+1) == '{') {
            pos += 2;  // Skip the {{ characters
            
            // Extract the placeholder name
            char placeholder[128] = {0};
            char *p = placeholder;
            while (*pos && *pos != '}' && (p - placeholder) < sizeof(placeholder) - 1) {
                *p++ = *pos++;
            }
            *p = '\0';
            
            // Skip the closing }}
            if (*pos == '}' && *(pos+1) == '}') {
                pos += 2;
            }
            
            // Trim whitespace
            char *trim = placeholder;
            while (*trim == ' ') trim++;
            
            // Replace the placeholder with the actual value
            int written = 0;
            
            if (strcmp(trim, "client_ip") == 0) {
                written = snprintf(write_pos, remaining, "%s", client_ip);
            }
            else if (strcmp(trim, "server_time") == 0) {
                written = snprintf(write_pos, remaining, "%s", time_str);
            }
            else if (strcmp(trim, "system_info") == 0) {
                written = snprintf(write_pos, remaining, "%s", system_info);
            }
            else if (strcmp(trim, "network_info") == 0) {
                written = snprintf(write_pos, remaining, "%s", network_info);
            }
            else if (strcmp(trim, "openwrt_version") == 0) {
                written = snprintf(write_pos, remaining, "%s", openwrt_version);
            }
            else if (strcmp(trim, "kernel_version") == 0) {
                written = snprintf(write_pos, remaining, "%s", kernel_version);
            }
            else if (strcmp(trim, "uptime") == 0) {
                written = snprintf(write_pos, remaining, "%s", uptime);
            }
            else if (strcmp(trim, "#terminal_history") == 0) {
                // Terminal history is more complex to handle
                // For simplicity, we'll just skip this part in this simple implementation
                // A proper template engine would handle this
                written = 0;
            }
            else {
                // Unknown placeholder, just write it back
                written = snprintf(write_pos, remaining, "{{%s}}", placeholder);
            }
            
            if (written > 0) {
                write_pos += written;
                remaining -= written;
            }
        }
        else {
            // Regular character, just copy it
            *write_pos++ = *pos++;
            remaining--;
        }
    }
    
    // Null terminate the processed string
    *write_pos = '\0';
    
    // Add the terminal history if room
    if (history_count > 0 && strstr(processed, "terminal-body") != NULL) {
        char *terminal_body_end = strstr(processed, "</div>\n        <form");
        if (terminal_body_end) {
            char history_html[4096] = {0};
            char *p = history_html;
            int remaining_history = sizeof(history_html) - 1;
            
            for (int i = 0; i < history_count && remaining_history > 0; i++) {
                int written = snprintf(p, remaining_history, 
                    "            <div class=\"terminal-prompt\">$ %s</div>\n", 
                    command_history[i]);
                
                if (written > 0) {
                    p += written;
                    remaining_history -= written;
                }
                
                // Add the current command output if this is the most recent command
                if (i == history_count - 1 && command && strcmp(command, command_history[i]) == 0 && cmd_output) {
                    written = snprintf(p, remaining_history,
                        "            <div class=\"command-output %s\">%s</div>\n",
                        exit_status == 0 ? "command-success" : "command-error",
                        cmd_output);
                    
                    if (written > 0) {
                        p += written;
                        remaining_history -= written;
                    }
                }
            }
            
            // Add the command output if command is not in history but was just executed
            if (command && command[0] && cmd_output && 
                (history_count == 0 || strcmp(command, command_history[history_count-1]) != 0)) {
                int written = snprintf(p, remaining_history,
                    "            <div class=\"terminal-prompt\">$ %s</div>\n"
                    "            <div class=\"command-output %s\">%s</div>\n",
                    command, 
                    exit_status == 0 ? "command-success" : "command-error",
                    cmd_output);
                
                if (written > 0) {
                    p += written;
                    remaining_history -= written;
                }
            }
            
            // Now insert the history_html before terminal_body_end
            size_t history_len = strlen(history_html);
            size_t insert_pos = terminal_body_end - processed;
            
            if (insert_pos + history_len < TEMPLATE_MAX_SIZE) {
                // Make room by moving the tail
                memmove(processed + insert_pos + history_len, processed + insert_pos, 
                        strlen(processed + insert_pos) + 1);  // +1 for null terminator
                
                // Insert the history
                memcpy(processed + insert_pos, history_html, history_len);
            }
        }
    }
    
    // Prepare the HTTP response
    sprintf(buffer,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s",
        strlen(processed), processed);
    
    // Clean up
    free(template);
    free(processed);
}

// MQTT broker status
typedef struct {
    int running;
    int client_count;
    int messages_published;
    int messages_received;
} mqtt_status;

// Global MQTT status
mqtt_status mqtt_state = {0, 0, 0, 0};

// Function to generate MQTT status JSON
char* generate_mqtt_status_json() {
    char *json = malloc(256);
    if (!json) return NULL;
    
    sprintf(json, 
        "{\n"
        "  \"running\": %s,\n"
        "  \"clients\": %d,\n"
        "  \"published\": %d,\n"
        "  \"received\": %d\n"
        "}",
        mqtt_state.running ? "true" : "false",
        mqtt_state.client_count,
        mqtt_state.messages_published,
        mqtt_state.messages_received
    );
    
    return json;
}

// Function to generate system information JSON
char* generate_system_json() {
    int exit_status;
    
    // Get OpenWRT version
    char *version = get_openwrt_version();
    
    // Get kernel version
    char *kernel = get_kernel_version();
    
    // Get uptime
    char *uptime = get_uptime();
    
    // Get CPU info
    char *cpu_info = execute_command("cat /proc/cpuinfo | grep 'model name' | head -1 || cat /proc/cpuinfo | grep 'cpu model' | head -1", &exit_status);
    
    // Format JSON
    char *json = malloc(4096);
    if (!json) {
        if (cpu_info) free(cpu_info);
        return NULL;
    }
    
    // Escape strings for JSON
    char *version_esc = json_escape_string(version);
    char *kernel_esc = json_escape_string(kernel);
    char *uptime_esc = json_escape_string(uptime);
    char *cpu_esc = json_escape_string(cpu_info ? cpu_info : "Unknown");
    
    sprintf(json, 
        "{\n"
        "  \"openwrt_version\": \"%s\",\n"
        "  \"kernel_version\": \"%s\",\n"
        "  \"uptime\": \"%s\",\n"
        "  \"cpu_info\": \"%s\"\n"
        "}",
        version_esc,
        kernel_esc,
        uptime_esc,
        cpu_esc
    );
    
    // Clean up
    free(version_esc);
    free(kernel_esc);
    free(uptime_esc);
    free(cpu_esc);
    if (cpu_info) free(cpu_info);
    
    return json;
}

// Function to generate network information JSON
char* generate_network_json() {
    int exit_status;
    
    // Get interfaces
    char *interfaces = execute_command("ifconfig | grep -E '^[a-zA-Z0-9]+' | awk '{print $1}'", &exit_status);
    
    // Get IP addresses
    char *ip_addresses = execute_command("ifconfig | grep -E 'inet addr:|inet '", &exit_status);
    
    // Get routing
    char *routing = execute_command("route -n | tail -n +3", &exit_status);
    
    // Format JSON
    char *json = malloc(8192);
    if (!json) {
        if (interfaces) free(interfaces);
        if (ip_addresses) free(ip_addresses);
        if (routing) free(routing);
        return NULL;
    }
    
    // Escape strings for JSON
    char *interfaces_esc = json_escape_string(interfaces ? interfaces : "");
    char *ip_addresses_esc = json_escape_string(ip_addresses ? ip_addresses : "");
    char *routing_esc = json_escape_string(routing ? routing : "");
    
    sprintf(json, 
        "{\n"
        "  \"interfaces\": \"%s\",\n"
        "  \"ip_addresses\": \"%s\",\n"
        "  \"routing\": \"%s\"\n"
        "}",
        interfaces_esc,
        ip_addresses_esc,
        routing_esc
    );
    
    // Clean up
    free(interfaces_esc);
    free(ip_addresses_esc);
    free(routing_esc);
    if (interfaces) free(interfaces);
    if (ip_addresses) free(ip_addresses);
    if (routing) free(routing);
    
    return json;
}

// Function to generate firmware information JSON
char* generate_firmware_json() {
    int exit_status;
    
    // Get firmware details
    char *firmware_info = execute_command("cat /etc/openwrt_release", &exit_status);
    
    // Get build date
    char *build_date = execute_command("ls -l --time-style=long-iso /bin/busybox | awk '{print $6}'", &exit_status);
    
    // Get architecture
    char *arch = execute_command("uname -m", &exit_status);
    
    // Format JSON
    char *json = malloc(4096);
    if (!json) {
        if (firmware_info) free(firmware_info);
        if (build_date) free(build_date);
        if (arch) free(arch);
        return NULL;
    }
    
    // Escape strings for JSON
    char *firmware_esc = json_escape_string(firmware_info ? firmware_info : "Unknown");
    char *build_date_esc = json_escape_string(build_date ? build_date : "Unknown");
    char *arch_esc = json_escape_string(arch ? arch : "Unknown");
    
    // Remove newlines
    if (build_date_esc) {
        char *newline = strchr(build_date_esc, '\n');
        if (newline) *newline = '\0';
    }
    
    if (arch_esc) {
        char *newline = strchr(arch_esc, '\n');
        if (newline) *newline = '\0';
    }
    
    sprintf(json, 
        "{\n"
        "  \"version\": \"%s\",\n"
        "  \"build_date\": \"%s\",\n"
        "  \"architecture\": \"%s\",\n"
        "  \"status\": \"stable\",\n"
        "  \"update_available\": false\n"
        "}",
        get_openwrt_version(),
        build_date_esc,
        arch_esc
    );
    
    // Clean up
    free(firmware_esc);
    free(build_date_esc);
    free(arch_esc);
    if (firmware_info) free(firmware_info);
    if (build_date) free(build_date);
    if (arch) free(arch);
    
    return json;
}

// Start MQTT broker
int start_mqtt_broker() {
    // In a real implementation, this would start the MQTT broker
    // Here we just simulate it
    mqtt_state.running = 1;
    mqtt_state.client_count = 0;
    mqtt_state.messages_published = 0;
    mqtt_state.messages_received = 0;
    
    return 1; // Success
}

// Stop MQTT broker
int stop_mqtt_broker() {
    // In a real implementation, this would stop the MQTT broker
    // Here we just simulate it
    mqtt_state.running = 0;
    
    return 1; // Success
}

// Function to handle API requests
void handle_api_request(int socket, const char *path) {
    char *json = NULL;
    int success = 0;
    
    // Extract the API endpoint
    if (strcmp(path, "/api/metrics") == 0) {
        // Generate metrics JSON
        json = generate_metrics_json();
        success = (json != NULL);
    }
    else if (strcmp(path, "/api/system") == 0) {
        // Generate system info JSON
        json = generate_system_json();
        success = (json != NULL);
    }
    else if (strcmp(path, "/api/network") == 0) {
        // Generate network info JSON
        json = generate_network_json();
        success = (json != NULL);
    }
    else if (strcmp(path, "/api/firmware") == 0) {
        // Generate firmware info JSON
        json = generate_firmware_json();
        success = (json != NULL);
    }
    else if (strcmp(path, "/api/mqtt/status") == 0) {
        // Generate MQTT status JSON
        json = generate_mqtt_status_json();
        success = (json != NULL);
    }
    else if (strcmp(path, "/api/mqtt/start") == 0) {
        // Start MQTT broker
        success = start_mqtt_broker();
        json = strdup("{ \"success\": true }");
    }
    else if (strcmp(path, "/api/mqtt/stop") == 0) {
        // Stop MQTT broker
        success = stop_mqtt_broker();
        json = strdup("{ \"success\": true }");
    }
    
    if (success && json) {
        // Prepare the response header
        char *response = malloc(strlen(json) + 256);
        if (!response) {
            free(json);
            char error_response[] = 
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: 35\r\n"
                "\r\n"
                "{\"error\":\"Memory allocation error\"}";
            
            send(socket, error_response, strlen(error_response), 0);
            return;
        }
        
        sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"  // Add CORS header for API
            "Content-Length: %zu\r\n"
            "\r\n"
            "%s", strlen(json), json);
        
        // Send the response
        send(socket, response, strlen(response), 0);
        
        // Clean up
        free(json);
        free(response);
    }
    else {
        // Unknown API endpoint or error
        char error_response[] = 
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"  // Add CORS header for API
            "Content-Length: 44\r\n"
            "\r\n"
            "{\"error\":\"The requested API was not found\"}";
        
        send(socket, error_response, strlen(error_response), 0);
    }
}

// Main function
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
    
    // Initialize metrics
    memset(&metrics, 0, sizeof(metrics));
    metrics.last_bandwidth_check = time(NULL);
    
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
    if (listen(server_fd, 10) < 0) {
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
        
        // Parse request line
        char method[16] = {0};
        char path[MAX_PATH_LENGTH] = {0};
        char protocol[16] = {0};
        
        sscanf(buffer, "%s %s %s", method, path, protocol);
        
        // Parse command if this is a GET request with command parameter
        char command[MAX_COMMAND_SIZE] = {0};
        char *cmd_output = NULL;
        int exit_status = 0;
        
        // Check if this is a query string
        char *query_string = strchr(path, '?');
        if (query_string) {
            *query_string = '\0';  // Terminate the path at the ?
            query_string++;  // Move to the character after ?
            
            // Parse the query parameters
            parse_query_params(query_string, command, sizeof(command));
            
            // Restore the path with query string for potential static file serving
            *(query_string - 1) = '?';
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
        
        // Handle based on the path
        if (strncmp(path, "/api/", 5) == 0) {
            // API request
            handle_api_request(new_socket, path);
        }
        else if (strncmp(path, "/css/", 5) == 0 || 
                 strncmp(path, "/js/", 4) == 0 || 
                 strncmp(path, "/img/", 5) == 0) {
            // Static file request (CSS, JS, images)
            handle_static_file(new_socket, path);
        }
        else {
            // Render the main template or handle other paths
            if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
                // Render the main template
                render_template(buffer, client_ip, command[0] ? command : NULL, cmd_output, exit_status);
                send(new_socket, buffer, strlen(buffer), 0);
            }
            else if (file_exists(path + 1)) {  // +1 to skip leading /
                // Serve static file
                handle_static_file(new_socket, path);
            }
            else {
                // Try to redirect to index.html
                render_template(buffer, client_ip, command[0] ? command : NULL, cmd_output, exit_status);
                send(new_socket, buffer, strlen(buffer), 0);
            }
        }
        
        // Free command output memory
        if (cmd_output) {
            free(cmd_output);
            cmd_output = NULL;
        }
        
        // Close the connection
        close(new_socket);
    }
    
    return 0;
}