#include <stdio.h>
#include "gateway.h"
#include "heapkit.h"
#include "plugin_manager.h"
#include <dlfcn.h>
#include <sys/stat.h>

#include "event_bus.h"
#include "gateway_dispatcher.h"
#include "log.h"
#include "scheduler.h"

static void gateway_clear(void);

int g_gateway_connected_flag = 0;

int gateway_is_alive = 0;

gateway_entry_t gateway_entry = {
    .hk_get_type = gateway_hk_get_type,
    .hk_field_exists = gateway_hk_field_exists,
    .hk_set_type = gateway_hk_set_type,
    .hk_get_list = gateway_hk_get_list,

    .pm_get_list = gateway_pm_get_list,
    .pm_action_reboot = gateway_pm_action_reboot,

    .eb_get_list = gateway_eb_get_list,
    .sc_get_list = gateway_sc_get_list,
};

gateway_init_fn init_fn;
gateway_start_fn start_fn;
gateway_stop_fn stop_fn;
gateway_on_data_fn on_data_fn;
gateway_close_fn close_fn;

int gateway_msg_send(api_type_t api_type, const char *linestr)
{
    const char* api_name = "unknown";
    switch (api_type) {
        case EVENT_BUS_API: api_name = "event_bus"; break;
        case SCHEDULER_API: api_name = "scheduler"; break;
        case HK_API:        api_name = "hk";        break;
        case PM_API:        api_name = "pm";        break;
        case CRASH_API:     api_name = "crash";     break;
        default:            api_name = "unknown";   break;
    }

    char safe_buf[1024];
    snprintf(safe_buf, sizeof(safe_buf), "[on_data] [%s]\n%s\n", api_name, linestr);
    gateway_d_msg_add(safe_buf, strlen(safe_buf));

    return 0;
}

/*
 *
 * @brief EB & SC (event_bus & scheduler)
 */
int gateway_eb_get_list(char *out_buf, size_t buf_len)
{
    event_bus_get_list(out_buf, buf_len);
    return 0;
}

int gateway_sc_get_list(char *out_buf, size_t buf_len)
{
    scheduler_get_list(out_buf, buf_len);
    return 0;
}

/*
 *
 * @brief PM (plugin manager)
 */
int gateway_pm_get_list(char *out_buf, size_t buf_len)
{
    pm_list_serialize(out_buf, buf_len);
    return 0;
}

int gateway_hk_get_list(char *out_buf, size_t buf_len)
{
    hk_get_list(out_buf, buf_len);
    return 0;
}

int gateway_pm_action_reboot(plugin_id_t plugin_id, uint8_t action)
{
    const int ret = pm_action_reboot(plugin_id, action);

    return ret;
}

/*
 *
 * @brief HK (heapkit)
 */
int gateway_hk_get_type(uint32_t plugin_id, const char *type_name, char *out_buf, size_t buf_len)
{
    return hk_serialize(plugin_id, type_name, out_buf, buf_len);
}

int gateway_hk_field_exists(uint32_t plugin_id, const char *type_name, const char *field_name)
{
    if (hk_field_exists(plugin_id, type_name, field_name)) return 1;
    return 0;
}

int gateway_hk_set_type(uint32_t plugin_id, const char *type_name, const char *data)
{
    hk_deserialize(plugin_id, type_name, data);
    return 1;
}

int gateway_init_lib(const char *lib_path)
{
    if (gateway_is_alive == 1) {
        return GATEWAY_ALREADY_INITIALIZED;
    }

    struct stat st;
    if (stat(lib_path, &st) != 0) {
        return GATEWAY_FILE_NOT_FOUND;
    }

    void *handle = dlopen(lib_path, RTLD_NOW);
    if (!handle) {
        return GATEWAY_DLOPEN_FAILED;
    }

    init_fn = (gateway_init_fn)dlsym(handle, "gateway_init");
    if (!init_fn) {
        dlclose(handle);
        return GATEWAY_INIT_SYMBOL_NOT_FOUND;
    }

    start_fn = (gateway_start_fn)dlsym(handle, "gateway_start");
    if (!start_fn) {
        dlclose(handle);
        return GATEWAY_START_SYMBOL_NOT_FOUND;
    }

    stop_fn = (gateway_stop_fn)dlsym(handle, "gateway_stop");
    if (!stop_fn) {
        dlclose(handle);
        return GATEWAY_STOP_SYMBOL_NOT_FOUND;
    }

    close_fn = (gateway_close_fn)dlsym(handle, "gateway_close");
    if (!close_fn) {
        dlclose(handle);
        return GATEWAY_CLOSE_SYMBOL_NOT_FOUND;
    }

    on_data_fn = (gateway_on_data_fn)dlsym(handle, "gateway_on_data");
    if (!on_data_fn) {
        dlclose(handle);
        return GATEWAY_ON_DATA_SYMBOL_NOT_FOUND;
    }

    init_fn(&gateway_entry);

    gateway_dispatcher_init(on_data_fn);

    g_gateway_connected_flag = 1;
    printf("[gateway] Gateway initialized\n");
    return GATEWAY_OK;
}

void gateway_start_lib(void)
{
    if (g_gateway_connected_flag && gateway_is_alive == 0) {
        start_fn();
        gateway_is_alive = 1;
    }
}

void gateway_stop_lib(void)
{
    if (g_gateway_connected_flag && gateway_is_alive == 1) {
        g_gateway_connected_flag = 0;
        stop_fn();
    }
}

void gateway_restart_lib(void)
{
    if (g_gateway_connected_flag) {
        stop_fn();
        g_gateway_connected_flag = 0;
        start_fn();
        g_gateway_connected_flag = 1;
    }
}

void gateway_close_lib(void)
{
    if (g_gateway_connected_flag) {
        if (gateway_is_alive == 1) {
            gateway_is_alive = 0;
            stop_fn();
        }

        g_gateway_connected_flag = 0;
        close_fn();
        gateway_clear();
    }
}

static void gateway_clear(void)
{
    g_gateway_connected_flag = 0;
    gateway_is_alive = 0;

    gateway_dispatcher_close();

    init_fn = NULL;
    start_fn = NULL;
    stop_fn = NULL;
    on_data_fn = NULL;
    close_fn = NULL;
}

const char* gateway_status_to_str(int status)
{
    switch (status) {
        case GATEWAY_OK: return "Gateway initialized successfully.";
        case GATEWAY_ALREADY_INITIALIZED: return "Gateway is already initialized.";
        case GATEWAY_FILE_NOT_FOUND: return "Gateway library file not found.";
        case GATEWAY_DLOPEN_FAILED: return "Failed to load gateway library (dlopen failed).";
        case GATEWAY_INIT_SYMBOL_NOT_FOUND: return "Missing 'init' function in gateway library.";
        case GATEWAY_START_SYMBOL_NOT_FOUND: return "Missing 'start' function in gateway library.";
        case GATEWAY_STOP_SYMBOL_NOT_FOUND: return "Missing 'stop' function in gateway library.";
        case GATEWAY_CLOSE_SYMBOL_NOT_FOUND: return "Missing 'close' function in gateway library.";
        case GATEWAY_ON_DATA_SYMBOL_NOT_FOUND: return "Missing 'on_data' function in gateway library.";
        default: return "Unknown error occurred.";
    }
}
