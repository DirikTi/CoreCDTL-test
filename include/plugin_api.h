#ifndef CORECDTL_PLUGIN_API_H
#define CORECDTL_PLUGIN_API_H

#include <core_utils.h>
#include <stdalign.h>

#include "core_api.h"

typedef enum {
    PLUGIN_STATE_ALIVE,
    PLUGIN_STATE_STOPPED,
    PLUGIN_STATE_KILLED,
    PLUGIN_STATE_CRASHED
} plugin_state_t;

// C ABI entrypoints a plugin must export
typedef int     (*plugin_initialize_fn)(core_api_t *core_api, plugin_param_t *params, size_t param_count);
typedef void    (*plugin_shutdown_fn)(void);
typedef int     (*plugin_start_fn)(void);
typedef void    (*plugin_stop_fn)(void);
typedef void    (*plugin_crash_fn)(void);

// C ABI entrypoint symbols
#define PLUGIN_INIT_SYM     "plugin_initialize"
#define PLUGIN_DEINIT_SYM   "plugin_shutdown"
#define PLUGIN_START_SYM    "plugin_start"
#define PLUGIN_STOP_SYM     "plugin_stop"
#define PLUGIN_CRASH_SYM    "plugin_crash"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

_Static_assert(sizeof(plugin_info_t) == 32, "Info should be 32 bytes, got " TOSTRING(sizeof(plugin_info_t)));

typedef struct __attribute__((packed)) {
    plugin_initialize_fn initialize;
    plugin_start_fn start;
    plugin_stop_fn stop;
    plugin_shutdown_fn shutdown;
} plugin_funcs_t;

typedef struct __attribute__((packed)) {
    plugin_id_t id;
    uint8_t state;
    uint8_t flags;
    char padding[2];
} plugin_hot_meta_t;

typedef struct __attribute__((packed)) {
    char *path;
    char *manifest_path;
    char *data_path;
    void *so;
    plugin_param_t *params;
    uint8_t param_count;
    char padding[3];
} plugin_cold_data_t;

_Static_assert(sizeof(plugin_hot_meta_t) == 8, "Hot meta should be 8 bytes, got " TOSTRING(sizeof(plugin_hot_meta_t)));
_Static_assert(sizeof(plugin_funcs_t) == 32, "Funcs should be 32 bytes, got " TOSTRING(sizeof(plugin_funcs_t)));
_Static_assert(sizeof(plugin_info_t) == 32, "Info should be 32 bytes, got " TOSTRING(sizeof(plugin_info_t)));
_Static_assert(sizeof(plugin_cold_data_t) == 44, "Cold data should be 40 bytes, got " TOSTRING(sizeof(plugin_cold_data_t)));

#define TOTAL_SIZE (sizeof(plugin_hot_meta_t) + \
                   sizeof(plugin_funcs_t) + \
                   sizeof(plugin_info_t) + \
                   sizeof(plugin_cold_data_t))

#pragma message("Total struct size: " TOSTRING(TOTAL_SIZE))

typedef struct __attribute__((aligned(64))) {
    plugin_hot_meta_t hot_meta;      // 8 bytes
    plugin_funcs_t funcs;            // 32 bytes
    plugin_info_t info;              // 32 bytes
    plugin_cold_data_t cold_data;    // 40 bytes
    core_api_t core_api;

    char __padding[16];
} plugin_handle_t;

_Static_assert(sizeof(plugin_handle_t) == 256,
               "Plugin handle should be 256 bytes, got " TOSTRING(sizeof(plugin_handle_t)));
_Static_assert(alignof(plugin_handle_t) == 64,
               "Plugin handle alignment wrong: " TOSTRING(alignof(plugin_handle_t)));

// Helper macros
#define PLUGIN_SO(plugin)          ((plugin)->cold_data.so)
#define PLUGIN_PATH(plugin)        ((plugin)->cold_data.path)
#define PLUGIN_MANIFEST_PATH(plugin)    ((plugin)->cold_data.manifest_path)
#define PLUGIN_DATA_PATH(plugin)    ((plugin)->cold_data.data_path)
#define PLUGIN_PARAMS(plugin)      ((plugin)->cold_data.params)
#define PLUGIN_PARAM_COUNT(plugin) ((plugin)->cold_data.param_count)

#endif //CORECDTL_PLUGIN_API_H