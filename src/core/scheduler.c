#include "platform.h"
#include "scheduler.h"

#include <stdio.h>

#include "core_utils.h"
#include "crash_recovery.h"
#include "gateway.h"
#include "log.h"

#if defined(OS_LINUX)
#include <pthread.h>
#endif

#if OS_WINDOWS
#include <windows.h>
#endif

#include <stdlib.h>
#include <time.h>

typedef struct scheduler_timer_s {
    int id;
    uint64_t due_ms;
    uint64_t interval_ms;
    sched_cb_t cb;
    plugin_id_t plugin_id;
    void *user;
    int repeating;
    struct scheduler_timer_s *next;
} scheduler_timer_t;

__thread sigjmp_buf scheduler_thread_jmp_env;
__thread volatile error_context_t scheduler_error_ctx = {
    .marker = SCHEDULER_ERROR_MARKER
};

static api_type_t self_api_type = SCHEDULER_API;

static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cv = PTHREAD_COND_INITIALIZER;
static scheduler_timer_t *g_timers = NULL;
static int g_next_id = 1;
static int g_running = 0;
static pthread_t g_thr;

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static void insert_timer(scheduler_timer_t *t)
{
    if (!g_timers || t->due_ms < g_timers->due_ms) {
        t->next = g_timers;
        g_timers = t;
        return;
    }

    scheduler_timer_t *it = g_timers;
    
    while (it->next && it->next->due_ms <= t->due_ms)
        it = it->next;
    
    t->next = it->next;
    it->next = t;
}

__attribute__((noinline))
static void *sched_thread(void *arg)
{
    (void)arg;

    pthread_mutex_lock(&g_mtx);

    error_context_t local_ctx = scheduler_error_ctx;
    asm volatile("" : : "r"(&local_ctx) : "memory");
    
    while (g_running) {

        uint64_t now = now_ms();
        
        if (!g_timers) {
            // wait indefinitely
            pthread_cond_wait(&g_cv, &g_mtx);
            continue;
        }
        // compute wait time until next timer
        uint64_t due = g_timers->due_ms;
        if (due <= now)
        {
            // pop the head
            scheduler_timer_t *t = g_timers;
            g_timers = t->next;

            sched_cb_t cb = t->cb;
            void *user = t->user;
            int repeating = t->repeating;
            uint64_t interval = t->interval_ms;
            pthread_mutex_unlock(&g_mtx);

            if (sigsetjmp(scheduler_thread_jmp_env, 1) != 0) {
                // Get Error
                add_scheduler_critical_error(t->plugin_id, t->id,
                CRITICAL_ERROR_QUEUE_SOURCE_SCHEDULER, scheduler_error_ctx);
            } else {
                cb(NULL, 0, user);
            }


            pthread_mutex_lock(&g_mtx);
            if (repeating && g_running) {
                t->due_ms = now_ms() + interval;
                t->next = NULL;
                insert_timer(t);
            }
            else
                free(t);

            continue;
        }
        else {
            // timed wait
            struct timespec ts;
            uint64_t wait_ms = due - now;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += (wait_ms / 1000);
            ts.tv_nsec += (long)(wait_ms % 1000) * 1000000L;
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000L;
            }
            pthread_cond_timedwait(&g_cv, &g_mtx, &ts);
        }
    }

    pthread_mutex_unlock(&g_mtx);
    return NULL;
}

int scheduler_init(void)
{
    g_running = 1;
    if (pthread_create(&g_thr, NULL, sched_thread, NULL) != 0) {
        g_running = 0;
        return -1;
    }

    return 0;
}

void scheduler_shutdown(void)
{
    pthread_mutex_lock(&g_mtx);
    
    g_running = 0;

    pthread_cond_broadcast(&g_cv);
    pthread_mutex_unlock(&g_mtx);
    
    pthread_join(g_thr, NULL);
    
    // cleanup timers
    pthread_mutex_lock(&g_mtx);
    scheduler_timer_t *it = g_timers;

    while (it) {
        scheduler_timer_t *n = it->next;
        free(it);
        it = n;
    }

    g_timers = NULL;
    pthread_mutex_unlock(&g_mtx);
}

int scheduler_after_ms(plugin_id_t plugin_id, uint64_t ms, sched_cb_t cb, void *user)
{
    if (!cb)
        return -1;

    scheduler_timer_t *t = (scheduler_timer_t *)malloc(sizeof(scheduler_timer_t));

    if (!t)
        return -1;

    t->id = g_next_id++;
    t->interval_ms = 0;
    t->due_ms = now_ms() + ms;
    t->cb = cb;
    t->user = user;
    t->repeating = 0;
    t->next = NULL;
    t->plugin_id = plugin_id;

    pthread_mutex_lock(&g_mtx);
    
    insert_timer(t);
    
    pthread_cond_broadcast(&g_cv);
    pthread_mutex_unlock(&g_mtx);

    if (g_gateway_connected_flag) {
        char msg[GATEWAY_DTLS_MSG_LEN];
        snprintf(msg, GATEWAY_DTLS_MSG_LEN, "[subs_after] %d %d %lld [ms]", plugin_id, t->id, ms);
        gateway_msg_send(self_api_type, msg);
    }
    
    return t->id;
}

int scheduler_every_ms(plugin_id_t plugin_id, uint64_t ms, sched_cb_t cb, void *user)
{
    if (!cb || ms == 0)
        return -1;

    scheduler_timer_t *t = (scheduler_timer_t *)malloc(sizeof(scheduler_timer_t));

    if (!t)
        return -1;

    t->id = g_next_id++;
    t->interval_ms = ms;
    t->due_ms = now_ms() + ms;
    t->cb = cb;
    t->user = user;
    t->repeating = 1;
    t->next = NULL;
    t->plugin_id = plugin_id;

    pthread_mutex_lock(&g_mtx);
    
    insert_timer(t);
    
    pthread_cond_broadcast(&g_cv);
    pthread_mutex_unlock(&g_mtx);

    if (g_gateway_connected_flag) {
        char msg[GATEWAY_DTLS_MSG_LEN];
        snprintf(msg, GATEWAY_DTLS_MSG_LEN, "[subs_every] %d %d %lld [ms]", plugin_id, t->id, ms);
        gateway_msg_send(self_api_type, msg);
    }

    return t->id;
}

int scheduler_cancel(plugin_id_t plugin_id, int id)
{
    if (id <= 0)
        return -1;

    pthread_mutex_lock(&g_mtx);
    
    scheduler_timer_t *it = g_timers;
    scheduler_timer_t *prev = NULL;

    while (it) {
        if (it->id == id && it->plugin_id == plugin_id)
        {
            if (prev)
                prev->next = it->next;
            else
                g_timers = it->next;
            free(it);
            pthread_mutex_unlock(&g_mtx);
            return 0;
        }
        prev = it;
        it = it->next;
    }
    pthread_mutex_unlock(&g_mtx);

    if (g_gateway_connected_flag) {
        char msg[GATEWAY_DTLS_MSG_LEN];
        snprintf(msg, GATEWAY_DTLS_MSG_LEN, "[cancel] %d %d", plugin_id, id);
        gateway_msg_send(self_api_type, msg);
    }

    return -1;
}

int scheduler_get_list(char *out_buf, size_t buf_len)
{
    scheduler_timer_t *it = g_timers;

    snprintf(out_buf, buf_len, "[on_data] [scheduler]\n[getall]\n");

    while (it) {
        char temp[128];
        if (it->repeating) {
            snprintf(temp, sizeof(temp), "[every] %d %d %lld\n",
                     it->id, it->plugin_id, it->interval_ms);
        } else {
            snprintf(temp, sizeof(temp), "[after] %d %d %lld\n",
                     it->id, it->plugin_id, it->interval_ms);
        }

        size_t current_len = strlen(out_buf);
        size_t remaining = buf_len - current_len - 1;

        if (remaining > 0) {
            strncat(out_buf, temp, remaining);
        }

        it = it->next;
    }

    return 0;
}

