#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "art.h"
#include <sys/socket.h>
#include <sys/un.h>

#define CORECDTL_API_VERSION 1
#define CORECDTL_ABI_VERSION 1

#ifdef __APPLE__
    #define PID_FILE_PATH  "/tmp/corecdtl.pid"
#define SOCKET_PATH "/tmp/corecdtl_ctl.sock"
#elif __linux__
    #define PID_FILE_PATH  "/run/corecdtl/corecdtl.pid"
    #define SOCKET_PATH "/run/corecdtl/corecdtl_ctl.sock"
#else
    #define PID_FILE_PATH  "./corecdtl.pid"
#endif

static int g_connected = 0;
static int g_sock_fd = -1;

pid_t read_pid_from_file(void);

void print_help(void)
{
    printf("\n");
    printf("Available commands:\n");
    printf("  connect               Connect to the daemon (☣ Enter the matrix)\n");
    printf("  show gateways             List all available libraries in the 'libs/' folder\n");
    printf("  status gateway           Show running status of all loaded libraries\n");
    printf("  gateway <name> start      Load and start the specified gateway\n");
    printf("  gateway <name> stop       Stop and unload the specified gateway\n");
    printf("  gateway <name> restart    Restart the specified gateway\n");
    printf("  gateway <name> status     Show status of the specified gateway\n");
    printf("  help                  Show this help message\n");
    printf("  exit                  Exit the CTL\n");
    printf("\n");
}

void show_gateways(void)
{
    DIR *dir = opendir("./gateways");
    if (!dir) {
        perror("opendir");
        printf("Cannot open ./gateways directory\n");
        return;
    }

    struct dirent *entry;
    int found = 0;

    printf("Available gateways:\n");

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            if (strstr(entry->d_name, ".so") || strstr(entry->d_name, ".dylib")) {
                printf("  - %s\n", entry->d_name);
                found = 1;
            }
        }
    }

    closedir(dir);

    if (!found) {
        printf("  No shared libraries found in ./libs/\n");
    }
}

int connect_to_daemon(void)
{
    if (g_connected) {
        printf("Already connected.\n");
        return 0;
    }

    pid_t daemon_pid = read_pid_from_file();
    printf("PID = %d\n", daemon_pid);
    if (daemon_pid <= 0 || kill(daemon_pid, SIGUSR1) == -1) {
        perror("kill(SIGUSR1)");
        return -1;
    }

    printf("☁ Connecting to daemon...\n");
    usleep(300 * 1000);

    struct sockaddr_un addr;

    g_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_sock_fd == -1) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    for (int i = 0; i < 10; ++i) {
        if (connect(g_sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            printf("☣ Entering the matrix... Connected to daemon.\n");
            g_connected = 1;
            return 0;
        }
        usleep(100 * 1000);
    }

    perror("connect");
    close(g_sock_fd);
    g_sock_fd = -1;
    return -1;
}

int send_command(const char* command)
{
    if (!g_connected) {
        fprintf(stderr, "Not connected. Use 'connect' first.\n");
        return -1;
    }

    if (write(g_sock_fd, command, strlen(command)) < 0) {
        perror("write");
        return -1;
    }

    char buffer[256];
    ssize_t r = read(g_sock_fd, buffer, sizeof(buffer) - 1);
    if (r > 0) {
        buffer[r] = '\0';
        printf(">> %s\n", buffer);
    }

    return 0;
}

pid_t read_pid_from_file(void) {
    FILE *file = fopen(PID_FILE_PATH, "r");
    if (!file) {
        return -1;
    }

    pid_t pid;
    if (fscanf(file, "%d", &pid) != 1) {
        fclose(file);
        return -1;
    }

    fclose(file);
    return pid;
}

void printf_start_line()
{
    if (g_connected) {
        printf("(connected)> ");
    } else {
        printf("> ");
    }
}

void run_repl(void)
{
    char input[256];

    while (1) {
        printf_start_line();
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "exit") == 0) {
            printf("Disconnected. Bye!\n");
            break;
        }

        if (strcmp(input, "help") == 0) {
            print_help();
            continue;
        }

        if (strcmp(input, "connect") == 0) {
            connect_to_daemon();
            continue;
        }

        if (strcmp(input, "show gateways") == 0) {
            show_gateways();
            continue;
        }

        if (strncmp(input, "gateway ", 8) == 0) {
            if (!g_connected) {
                fprintf(stderr, "You must 'connect' before sending commands.\n");
                continue;
            }

            const char *actual_cmd = input + 8;
            send_command(actual_cmd);
            continue;
        }

        if (!g_connected) {
            fprintf(stderr, "You must 'connect' before sending commands.\n");
            continue;
        }

        send_command(input);
    }

    if (g_connected) {
        close(g_sock_fd);
    }
}

int main(void)
{
    system("clear");
    print_ascii_art();
    printf("Type 'help' for available commands.\n");

    run_repl();
    return 0;
}
