#ifndef OPENWRT_MGMT_H
#define OPENWRT_MGMT_H

#include <stddef.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
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
#include <sys/wait.h>

#define DEFAULT_PORT 5000
#define BUFFER_SIZE 65536
#define MAX_COMMAND_SIZE 2048
#define MAX_HISTORY 10
#define MAX_PATH_LENGTH 256
#define TEMPLATE_MAX_SIZE 65536

typedef struct {
    float cpu_usage;
    unsigned long total_memory;
    unsigned long used_memory;
    float memory_usage;
    unsigned long total_storage;
    unsigned long free_storage;
    unsigned long used_storage;
    float storage_usage;
    float download_rate;
    float upload_rate;
    unsigned long rx_bytes;
    unsigned long tx_bytes;
    unsigned long last_rx_bytes;
    unsigned long last_tx_bytes;
    time_t last_bandwidth_check;
    int internet_connected;
    int ultima_server_connected;
} system_metrics;

typedef struct {
    int running;
    int client_count;
    int messages_published;
    int messages_received;
} mqtt_status;

typedef struct {
    char *ip_address;
    int port;
    char *web_root;
    char *template_dir;
} server_config;

int server_init(server_config *config);

void server_run(int server_fd);

void server_cleanup(int server_fd);




#endif 
