#include "core_ctl.h"

// unix_sock_server.c
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <fcntl.h>
#include "gateway.h"

#define CONNECTION_TIMEOUT_SEC 30

#ifdef __APPLE__
    #define GATEWAY_LIB_PATH "./gateways/"
    #define LIB_EXT ".dylib"
    #define SOCKET_PATH "/tmp/corecdtl_ctl.sock"
#elif __linux__
    #define GATEWAY_LIB_PATH "./gateways/"
    #define LIB_EXT ".so"
    #define SOCKET_PATH "/run/corecdtl/corecdtl_ctl.sock"
#else
    #define GATEWAY_LIB_PATH "./gateways/"
    #define LIB_EXT ".so"
    #define SOCKET_PATH "./corecdtl_ctl.sock"
#endif

static int gateway_is_alive = 0;

static int create_and_bind_socket(void)
{
    int sockfd;
    struct sockaddr_un addr;

    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    unlink(SOCKET_PATH);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 1) < 0) {
        perror("listen");
        close(sockfd);
        return -1;
    }

    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    return sockfd;
}

static int handle_ctl_command(const char *command, char *response, size_t resp_size)
{
    char lib[128], action[64];
    memset(lib, 0, sizeof(lib));
    memset(action, 0, sizeof(action));

    if (sscanf(command, "%127s %63s", lib, action) != 2) {
        snprintf(response, resp_size, "Invalid command format.");
        return -1;
    }

    for (int i = 0; action[i]; i++) {
        action[i] = (char)tolower(action[i]);
    }

    char lib_path[256] = {0};
    snprintf(lib_path, sizeof(lib_path), "%s%s%s", GATEWAY_LIB_PATH, lib, LIB_EXT);

    int res = -1;

    if (strcmp(action, "start") == 0) {
        if (!gateway_is_alive) {
            res = gateway_init_lib(lib_path);
            if (res == GATEWAY_OK) {
                gateway_start_lib();
            }
        } else {
            res = GATEWAY_OK;
        }
        snprintf(response, resp_size, res == GATEWAY_OK ?
                 "Started '%s'" : "Failed to start '%s': %s", lib_path, gateway_status_to_str(res));
    }
    else if (strcmp(action, "stop") == 0) {
        if (gateway_is_alive) {
            gateway_stop_lib();
            gateway_close_lib();
            res = GATEWAY_OK;
        } else {
            res = -1;
        }
        snprintf(response, resp_size, res == GATEWAY_OK ?
                 "Stopped '%s'" : "Failed to stop '%s'", lib_path);
    }
    else if (strcmp(action, "restart") == 0) {
        if (gateway_is_alive) {
            gateway_restart_lib();
            res = GATEWAY_OK;
        } else {
            res = gateway_init_lib(lib_path);
            if (res == GATEWAY_OK) {
                gateway_start_lib();
            }
        }
        snprintf(response, resp_size, res == GATEWAY_OK ?
                 "Restarted '%s'" : "Failed to restart '%s': %s", lib_path, gateway_status_to_str(res));
    }
    else if (strcmp(action, "status") == 0) {
        const char *status = gateway_is_alive ? "Running" : "Stopped";
        snprintf(response, resp_size, "Status of '%s': %s", lib_path, status);
        res = GATEWAY_OK;
    }
    else {
        snprintf(response, resp_size, "Unknown action: '%s'", action);
    }


    return res;
}

int unix_sock_ctl_run(void)
{
    int listen_fd = create_and_bind_socket();
    if (listen_fd < 0) {
        fprintf(stderr, "Failed to create/bind unix socket\n");
        return -1;
    }

    fd_set readfds;
    struct timeval timeout;
    int client_fd = -1;

    timeout.tv_sec = CONNECTION_TIMEOUT_SEC;
    timeout.tv_usec = 0;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(listen_fd, &readfds);

        int sel = select(listen_fd + 1, &readfds, NULL, NULL, &timeout);
        if (sel == -1) {
            perror("select");
            break;
        } else if (sel == 0) {
            printf("No connection within timeout period. Exiting...\n");
            break;
        }

        if (FD_ISSET(listen_fd, &readfds)) {
            client_fd = accept(listen_fd, NULL, NULL);
            if (client_fd < 0) {
                perror("accept");
                break;
            }
            printf("CLI connected\n");

            timeout.tv_sec = CONNECTION_TIMEOUT_SEC;
            timeout.tv_usec = 0;

            while (1) {
                FD_ZERO(&readfds);
                FD_SET(client_fd, &readfds);

                sel = select(client_fd + 1, &readfds, NULL, NULL, &timeout);
                if (sel == -1) {
                    perror("select client_fd");
                    break;
                } else if (sel == 0) {
                    printf("No data or activity from CLI for %d seconds. Closing connection.\n", CONNECTION_TIMEOUT_SEC);
                    break;
                }

                if (FD_ISSET(client_fd, &readfds)) {
                    char buffer[256];
                    ssize_t r = read(client_fd, buffer, sizeof(buffer) - 1);
                    if (r == 0 || r < 0) {
                        break;
                    } else {
                        buffer[r] = '\0';

                        char resp[256];
                        memset(resp, 0, sizeof(resp));

                        handle_ctl_command(buffer, resp, sizeof(resp));
                        write(client_fd, resp, strlen(resp));
                    }
                }
                timeout.tv_sec = CONNECTION_TIMEOUT_SEC;
                timeout.tv_usec = 0;
            }

            close(client_fd);
            client_fd = -1;

            timeout.tv_sec = CONNECTION_TIMEOUT_SEC;
            timeout.tv_usec = 0;
        }
    }

    close(listen_fd);
    unlink(SOCKET_PATH);
    return 0;
}
