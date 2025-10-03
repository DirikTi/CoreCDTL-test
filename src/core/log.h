#ifndef CORE_LOG_H
#define CORE_LOG_H

#include <stddef.h>

#include "core_utils.h"
#include "crash_recovery.h"

// COLOR TAGS
#define DEFAULT_TEXT  "\033[37;104m  LOG    \033[0m"
#define DEBUG_TEXT    "\033[1;97;45m  DEBUG  \033[0m"  // White word, purple background
#define INFO_TEXT     "\033[1;97;42m  INFO   \033[0m"  // White word, green background
#define WARN_TEXT     "\033[1;97;43m  WARN   \033[0m"  // White word, yellow background
#define ERROR_TEXT    "\033[1;97;41m  ERROR  \033[0m"  // White word, red background
#define CORE_TEXT     "\033[1;97;44m   CORE   \033[0m" // White word, blue background
#define PLUGIN_BG_COLOR "\033[45m"  // Purple background
#define PLUGIN_TEXT_COLOR "\033[1;97m" // Bold White word
#define RESET_COLOR   "\033[0m"

#define PLUGIN_LABEL_FORMAT PLUGIN_TEXT_COLOR PLUGIN_BG_COLOR "  %s  " RESET_COLOR

#define CORE_LOG_DIR "logs"
#define CRITICAL_ERROR_LOG_FILE "critical_errors.log"

enum LogLevels {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ERROR = 3,
    DEFAULT = 4
};

typedef enum {
    CRITICAL_ERROR_QUEUE_SOURCE_SCHEDULER,
    CRITICAL_ERROR_QUEUE_SOURCE_EVENT_BUS,
} log_critical_error_queue_source_t;

// Core internal logger
void core_log_internal(size_t level, const char *fmt, ...);

// Plugin internal logger
void plugin_log_internal(size_t level, const char *plugin_name, const char *fmt, ...);

void add_event_bus_critical_error(const plugin_id_t plugin_id, const char *name,
    log_critical_error_queue_source_t log_critical_error_queue_source, const error_context_t error_ctx);

void add_scheduler_critical_error(const plugin_id_t plugin_id, int timer_id,
    log_critical_error_queue_source_t log_critical_error_queue_source, const error_context_t error_ctx);

void core_log_init(void);

// === Public API ===
// These are wrappers around `core_log_internal`
// Macros ensure that in release builds (without defines), debug/warn/info logs are stripped at compile time.

#ifdef CORE_LOG_DEBUG
    #define core_log_debug(fmt, ...) core_log_internal(DEBUG, fmt, ##__VA_ARGS__)
#else
    // Debug logs are compiled out if CORE_LOG_DEBUG is not defined
    #define core_log_debug(fmt, ...) ((void)0)
#endif

#ifdef CORE_LOG_INFO
    /**
     * Info log function.
     * Requires build.conf -> ENABLE_LOG_INFO=1 (will set -DCORE_LOG_INFO).
     * Used for high-level informational messages about system state.
     */
    #define core_log_info(fmt, ...) core_log_internal(INFO, fmt, ##__VA_ARGS__)
#else
    #define core_log_info(fmt, ...) ((void)0)
#endif

#ifdef CORE_LOG_WARN
    /**
     * Warning log function.
     * Requires build.conf -> ENABLE_LOG_WARN=1 (will set -DCORE_LOG_WARN).
     * Used for potential issues that don't stop execution.
     */
    #define core_log_warn(fmt, ...) core_log_internal(WARN, fmt, ##__VA_ARGS__)
#else
    #define core_log_warn(fmt, ...) ((void)0)
#endif

/**
 * Error logs are always enabled.
 * They will also always be written to file.
 */
#define core_log_error(fmt, ...) core_log_internal(ERROR, fmt, ##__VA_ARGS__)

#endif // CORE_LOG_H
