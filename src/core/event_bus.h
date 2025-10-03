#ifndef CORE_EVENT_BUST_H
#define CORE_EVENT_BUST_H

#include <stddef.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdint.h>
#include "core_utils.h"

#define MAX_INIT_EVENT_BUS_THREADS 5
#define MAX_TASKS 100

extern __thread sigjmp_buf event_thread_jmp_env;
#define EVENT_BUS_ERROR_MARKER 0xDEADBEEFCAFEBABEULL

typedef void (*bus_cb_t)(const void* data, size_t len, void* user);

struct p_thread_pool {
    pthread_t **thread_ptrs;
    size_t thread_max_count;
    size_t thread_count;
    uint64_t thread_map_mask;
};

typedef struct sub_s {
    char *event;
    plugin_id_t plugin_id;
    event_id_t event_id;
    bus_cb_t cb;
    void *user;
    struct sub_s *next;
} sub_t;

typedef struct {
    void *data;
    size_t len;
    sub_t *sub_ptr;
} bus_task_t;

typedef struct {
    bus_task_t tasks[MAX_TASKS];
    size_t head;
    size_t tail;
    size_t count;
    pthread_mutex_t mutex;
    pthread_cond_t cond_not_empty;
    pthread_cond_t cond_not_full;
} task_event_bus_queue_t;

int bus_init(void);
void bus_shutdown(void);
int bus_subscribe(plugin_id_t plugin_id, const char *event, bus_cb_t cb, void *user);
int bus_publish(const plugin_id_t plugin_id, const char *event, const void *data, size_t len);

void event_bus_get_list(char *out_buf, size_t out_buf_size);

typedef int (*api_bus_subscribe_fn)(const char* event, bus_cb_t cb, void* user);

#endif // CORE_EVENT_BUST_H
