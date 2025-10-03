/**
 * core/log.c
 * ------------
 * Core logging system for the project.
 * 
 * Features:
 * - Thread-safe logging to stderr
 * - Optional file logging based on compile-time flags
 * - Supports levels: DEBUG, INFO, WARN, ERROR
 * - ANSI color output for terminal
 *
 * Usage:
 *   core_log_debug("Debug message: %d", value);
 *   core_log_error("Error message: %s", error_str);
 *
 * Author: Ertuğrul Dirik (Asikus)
 * Created: 2025-08-22
 */

#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "plugin_manager.h"

#include "core_utils.h"
#include "crash_recovery.h"
#include "gateway.h"

#define MAX_CRITICAL_ERROR_QUEUE_SIZE 63
#define CRITICAL_ERROR_SLEEP_TIME 1000U

void process_all_critical_errors(void);
static void write_critical_error_to_file(const char *msg);

static pthread_mutex_t g_log_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_critical_error_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_error_file_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_critical_error_cond = PTHREAD_COND_INITIALIZER;

static char *g_critical_error_queues[MAX_CRITICAL_ERROR_QUEUE_SIZE];
static uint32_t g_critical_error_bitmap = 0;

static api_type_t self_api_type = CRASH_API;

static const char *LEVEL_TAGS[] = {
	DEBUG_TEXT,
    INFO_TEXT,
    WARN_TEXT,
    ERROR_TEXT,
    DEFAULT_TEXT,
};

static pthread_t g_critical_error_thread;

static void *critical_error_consumer_thread(void *arg)
{
    (void)arg;

    while (1) {
        pthread_mutex_lock(&g_critical_error_mtx);

        while (g_critical_error_bitmap == 0) {
            pthread_cond_wait(&g_critical_error_cond, &g_critical_error_mtx);
        }

        for (int i = 0; i < MAX_CRITICAL_ERROR_QUEUE_SIZE; ++i) {
            if (g_critical_error_bitmap & (1U << i)) {
                char *msg = g_critical_error_queues[i];
                g_critical_error_queues[i] = NULL;
                g_critical_error_bitmap &= ~(1U << i);

                pthread_mutex_unlock(&g_critical_error_mtx);

                if (msg) {
                    write_critical_error_to_file(msg);
                    free(msg);
                }

                pthread_mutex_lock(&g_critical_error_mtx);
            }
        }

        pthread_mutex_unlock(&g_critical_error_mtx);
    }

    return NULL;
}


void core_log_init(void)
{
    mkdir(CORE_LOG_DIR, 0700);

    pthread_create(&g_critical_error_thread, NULL, critical_error_consumer_thread, NULL);
    pthread_detach(g_critical_error_thread);
}

// Helper: write logs into file
static void log_to_file(const char *level, const char *ts, const char *fmt, va_list args)
{
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s.log", CORE_LOG_DIR, level);
    FILE *f = fopen(filepath, "a");
    if (!f) return;
    fprintf(f, "[%s] ", ts);
    vfprintf(f, fmt, args);
    fprintf(f, "\n");
    fclose(f);
}

void plugin_log_internal(size_t level, const char *plugin_name, const char *fmt, ...)
{
    const char *tag = DEFAULT_TEXT;
    if (level >= DEBUG && level <= ERROR) {
        tag = LEVEL_TAGS[level];
    }

    time_t t = time(NULL);
    struct tm tmv;
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif

    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);

    pthread_mutex_lock(&g_log_mtx);

    char plugin_label[150];
    snprintf(plugin_label, sizeof(plugin_label), PLUGIN_LABEL_FORMAT, plugin_name);

    fprintf(stderr, "%s%s\n", plugin_label, tag);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n[\033[90m%s\033[0m]\n\n", ts);

    // FILE OUTPUT
    va_list ap2;
    va_start(ap2, fmt);
    if (level == ERROR) {
        log_to_file("error", ts, fmt, ap2); // always write errors
    }
    else if (level == WARN) {
        log_to_file("warn", ts, fmt, ap2);
    }
    else if (level == INFO) {
        log_to_file("info", ts, fmt, ap2);
    }
#ifdef CORE_LOG_DEBUG
    else if (level == DEBUG) {
        log_to_file("debug", ts, fmt, ap2);
    }
#endif
    va_end(ap2);

    pthread_mutex_unlock(&g_log_mtx);
}

// Internal function used by all wrappers
void core_log_internal(size_t level, const char *fmt, ...)
{

    const char *tag = DEFAULT_TEXT;
    if (level >= DEBUG && level <= ERROR) {
        tag = LEVEL_TAGS[level];
    }

    time_t t = time(NULL);
    struct tm tmv;
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif

    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);

    pthread_mutex_lock(&g_log_mtx);

    fprintf(stderr, "%s%s\n", CORE_TEXT, tag);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n[\033[90m%s\033[0m]\n\n", ts);

    // FILE OUTPUT
    va_list ap2;
    va_start(ap2, fmt);
    if (level == ERROR) {
        log_to_file("error", ts, fmt, ap2); // always write errors
    }
#ifdef CORE_LOG_WARN
    else if (level == WARN) {
        log_to_file("warn", ts, fmt, ap2);
    }
#endif
#ifdef CORE_LOG_INFO
    else if (level == INFO) {
        log_to_file("info", ts, fmt, ap2);
    }
#endif
#ifdef CORE_LOG_DEBUG
    else if (level == DEBUG) {
        log_to_file("debug", ts, fmt, ap2);
    }
#endif
    va_end(ap2);

    pthread_mutex_unlock(&g_log_mtx);
}

static inline void mark_slot_used(int index)
{
    g_critical_error_bitmap |= (1U << index);
}

static void write_critical_error_to_file(const char *msg)
{
    pthread_mutex_lock(&g_error_file_mtx);

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "./%s/%s", CORE_LOG_DIR, CRITICAL_ERROR_LOG_FILE);
    FILE *f = fopen(filepath, "a");
    if (f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    } else {
        core_log_error("Failed to write critical error log file: %s", msg);
    }

    pthread_mutex_unlock(&g_error_file_mtx);
}

void add_event_bus_critical_error(const plugin_id_t plugin_id, const char *name,
    log_critical_error_queue_source_t log_critical_error_queue_source, const error_context_t error_ctx)
{
    pthread_mutex_lock(&g_critical_error_mtx);
    int slot = -1;
    for (int i = 0; i < MAX_CRITICAL_ERROR_QUEUE_SIZE; ++i) {
        if (!(g_critical_error_bitmap & (1U << i))) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        pthread_mutex_unlock(&g_critical_error_mtx);
        core_log_warn("Critical error list has no slot");
        return;
    }

    char critical_error_buffer[255];

    const char *plugin_name = plugin_get_name(plugin_id);

    time_t t = time(NULL);
    struct tm tmv;
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif

    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);

    snprintf(
        critical_error_buffer,
        sizeof(critical_error_buffer),
        "[%s] [%s] %d [%s] %d %d %p",
        ts,
        plugin_name,
        log_critical_error_queue_source,
        name,
        error_ctx.signal,
        error_ctx.signal_code,
        error_ctx.fault_address
    );

    if (g_gateway_connected_flag) {
        char msg[GATEWAY_DTLS_MSG_LEN];
        memcpy(msg, critical_error_buffer, GATEWAY_DTLS_MSG_LEN);
        gateway_msg_send(self_api_type, msg);
    }

    g_critical_error_queues[slot] = strdup(critical_error_buffer);

    mark_slot_used(slot);
    pthread_cond_signal(&g_critical_error_cond);
    pthread_mutex_unlock(&g_critical_error_mtx);
}

void add_scheduler_critical_error(const plugin_id_t plugin_id, int timer_id,
    log_critical_error_queue_source_t log_critical_error_queue_source, const error_context_t error_ctx)
{
    pthread_mutex_lock(&g_critical_error_mtx);
    int slot = -1;
    for (int i = 0; i < MAX_CRITICAL_ERROR_QUEUE_SIZE; ++i) {
        if (!(g_critical_error_bitmap & (1U << i))) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        pthread_mutex_unlock(&g_critical_error_mtx);
        core_log_warn("Critical error list has no slot");
        return;
    }

    char critical_error_buffer[255];

    const char *plugin_name = plugin_get_name(plugin_id);

    time_t t = time(NULL);
    struct tm tmv;
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif

    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);

    snprintf(
        critical_error_buffer,
        sizeof(critical_error_buffer),
        "[%s] [%s] %d %d %d %d %p",
        ts,
        plugin_name,
        log_critical_error_queue_source,
        timer_id,
        error_ctx.signal,
        error_ctx.signal_code,
        error_ctx.fault_address
    );

    if (g_gateway_connected_flag) {
        char msg[GATEWAY_DTLS_MSG_LEN];
        memcpy(msg, critical_error_buffer, GATEWAY_DTLS_MSG_LEN);
        gateway_msg_send(self_api_type, msg);
    }

    g_critical_error_queues[slot] = strdup(critical_error_buffer);

    mark_slot_used(slot);
    pthread_cond_signal(&g_critical_error_cond);
    pthread_mutex_unlock(&g_critical_error_mtx);
}
