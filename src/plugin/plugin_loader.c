#include "plugin_loader.h"

#include <ctype.h>

#include "platform.h"
#include "../core/log.h"
#include <string.h>

#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include "heapkit.h"
#include "plugin_api.h"
#include "utils_id.h"
#include "./plugin_stub.h"

#if OS_LINUX || OS_DARWIN
#include <dlfcn.h>
#endif

#define FREE_PLUGIN() \
    plugin_free_info(&h->info); \
    plugin_free_params(&h->cold_data.params, &h->cold_data.param_count); \
    free(h); \

static void plugin_free_info(plugin_info_t *info);
static void plugin_free_params(plugin_param_t **params, size_t *count);
static int plugin_manifest_set(const char *plugin_name, const char *manifest_path, plugin_handle_t *h);
static int plugin_data_set(const char *data_path, plugin_handle_t *h);
static int plugin_api_obj_fns_set(plugin_handle_t *h);

static plugin_id_t current_id = 1;

typedef void (*plugin_log_fn)(plugin_handle_t* h, const char* msg);

plugin_handle_t *plugin_load(const char *plugin_name, const char *path, const char *manifest_path, const char *data_path)
{
    (void)manifest_path;

    if (!path || !manifest_path)
        return NULL;

    plugin_handle_t *h = (plugin_handle_t *)calloc(1, sizeof(*h));

    if (!h)
        return NULL;

    h->info.name = plugin_name;
    h->info.version = NULL;

    if (plugin_manifest_set(plugin_name ,manifest_path, h) != 0) {
        FREE_PLUGIN();
        return NULL;
    }

    if (plugin_data_set(data_path, h) != 0) {
        FREE_PLUGIN();
        return NULL;
    }

    if (plugin_api_obj_fns_set(h) != 0) {
        FREE_PLUGIN();
        return NULL;
    }

#if OS_LINUX || OS_DARWIN

    PLUGIN_SO(h) = dlopen(path, RTLD_NOW);

    if (!PLUGIN_SO(h)) {
        core_log_warn("plugin loader: dlopen failed %s", dlerror());
        FREE_PLUGIN();
        return NULL;
    }

    h->funcs.initialize = (plugin_initialize_fn)dlsym(PLUGIN_SO(h), PLUGIN_INIT_SYM);
    h->funcs.start = (plugin_start_fn)dlsym(PLUGIN_SO(h), PLUGIN_START_SYM);
    h->funcs.stop = (plugin_stop_fn)dlsym(PLUGIN_SO(h), PLUGIN_STOP_SYM);
    h->funcs.shutdown = (plugin_shutdown_fn)dlsym(PLUGIN_SO(h), PLUGIN_DEINIT_SYM);
#else
#error "Windows loader not implemented in skeleton"
#endif

    if (!h->funcs.initialize) {
        core_log_warn("Plugin Loader: Cannot load plugin, missing required init func => %s v%s", h->info.name, h->info.version);
#if OS_LINUX || OS_DARWIN
        if (PLUGIN_SO(h))
            dlclose(PLUGIN_SO(h));
#endif
        FREE_PLUGIN();
        return NULL;
    }

    PLUGIN_PATH(h) = strdup(path);
    PLUGIN_MANIFEST_PATH(h) = strdup(manifest_path);
    PLUGIN_DATA_PATH(h) = strdup(data_path);

    return h;
}

static int plugin_manifest_set(const char *plugin_name, const char *manifest_path, plugin_handle_t *h)
{
    FILE *file = fopen(manifest_path, "r");
    if (!file) {
        core_log_error("Plugin loader: Cannot open %s file.", manifest_path);
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0 || line[0] == '#') continue;

        if (strncmp(line, "--", 2) == 0) {
            char *key = line + 2;
            char *value = strchr(key, '=');

            if (value) {
                *value = '\0';
                value++;

                // Temporary Array
                plugin_param_t *temp = realloc(PLUGIN_PARAMS(h), (PLUGIN_PARAM_COUNT(h) + 1) * sizeof(plugin_param_t));
                if (!temp) {
                    core_log_error("Plugin Loader: Memory allocation failed => %s", manifest_path);
                    fclose(file);
                    return -1;
                }
                PLUGIN_PARAMS(h) = temp;

                PLUGIN_PARAMS(h)[h->cold_data.param_count].key = strdup(key);
                PLUGIN_PARAMS(h)[h->cold_data.param_count].value = strdup(value);
                PLUGIN_PARAM_COUNT(h)++;
            }


        } else {
            char *key = line;
            char *value = strchr(line, '=');

            if (value) {
                *value = '\0';
                value++;

                // Trim whitespace
                char *trimmed_key = key;
                while (*trimmed_key == ' ') trimmed_key++;

                char *trimmed_value = value;
                while (*trimmed_value == ' ') trimmed_value++;

                if (strcasecmp(trimmed_key, "Name") == 0) {
                    h->info.name = strdup(trimmed_value);
                } else if (strcasecmp(trimmed_key, "Version") == 0) {
                    h->info.version = strdup(trimmed_value);
                } else if (strcasecmp(trimmed_key, "Code") == 0) {
                    char *endptr = NULL;
                    errno = 0;
                    long val = strtol(trimmed_value, &endptr, 10);

                    if (endptr == trimmed_value) {
                        core_log_warn("Cored: % plugin '%s' is not a valid number for Code\n", plugin_name, trimmed_value);
                        h->info.code = 0;
                    } else if ((val == LONG_MAX || val == LONG_MIN) && errno == ERANGE) {
                        core_log_warn("Cored: % plugin '%s' is not a valid number for Code\n", plugin_name, trimmed_value);
                        h->info.code = 0;
                    } else {
                        h->info.code = (int)val;
                    }
                }
            }
        }
    }

    fclose(file);

    h->info.id = current_id++;

    return 0;
}

static int plugin_data_set(const char *data_path, plugin_handle_t *h)
{
    FILE *file = fopen(data_path, "r");

    if (!file) {
        core_log_error("Plugin loader: Cannot open %s file.", data_path);
        return -1;
    }

    char line[256];
    type_info_t* current_type = NULL;
    field_info_t* current_fields = NULL;
    size_t field_index = 0;

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0 || line[0] == '#') continue;

        char* trimmed = line;
        while (isspace(*trimmed)) trimmed++;

        if (strncmp(trimmed, "struct", 6) == 0) {
            char struct_name[64];
            sscanf(trimmed, "struct %s", struct_name);

            current_type = calloc(1, sizeof(type_info_t));
            current_type->name = strdup(struct_name);
            current_type->field_count = 0;
            current_fields = calloc(HEAPKIT_MAX_FIELD, sizeof(field_info_t));
            field_index = 0;
            current_type->size = 0;
            current_type->is_simple = false;

        } else if (strncmp(trimmed, "field", 5) == 0 && current_type) {
            char field_name[64], field_type[64];
            char* comment = strchr(trimmed, '#');
            if (comment) *comment = '\0';

            char* token = strtok(trimmed, " \t");
            if (!token || strcmp(token, "field") != 0) continue;

            token = strtok(NULL, " \t");
            if (!token) continue;
            strcpy(field_name, token);

            token = strtok(NULL, " \t");
            if (!token) continue;
            strcpy(field_type, token);

            const char* struct_type = NULL;
            field_type_t ftype = hk_parse_type(field_type, &struct_type);

            field_info_t* field = &current_fields[field_index++];
            field->name = strdup(field_name);
            field->type = ftype;
            field->size = hk_type_size(ftype);
            field->offset = current_type->size;

            if (ftype == TYPE_STRUCT && struct_type) {
                field->type_name = strdup(struct_type);
            }

            current_type->size += field->size;
        } else if (strncmp(trimmed, "end", 3) == 0 && current_type) {
            current_type->fields = current_fields;
            current_type->field_count = field_index;

            if (hk_type_register(h->info.id, current_type) != 0) {
                // Failed
                return 1;
            }

            current_type = NULL;
            current_fields = NULL;
            field_index = 0;
        }
    }

    fclose(file);
    return 0;
}

static int plugin_api_obj_fns_set(plugin_handle_t *h)
{
    int ret = 0;
    if ((ret = plugin_stub_setup(h)) != 0) {
        core_log_error("plugin_stub_setup(h) error %d", ret);
        return ret;
    };

    h->core_api.abi_version = CORECDTL_ABI_VERSION;

    return 0;
}

static void plugin_free_info(plugin_info_t *info)
{
    if (!info) return;
    free(info->name);
    free(info->version);

    info->name = info->version = NULL;
}

static void plugin_free_params(plugin_param_t **params, size_t *count)
{
    if (!params || !*params) return;

    for (size_t i = 0; i < *count; i++) {
        free((*params)[i].key);
        free((*params)[i].value);
    }
    free(*params);
    *params = NULL;
    *count = 0;
}

void plugin_unload(plugin_handle_t *h)
{
    if (!h) return;

    if (h->funcs.stop) h->funcs.stop();
    if (h->funcs.shutdown) h->funcs.shutdown();

#ifdef OS_LINUX
    if (h->cold_data.so) dlclose(h->cold_data.so);
#elif defined(__APPLE__)
    if (h->cold_data.so) dlclose(h->cold_data.so);
#else

#endif

    free(h->cold_data.path);
    free(h->cold_data.manifest_path);
    free(h->cold_data.data_path);

    FREE_PLUGIN()

}