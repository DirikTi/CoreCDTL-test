#ifndef CORECDTL_GATEWAY_H
#define CORECDTL_GATEWAY_H

#include <stdlib.h>
#include "core_utils.h"

#define GATEWAY_DTLS_MSG_LEN 512

typedef enum {
    EVENT_BUS_API,
    SCHEDULER_API,
    HK_API,
    PM_API,
    CRASH_API
} api_type_t;

typedef enum {
    GATEWAY_OK = 0,
    GATEWAY_ALREADY_INITIALIZED = -7,
    GATEWAY_FILE_NOT_FOUND = -10,
    GATEWAY_DLOPEN_FAILED = -1,
    GATEWAY_INIT_SYMBOL_NOT_FOUND = -2,
    GATEWAY_START_SYMBOL_NOT_FOUND = -3,
    GATEWAY_STOP_SYMBOL_NOT_FOUND = -4,
    GATEWAY_ON_DATA_SYMBOL_NOT_FOUND = -5,
    GATEWAY_CLOSE_SYMBOL_NOT_FOUND = -6
} gateway_status_t;

typedef int (*gateway_hk_get_type_fn)(uint32_t plugin_id, const char *type_name, char *out_buf, size_t buf_len);
typedef int (*gateway_hk_type_exists_fn)(uint32_t plugin_id, const char *type_name, const char* field_name);
typedef int (*gateway_hk_set_type_fn)(uint32_t plugin_id, const char *type_name, const char *data);
typedef int (*gateway_hk_get_list_fn)(char *out_buf, size_t buf_len);

typedef int (*gateway_pm_get_list_fn)(char *out_buf, size_t buf_len);
typedef int (*gateway_pm_action_reboot_fn)(uint32_t plugin_id, uint8_t action);

typedef int (*gateway_eb_get_list_fn)(char *out_buf, size_t buf_len);
typedef int (*gateway_sc_get_list_fn)(char *out_buf, size_t buf_len);

int gateway_hk_get_type(uint32_t plugin_id, const char *type_name, char *out_buf, size_t buf_len);
int gateway_hk_field_exists(uint32_t plugin_id, const char *type_name, const char *field_name);
int gateway_hk_set_type(uint32_t plugin_id, const char *type_name, const char *data);
int gateway_hk_get_list(char *out_buf, size_t buf_len);

int gateway_pm_get_list( char *out_buf, size_t buf_len);
int gateway_pm_action_reboot(uint32_t plugin_id, uint8_t action);

int gateway_eb_get_list(char *out_buf, size_t buf_len);

int gateway_sc_get_list(char *out_buf, size_t buf_len);

typedef struct {
    gateway_hk_get_type_fn hk_get_type;
    gateway_hk_type_exists_fn hk_field_exists;
    gateway_hk_set_type_fn hk_set_type;
    gateway_hk_get_list_fn hk_get_list;

    gateway_pm_get_list_fn pm_get_list;
    gateway_pm_action_reboot_fn pm_action_reboot;

    gateway_eb_get_list_fn eb_get_list;
    gateway_sc_get_list_fn sc_get_list;
} gateway_entry_t;

typedef void (*gateway_init_fn)(gateway_entry_t *gateway_entry);
typedef void (*gateway_stop_fn)(void);
typedef void (*gateway_start_fn)(void);
typedef void (*gateway_on_data_fn)(const char* out_buf, size_t buf_len);
typedef void (*gateway_close_fn)(void);

int gateway_init_lib(const char *lib_path);
void gateway_start_lib(void);
void gateway_stop_lib(void);
void gateway_restart_lib(void);
void gateway_close_lib(void);

extern int g_gateway_connected_flag;

int gateway_msg_send(api_type_t api_type, const char *linestr);
const char* gateway_status_to_str(int status);

#endif //CORECDTL_GATEWAY_H