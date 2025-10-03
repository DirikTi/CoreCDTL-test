#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "platform.h"
#include "plugin_api.h"
#include "log.h"
#include "utils_string.h"
#include "plugin_manager.h"
#include "plugin_loader.h"

#if OS_WINDOWS
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

static plugin_handle_t *plugins = NULL;
static size_t plugins_count = 0;
static size_t plugins_capacity = 0;

#define FREE_PLUGIN_HANDLE(h) do { \
        if ((h)->cold_data.path) free((h)->cold_data.path); \
        if ((h)->cold_data.manifest_path) free((h)->cold_data.manifest_path); \
        if ((h)->cold_data.params) free((h)->cold_data.params); \
        if ((h)->cold_data.so) dlclose((h)->cold_data.so); \
        if ((h)->info.name) free((void*)(h)->info.name); \
        if ((h)->info.version) free((void*)(h)->info.version); \
        memset(h, 0, sizeof(plugin_handle_t)); \
    } while (0)

int plugin_init(void)
{
    struct dirent *entry;
    DIR *dp = opendir(PLUGIN_LIB_DIR);

    if (UNLIKELY(dp == NULL)) {
        mkdir(PLUGIN_LIB_DIR, 0700);
        dp = opendir(PLUGIN_LIB_DIR);
        if (UNLIKELY(dp == NULL)) return -1;
    }

    while ((entry = readdir(dp)) != NULL) {
        if (UNLIKELY(entry->d_type != DT_DIR || entry->d_name[0] == '.')) continue;

        char plugin_lib_path[512];
        snprintf(plugin_lib_path, sizeof(plugin_lib_path), "%s%s%s%s%s%s",
            PLUGIN_LIB_DIR, PATH_SEPARATOR, entry->d_name, PATH_SEPARATOR, entry->d_name, PLUGIN_LIB_EXTENSION);

        char manifest_path[512];
        snprintf(manifest_path, sizeof(manifest_path), "%s%s%s%s%s",
            PLUGIN_LIB_DIR, PATH_SEPARATOR, entry->d_name, PATH_SEPARATOR, PLUGIN_MANIFEST_FILE);

        char data_path[512];
        snprintf(data_path, sizeof(data_path), "%s%s%s%s%s",
            PLUGIN_LIB_DIR, PATH_SEPARATOR, entry->d_name, PATH_SEPARATOR, PLUGIN_DATA_FILE);

        plugin_handle_t *plugin = plugin_load(entry->d_name, plugin_lib_path, manifest_path, data_path);

        if (LIKELY(plugin != NULL)) {
            if (UNLIKELY(plugins_count >= plugins_capacity)) {
                plugins_capacity = plugins_capacity ? plugins_capacity * 2 : 10;
                plugins = realloc(plugins, plugins_capacity * sizeof(plugin_handle_t));
                if (UNLIKELY(plugins == NULL)) {
                    free(plugin);
                    continue;
                }
            }

            plugins[plugins_count++] = *plugin;
            free(plugin);
        } else {
            core_log_warn("Plugin %s cannot load", entry->d_name);
        }
    }

    closedir(dp);

    for (int i = 0; i < plugins_count; i++) {
        plugin_handle_t *h = &plugins[i];

        if (UNLIKELY(h->funcs.initialize(&h->core_api, PLUGIN_PARAMS(h), PLUGIN_PARAM_COUNT(h)) != 0)) {
            core_log_error("Plugin Loader: plugin_init failed => %s v%s", h->info.name, h->info.version);
            FREE_PLUGIN_HANDLE(h);
            continue;
        }

        core_log_info("Plugin Loader: Initialized plugin => %s v%s", h->info.name, h->info.version);

        if (h->funcs.start != NULL) {
            if (UNLIKELY(h->funcs.start() != 0)) {
                core_log_error("Plugin Loader: plugin_start failed => %s v%s", h->info.name, h->info.version);

                if (h->funcs.shutdown) {
                    h->funcs.shutdown();
                }

                FREE_PLUGIN_HANDLE(h);
                continue;
            }
        }

        core_log_info("Plugin Loader: Started plugin => %s v%s", h->info.name, h->info.version);
        h->hot_meta.state = PLUGIN_STATE_ALIVE;
    }

    return 0;
}

plugin_id_t plugin_get_p_id(const char *plugin_name)
{
    if (UNLIKELY(plugin_name == NULL)) return PLUGIN_ID_INVALID;

    for (size_t i = 0; i < plugins_count; i++) {
        if (LIKELY(plugins[i].info.name != NULL) &&
            LIKELY(optimized_strcmp(plugins[i].info.name, plugin_name) == 0)) {
            return plugins[i].info.id;
        }
    }

    return PLUGIN_ID_INVALID;
}

static inline plugin_handle_t *get_plugin_by_id(plugin_id_t id)
{
    if (UNLIKELY(id == PLUGIN_ID_INVALID || id > plugins_count)) return NULL;

    const size_t index = id - 1;
    return &plugins[index];
}

plugin_op_result_t plugin_shutdown(plugin_id_t plugin_id)
{
    if (UNLIKELY(plugin_id == PLUGIN_ID_INVALID))
        return PLUGIN_OP_INVALID_ID;

    plugin_handle_t *plugin;
    if (UNLIKELY((plugin = get_plugin_by_id(plugin_id)) == NULL))
        return PLUGIN_OP_NOT_FOUND;

    if (UNLIKELY(plugin->hot_meta.state == PLUGIN_STATE_KILLED))
        return PLUGIN_OP_ALREADY_IN_STATE;

    if (LIKELY(plugin->funcs.stop != NULL && plugin->hot_meta.state != PLUGIN_STATE_STOPPED)) {
        plugin->funcs.stop();
    }

    if (LIKELY(plugin->funcs.shutdown != NULL)) {
        plugin->funcs.shutdown();
    }

    plugin->hot_meta.state = PLUGIN_STATE_KILLED;
    return PLUGIN_OP_SUCCESS;
}

plugin_op_result_t plugin_initialize(plugin_id_t plugin_id)
{
    if (UNLIKELY(plugin_id == PLUGIN_ID_INVALID))
        return PLUGIN_OP_INVALID_ID;

    plugin_handle_t *plugin;
    if (UNLIKELY((plugin = get_plugin_by_id(plugin_id)) == NULL))
        return PLUGIN_OP_NOT_FOUND;

    if (UNLIKELY(plugin->hot_meta.state >= PLUGIN_STATE_KILLED))
        return PLUGIN_OP_ALREADY_IN_STATE;

    if (UNLIKELY(plugin->funcs.initialize == NULL))
        return PLUGIN_OP_FUNC_NOT_AVAILABLE;

    if (UNLIKELY(plugin->funcs.initialize(&plugin->core_api, PLUGIN_PARAMS(plugin), PLUGIN_PARAM_COUNT(plugin)) != 0))
        return PLUGIN_OP_FUNC_FAILED;

    if (plugin->funcs.start != NULL) {
        plugin->funcs.start();
    }

    plugin->hot_meta.state = PLUGIN_STATE_ALIVE;
    return PLUGIN_OP_SUCCESS;
}

plugin_op_result_t plugin_start(plugin_id_t plugin_id)
{
    if (UNLIKELY(plugin_id == PLUGIN_ID_INVALID))
        return PLUGIN_OP_INVALID_ID;

    plugin_handle_t *plugin;
    if (UNLIKELY((plugin = get_plugin_by_id(plugin_id)) == NULL))
        return PLUGIN_OP_NOT_FOUND;

    if (LIKELY(plugin->hot_meta.state == PLUGIN_STATE_ALIVE))
        return PLUGIN_OP_ALREADY_IN_STATE;

    if (plugin->funcs.start == NULL)
        return PLUGIN_OP_SUCCESS;

    if (UNLIKELY(plugin->funcs.start() != 0))
        return PLUGIN_OP_FUNC_FAILED;

    plugin->hot_meta.state = PLUGIN_STATE_ALIVE;
    return PLUGIN_OP_SUCCESS;
}

plugin_op_result_t plugin_stop(plugin_id_t plugin_id)
{
    if (UNLIKELY(plugin_id == PLUGIN_ID_INVALID))
        return PLUGIN_OP_INVALID_ID;

    plugin_handle_t *plugin;
    if (UNLIKELY((plugin = get_plugin_by_id(plugin_id)) == NULL))
        return PLUGIN_OP_NOT_FOUND;

    if (UNLIKELY(plugin->hot_meta.state != PLUGIN_STATE_ALIVE))
        return PLUGIN_OP_ALREADY_IN_STATE;

    if (UNLIKELY(plugin->funcs.stop == NULL))
        return PLUGIN_OP_FUNC_NOT_AVAILABLE;

    plugin->funcs.stop();
    plugin->hot_meta.state = PLUGIN_STATE_STOPPED;
    return PLUGIN_OP_SUCCESS;
}

void pm_list_serialize(char* out_buf, size_t out_buf_size)
{
    char temp_buf[256];
    out_buf[0] = '\0';

    snprintf(temp_buf, sizeof(temp_buf), "[on_data] [pm]\n[getall]\n");
    strncat(out_buf, temp_buf, out_buf_size - strlen(out_buf) - 1);

    for (size_t i = 0; i < plugins_count; i++) {
        plugin_handle_t *h = &plugins[i];

        snprintf(temp_buf, sizeof(temp_buf), "[%s] %d [%s] %d %d %d\n",
            h->info.name,
            h->info.id,
            h->info.version,
            h->info.code,
            h->hot_meta.state,
            (h->funcs.start != NULL && h->funcs.stop != NULL) ? 1 : 0
        );

        size_t curr_len = strlen(out_buf);
        size_t space_left = out_buf_size - curr_len - 1;

        if (strlen(temp_buf) < space_left) {
            strncat(out_buf, temp_buf, space_left);
        } else {
            // Buffer Stack_Overflow
            break;
        }
    }
}

int pm_action_reboot(plugin_id_t plugin_id, uint8_t action)
{
    plugin_handle_t *h = get_plugin_by_id(plugin_id);
    if (h == NULL) return -1;

    if (h->funcs.start == NULL || h->funcs.stop == NULL) return -2;

    if (action == 1) {
        if (h->hot_meta.state == PLUGIN_STATE_ALIVE) return -3;

        h->funcs.start();
        h->hot_meta.state = PLUGIN_STATE_ALIVE;
    }
    else {
        if (h->hot_meta.state == PLUGIN_STATE_STOPPED) return -3;

        h->funcs.stop();
        h->hot_meta.state = PLUGIN_STATE_STOPPED;
    }

    return 0;
}

size_t get_plugins_count(void)
{
    return plugins_count;
}

char *plugin_get_name(plugin_id_t plugin_id)
{
    if (UNLIKELY(plugin_id == PLUGIN_ID_INVALID || plugin_id > plugins_count)) return NULL;

    register int index = plugin_id - 1;
    return plugins[index].info.name;
}


__attribute__((visibility("default")))
void *plugin_bridge_get_fn_ptr(const char *plugin_name, const char *fn_name)
{
    plugin_id_t plugin_id = plugin_get_p_id(plugin_name);
    plugin_handle_t *h = get_plugin_by_id(plugin_id);

    if (!h) {
        fprintf(stderr, "Plugin not found: %s\n", plugin_name);
        return NULL;
    }

    if (!h->cold_data.so) {
        fprintf(stderr, "Plugin SO not loaded: %s\n", plugin_name);
        return NULL;
    }

    void *fn_ptr = dlsym(h->cold_data.so, fn_name);
    if (!fn_ptr) {
        fprintf(stderr, "Function %s not found in plugin %s\n", fn_name, plugin_name);
        return NULL;
    }

    return fn_ptr;
}
