#ifndef CORE_PLUGIN_H
#define CORE_PLUGIN_H

#include <stdint.h>
#include <stddef.h>

#define CORE_IMPLEMENT_API_GLUE
#define CORECDTL_API_VERSION 1

#ifndef PLUGIN_NAME
#define PLUGIN_NAME "unknown_plugin"
#error "PLUGIN_NAME is not defined! Please provide it with -DPLUGIN_NAME=\"your_plugin\""
#endif

#include <dlfcn.h>
#define CORE_DLOPEN(lib) dlopen(lib, RTLD_LAZY | RTLD_GLOBAL)
#define CORE_DLSYM(lib, func) dlsym(lib, func)

typedef struct plugin_param_s {
    const char *key;
    const char *value;
} plugin_param_t;

typedef uint32_t plugin_id_t;
typedef void (*core_event_cb)(const void *data, size_t len, void *user);
typedef core_event_cb core_timer_cb;
typedef void (*plugin_log_internal_func_t)(size_t level, const char *plugin_name, const char *fmt, ...);

typedef struct core_api_s
{
    size_t abi_version;

    // Heap_Kit
    int (*hk_getter)(const char* type_name, const char* field_name, void* value);
    int (*hk_setter)(const char* type_name, const char* field_name, void* out_value);

    // Event bus
    int (*subscribe)(const char *event, core_event_cb cb, void *user);
    int (*publish)(plugin_id_t plugin_id, const char *event, const void *data, size_t len);
    plugin_id_t (*get_plugin_id)(const char *plugin_name);

    // Scheduler / timers
    int (*timer_after_ms)(uint64_t ms, core_timer_cb cb, void *user);
    int (*timer_every_ms)(uint64_t ms, core_timer_cb cb, void *user);
    int (*timer_cancel)(int timer_id);

} core_api_t;

#ifdef CORE_IMPLEMENT_API_GLUE
extern const core_api_t *core_api = NULL;
extern plugin_log_internal_func_t plugin_log = NULL;
#else
extern core_api_t* core_api;
extern plugin_log_internal_func_t plugin_log;
#endif

typedef enum {
    PLUGIN_LOG_INFO,
    PLUGIN_LOG_WARN,
    PLUGIN_LOG_ERROR
} plugin_log_level_t;

#define p_log_info(...)  plugin_log(PLUGIN_LOG_INFO, PLUGIN_NAME, __VA_ARGS__)
#define p_log_warn(...)  plugin_log(PLUGIN_LOG_WARN, PLUGIN_NAME, __VA_ARGS__)
#define p_log_error(...) plugin_log(PLUGIN_LOG_ERROR, PLUGIN_NAME, __VA_ARGS__)

#define CORE_PLUGIN_INIT_API() \
    do { \
        if (core_api == NULL) { \
            void* handle = CORE_DLOPEN(NULL); \
            \
            core_api = p_core_api; \
            /* LOG_FNs */ \
            typedef plugin_log_internal_func_t (*get_plugin_log_func)(void); \
            get_plugin_log_func get_log_ptr = (get_plugin_log_func)CORE_DLSYM(handle, "get_plugin_log_internal"); \
            if (get_log_ptr) { \
                plugin_log = get_log_ptr(); \
            } \
            \
        } \
    } while (0)

#endif // CORE_PLUGIN_H