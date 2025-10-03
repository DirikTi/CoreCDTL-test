#include "runtime.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/errno.h>

#include "core_ctl.h"


#ifdef __cplusplus
extern "C" {
#endif

    void* create_api_subscribe_stub(unsigned int plugin_id, int (*real_fn)(unsigned int, const char*, void*, void*));

#ifdef __cplusplus
}
#endif

static volatile int g_run = 1;
static volatile int g_ctl_run = 0;

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;

void signal_cli_handle(int sig, siginfo_t *info, void *ucontext)
{
    if (g_ctl_run != 1) {
        pthread_mutex_lock(&g_mutex);
        g_ctl_run = 1;
        pthread_cond_signal(&g_cond);
        pthread_mutex_unlock(&g_mutex);
    }

}
__attribute__((visibility("hidden")))
static void *core_loop(void *arg)
{
    while (g_run) {
        pthread_mutex_lock(&g_mutex);

        while (g_ctl_run == 0 && g_run) {
            pthread_cond_wait(&g_cond, &g_mutex);
        }

        if (!g_run) {
            pthread_mutex_unlock(&g_mutex);
            break;
        }

        pthread_mutex_unlock(&g_mutex);

        unix_sock_ctl_run();
    }
    return NULL;
}

__attribute__((visibility("default")))
int corecdtl_init(void)
{
    if (core_init() != 0) {
        fprintf(stderr, "Core init failed\n");
        return 1;
    }

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = signal_cli_handle;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);

    pthread_t tid;
    int rc = pthread_create(&tid, NULL, core_loop, NULL);
    if (rc != 0) {
        fprintf(stderr, "corecdtl_init: pthread_create failed (%d)\n", rc);
        return 1;
    }
    pthread_detach(tid);

    return 0;
}

__attribute__((visibility("default")))
char* corecdtl_api_version(void)
{
    return CORECDTL_API_VERSION;
}

__attribute__((visibility("default")))
char* corecdtl_abi_version(void)
{
    return CORECDTL_ABI_VERSION;
}
