#include <stdio.h>
#include <stdlib.h>
#include <ur_management.h>

int main(int argc, char *argv[]) {
    server_config config = {
        .ip_address = "0.0.0.0",
        .port = DEFAULT_PORT,
        .web_root = "public",
        .template_dir = "templates"
    };

    // Parse command line arguments
    if (argc > 1) {
        config.ip_address = argv[1];
        if (argc > 2) {
            config.port = atoi(argv[2]);
        }
    }

    int server_fd = server_init(&config);
    if (server_fd < 0) {
        fprintf(stderr, "Failed to initialize server\n");
        return EXIT_FAILURE;
    }

    server_run(server_fd);
    server_cleanup(server_fd);

    return EXIT_SUCCESS;
}
