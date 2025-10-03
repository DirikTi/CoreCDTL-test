#include "gateway_dispatcher.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "log.h"

#define MSG_SIZE        1024
#define MAX_MSG_COUNT   64
#define MAX_TOTAL_SIZE  (MSG_SIZE * MAX_MSG_COUNT)
#define FLUSH_INTERVAL_MS 100


static pthread_mutex_t gateway_d_flush_mutex = PTHREAD_MUTEX_INITIALIZER;

static char *arena_d_instance = NULL;
static size_t current_offset = 0;

static pthread_t dispatcher_thread;
static int dispatcher_running = 0;

static dispatcher_on_data_fn gateway_on_data_cb = NULL;

static void* dispatcher_loop(void *arg)
{
    while (dispatcher_running) {
        usleep(FLUSH_INTERVAL_MS * 1000); // ms -> µs

        pthread_mutex_lock(&gateway_d_flush_mutex);
        int has_data = (current_offset > 0);
        pthread_mutex_unlock(&gateway_d_flush_mutex);
        if (has_data) {
            gateway_d_flush();
        }
    }

    return NULL;
}

void gateway_dispatcher_init(dispatcher_on_data_fn on_data_fn)
{
    arena_d_instance = malloc(MAX_TOTAL_SIZE);
    current_offset = 0;

    gateway_on_data_cb = on_data_fn;

    dispatcher_running = 1;
    pthread_create(&dispatcher_thread, NULL, dispatcher_loop, NULL);
}


void gateway_dispatcher_close(void)
{
    dispatcher_running = 0;
    pthread_join(dispatcher_thread, NULL);

    free(arena_d_instance);
    arena_d_instance = NULL;
    current_offset = 0;

    gateway_on_data_cb = NULL;
}

void gateway_d_msg_add(const char *msg, size_t msg_len)
{
    if (!arena_d_instance || !msg || msg_len == 0) {
        core_log_warn("[gateway_dispatcher] Areana d_distance null.\n");
        return;
    }

    pthread_mutex_lock(&gateway_d_flush_mutex);

    if (current_offset + msg_len + 1 >= MAX_TOTAL_SIZE) {
        core_log_warn("[gateway_dispatcher] Buffer full, dropping message.\n");
        pthread_mutex_unlock(&gateway_d_flush_mutex);
        return;
    }

    memcpy(arena_d_instance + current_offset, msg, msg_len);
    current_offset += msg_len;

    arena_d_instance[current_offset++] = '\n';

    pthread_mutex_unlock(&gateway_d_flush_mutex);
}

void gateway_d_flush(void)
{
    if (!arena_d_instance || current_offset == 0) return;

    pthread_mutex_lock(&gateway_d_flush_mutex);

    char msg_buf[MAX_TOTAL_SIZE];
    memcpy(msg_buf, arena_d_instance, current_offset);

    gateway_on_data_cb(msg_buf, current_offset);

    current_offset = 0;

    pthread_mutex_unlock(&gateway_d_flush_mutex);
}
