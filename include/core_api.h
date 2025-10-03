#ifndef CORE_API_H
#define CORE_API_H

#include <stdint.h>
#include <core_utils.h>
#include <pthread.h>

#define CORE_IMPLEMENT_API_GLUE 1

#ifndef CORECDTL_API_VERSION
#define CORECDTL_API_VERSION 0
#endif

#ifndef CORECDTL_ABI_VERSION
#define CORECDTL_ABI_VERSION 0
#endif

typedef plugin_id_t (*core_get_plugin_id_fn)(const char *plugin_name);

// Event callback (publish/subscribe)
typedef void (*core_event_cb)(const void *data, size_t len, void *user);

// Timer callback reuses core_event_cb signature (data may be NULL)
typedef core_event_cb core_timer_cb;

typedef int (*subscribe_fn)(const char*, core_event_cb, void*);

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

// Log Internal
typedef void (*plugin_log_internal_func_t)(size_t level, const char *plugin_name, const char *fmt, ...);

#endif // CORE_API_H
