#ifndef CORECDTL_PLUGIN_MANAGER_H
#define CORECDTL_PLUGIN_MANAGER_H

#include "core_api.h"

plugin_id_t plugin_get_p_id(const char *plugin_name);
char *plugin_get_name(plugin_id_t plugin_id);
int plugin_init(void);

/* Gateway Funcs */
void pm_list_serialize(char* out_buf, size_t out_buf_size);
int pm_action_reboot(plugin_id_t plugin_id, uint8_t action);

/**
 * @brief Plugin operation result codes
 */
typedef enum {
    PLUGIN_OP_SUCCESS = 0,              /**< Operation completed successfully */
    PLUGIN_OP_INVALID_ID = 1,           /**< Invalid plugin ID provided */
    PLUGIN_OP_NOT_FOUND = 2,            /**< Plugin with specified ID not found */
    PLUGIN_OP_ALREADY_IN_STATE = 3,     /**< Plugin already in target state */
    PLUGIN_OP_FUNC_NOT_AVAILABLE = 4,   /**< Required function not available */
    PLUGIN_OP_FUNC_FAILED = 5           /**< Plugin function execution failed */
} plugin_op_result_t;

#endif //CORECDTL_PLUGIN_MANAGER_H