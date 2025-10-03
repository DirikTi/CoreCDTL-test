#include "event_bus.h"
#include "log.h"
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdio.h>

#include "platform.h"
#include "../utils/utils_string.h"
#include "core_api.h"
#include "crash_recovery.h"
#include "gateway.h"
#include "plugin_manager.h"

static sub_t *g_head = NULL;
static pthread_mutex_t sub_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_bus_shut_down = 0;
static api_type_t self_api_type = EVENT_BUS_API;
static int event_count = 1;

__thread sigjmp_buf event_thread_jmp_env;
__thread volatile error_context_t event_bus_error_ctx = {
    .marker = EVENT_BUS_ERROR_MARKER
};

struct p_thread_pool pthread_pool = {
    .thread_ptrs = NULL,
    .thread_max_count = MAX_INIT_EVENT_BUS_THREADS,
    .thread_count = 0,
    .thread_map_mask = 0
};

task_event_bus_queue_t g_task_event_bus_queue_t;

void *bus_worker_thread(void *arg);

int bus_init(void)
{
    pthread_pool.thread_ptrs = malloc(sizeof(pthread_t*) * MAX_INIT_EVENT_BUS_THREADS);
    if (!pthread_pool.thread_ptrs) {
        core_log_error("Could not allocate bus thread pool");
        return -1;
    }

    for (int i = 0; i < MAX_INIT_EVENT_BUS_THREADS; i++) {
        pthread_t *thread = malloc(sizeof(pthread_t));
        if (!thread) return -1;

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

        if (pthread_create(thread, &attr, &bus_worker_thread, NULL) != 0) {
            free(thread);
            return -1;
        }

        pthread_attr_destroy(&attr);
        pthread_pool.thread_ptrs[i] = thread;
        pthread_pool.thread_count++;
        pthread_pool.thread_map_mask |= (1ULL << i);
    }

    g_task_event_bus_queue_t.head = 0;
    g_task_event_bus_queue_t.tail = 0;
    g_task_event_bus_queue_t.count = 0;
    pthread_mutex_init(&g_task_event_bus_queue_t.mutex, NULL);
    pthread_cond_init(&g_task_event_bus_queue_t.cond_not_empty, NULL);
    pthread_cond_init(&g_task_event_bus_queue_t.cond_not_full, NULL);

    return 0;
}

void bus_shutdown(void)
{
    pthread_mutex_lock(&g_task_event_bus_queue_t.mutex);
    g_bus_shut_down = 1;

    pthread_cond_broadcast(&g_task_event_bus_queue_t.cond_not_empty);
    pthread_mutex_unlock(&g_task_event_bus_queue_t.mutex);

    for (size_t i = 0; i < pthread_pool.thread_count; i++) {
        pthread_join(*pthread_pool.thread_ptrs[i], NULL);
        free(pthread_pool.thread_ptrs[i]);
    }
    free(pthread_pool.thread_ptrs);
    pthread_pool.thread_ptrs = NULL;
    pthread_pool.thread_count = 0;

    for (size_t i = 0; i < g_task_event_bus_queue_t.count; i++) {
        size_t index = (g_task_event_bus_queue_t.head + i) % MAX_TASKS;
        free(g_task_event_bus_queue_t.tasks[index].data);
    }

    pthread_mutex_destroy(&g_task_event_bus_queue_t.mutex);
    pthread_cond_destroy(&g_task_event_bus_queue_t.cond_not_empty);
    pthread_cond_destroy(&g_task_event_bus_queue_t.cond_not_full);

    pthread_mutex_lock(&sub_mutex);
    sub_t *it = g_head;
    while (it) {
        sub_t *n = it->next;
        free(it->event);
        free(it);
        it = n;
    }
    g_head = NULL;
    pthread_mutex_unlock(&sub_mutex);

    pthread_mutex_destroy(&sub_mutex);

    event_count = 1;
}

int bus_subscribe(plugin_id_t plugin_id, const char *event, bus_cb_t cb, void *user)
{
    if (!event) return BUS_SUB_ERR_INVALID_EVENT;
    if (!cb) return BUS_SUB_ERR_INVALID_CB;

    sub_t *s = malloc(sizeof(sub_t));
    if (!s) return BUS_SUB_ERR_ALLOC_FAILED;

    s->event = strdup(event);
    if (!s->event) {
        free(s);
        return BUS_SUB_ERR_STRDUP_FAILED;
    }

    s->cb = cb;
    s->user = user;
    s->plugin_id = plugin_id;
    s->event_id = event_count++;

    pthread_mutex_lock(&sub_mutex);
    s->next = g_head;
    g_head = s;
    pthread_mutex_unlock(&sub_mutex);

    if (g_gateway_connected_flag) {
        char msg[GATEWAY_DTLS_MSG_LEN];
        snprintf(msg, GATEWAY_DTLS_MSG_LEN, "[subscribe] %d [%s]", plugin_id, event);
        gateway_msg_send(self_api_type, msg);
    }

    return 0;
}


static int enqueue_task(bus_task_t task)
{
    pthread_mutex_lock(&g_task_event_bus_queue_t.mutex);

    while (g_task_event_bus_queue_t.count == MAX_TASKS && !g_bus_shut_down) {
        pthread_cond_wait(&g_task_event_bus_queue_t.cond_not_full,
                          &g_task_event_bus_queue_t.mutex);
    }

    if (g_bus_shut_down) {
        pthread_mutex_unlock(&g_task_event_bus_queue_t.mutex);
        return -1;
    }

    g_task_event_bus_queue_t.tasks[g_task_event_bus_queue_t.tail] = task;
    g_task_event_bus_queue_t.tail = (g_task_event_bus_queue_t.tail + 1) % MAX_TASKS;
    g_task_event_bus_queue_t.count++;

    pthread_cond_signal(&g_task_event_bus_queue_t.cond_not_empty);
    pthread_mutex_unlock(&g_task_event_bus_queue_t.mutex);
    return 0;
}

int bus_publish(const plugin_id_t plugin_id, const char *event, const void *data, size_t len)
{
    if (plugin_id == PLUGIN_ID_INVALID) return BUS_PUBLISH_ERR_INVALID_PLUGIN_ID;
    if (!data) return BUS_PUBLISH_ERR_INVALID_DATA;
    if (len == 0) return BUS_PUBLISH_ERR_INVALID_LENGTH;

    pthread_mutex_lock(&sub_mutex);

    const sub_t *it = g_head;
    while (it) {
        if (it->plugin_id == plugin_id && optimized_strcmp(it->event, event) == 0) {
            bus_task_t task;

            task.sub_ptr = (sub_t *)it;
            task.data = malloc(len);
            if (!task.data) {
                pthread_mutex_unlock(&sub_mutex);
                return BUS_PUBLISH_ERR_MALLOC_FAILED;
            }
            memcpy(task.data, data, len);
            task.len = len;

            enqueue_task(task);
            pthread_mutex_unlock(&sub_mutex);
            return 0; // Success
        }
        it = it->next;
    }

    pthread_mutex_unlock(&sub_mutex);

    if (g_gateway_connected_flag) {
        char msg[GATEWAY_DTLS_MSG_LEN];
        snprintf(msg, GATEWAY_DTLS_MSG_LEN, "[publish] %d [%s]", plugin_id, event);
        gateway_msg_send(self_api_type, msg);
    }
    return BUS_PUBLISH_ERR_EVENT_NOT_FOUND;
}

__attribute__((noinline))
void *bus_worker_thread(void *arg)
{
    (void)arg;

    error_context_t local_ctx = event_bus_error_ctx;
    asm volatile("" : : "r"(&local_ctx) : "memory");

    while (1) {
        pthread_mutex_lock(&g_task_event_bus_queue_t.mutex);

        while (g_task_event_bus_queue_t.count == 0 && !g_bus_shut_down) {
            pthread_cond_wait(&g_task_event_bus_queue_t.cond_not_empty,
                              &g_task_event_bus_queue_t.mutex);
        }

        if (g_bus_shut_down && g_task_event_bus_queue_t.count == 0) {
            pthread_mutex_unlock(&g_task_event_bus_queue_t.mutex);
            break;
        }

        bus_task_t task = g_task_event_bus_queue_t.tasks[g_task_event_bus_queue_t.head];

        g_task_event_bus_queue_t.head = (g_task_event_bus_queue_t.head + 1) % MAX_TASKS;
        g_task_event_bus_queue_t.count--;

        pthread_cond_signal(&g_task_event_bus_queue_t.cond_not_full);
        pthread_mutex_unlock(&g_task_event_bus_queue_t.mutex);

        // Error handler
        if (sigsetjmp(event_thread_jmp_env, 1) != 0) {
            // Logger add list enquque
            add_event_bus_critical_error(task.sub_ptr->plugin_id, task.sub_ptr->event,
                CRITICAL_ERROR_QUEUE_SOURCE_EVENT_BUS, event_bus_error_ctx);

            free(task.data);
        } else {
            if (LIKELY(task.sub_ptr && task.sub_ptr->cb)) {
                task.sub_ptr->cb(task.data, task.len, task.sub_ptr->user);
            }

            free((void *)task.data);
        }
    }

    return NULL;
}

void event_bus_get_list(char *out_buf, size_t out_buf_size)
{
    sub_t *it = g_head;

    snprintf(out_buf, out_buf_size, "[on_data] [event_bus]\n[getall]\n");

    while (it) {
        char temp[512];
        snprintf(temp, sizeof(temp), "[%s] %d\n", it->event, it->plugin_id);

        size_t current_len = strlen(out_buf);
        size_t remaining = out_buf_size - current_len - 1;

        if (remaining > 0) {
            strncat(out_buf, temp, remaining);
        }

        it = it->next;
    }
}
