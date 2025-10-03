#include "runtime.h"

#include <errno.h>

#include "log.h"
#include "event_bus.h"
#include "scheduler.h"
#include "../plugin/plugin_manager.h"
#include "../jit/llvm_jit.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "heapkit.h"
#include "jit_stub_generator.h"

// Linux/GCC/Clang visibility attribute
__attribute__((visibility("default"))) const core_api_t* core_get_api(void);
__attribute__((visibility("default"))) plugin_log_internal_func_t get_plugin_log_internal(void);

// __declspec(dllexport) const core_api_t* core_get_api(void);

/*
 * KISS
 *
 * Keep
 * It
 * Simple
 * Stupid
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#define PROT_RWX (PROT_READ | PROT_WRITE | PROT_EXEC)
#else
#define PROT_RWX (PROT_READ | PROT_WRITE | PROT_EXEC)
#endif

static core_api_t g_api;
static int g_inited = 0;

void *g_return_after_cb = NULL;

// Service registry (very simple name -> void* map)
typedef struct svc_s
{
    char *name;
    void *ptr;
    struct svc_s *next;
} svc_t;
static svc_t *g_svc = NULL;

static int ensure_pid_dir_exists(void);
static int write_pid_file(void);

int core_init(void)
{
    if (g_inited) {
        core_log_error("Already init Core");
        return 0;
    }

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        strcat(cwd, "/CoreCDTL");
        chdir(cwd);
    } else {
        core_log_error("Cannot get cwd");
        return 1;
    }

    if (ensure_pid_dir_exists() != 0) {
        return 2;
    }

    if (write_pid_file() != 0) {
        return 3;
    }

    crash_recovery_init();

    core_log_init();

    llvm_jit_init();
    plugin_init();
    llvm_jit_cleanup();

    core_log_info("Core init");

    if (bus_init() != 0) {
        core_log_error("Bus init failed");
        return 4;
    }


    if (scheduler_init() != 0) {
        core_log_error("Scheduler init failed");
        return 5;
    }
    
    g_inited = 1;
    return 0;
}

const core_api_t *core_get_api(void)
{
    return g_inited ? &g_api : NULL;
}

plugin_log_internal_func_t get_plugin_log_internal(void)
{
    return &plugin_log_internal;
}

static int ensure_pid_dir_exists(void)
{
    struct stat st = {0};

    if (stat(PID_DIR, &st) == -1) {
        if (mkdir(PID_DIR, 0755) == -1) {
            if (errno != EEXIST) {
                fprintf(stderr, "Error creating PID directory '%s': %s\n", PID_DIR, strerror(errno));
                return -1;
            }
        }
    } else {
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "'%s' exists but is not a directory\n", PID_DIR);
            return -1;
        }
    }
    return 0;
}

static int write_pid_file(void)
{
    FILE *f = fopen(PID_FILE_PATH, "w");
    if (!f) {
        fprintf(stderr, "Failed to open PID file '%s' for writing: %s\n", PID_FILE_PATH, strerror(errno));
        return -1;
    }
    fprintf(f, "%d\n", getpid());
    fclose(f);

    return 0;
}

void core_shutdown(void)
{
    core_log_info("Shutdown");
    svc_t *it = g_svc;
    while (it)
    {
        svc_t *n = it->next;
        free(it->name);
        free(it);
        it = n;
    }
    g_svc = NULL;

    scheduler_shutdown();
    bus_shutdown();

    unlink(FIFO_PATH);
    unlink(PID_FILE_PATH);

    g_inited = 0;
}
