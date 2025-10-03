#ifndef CORECDTL_UTILS_H
#define CORECDTL_UTILS_H

#include <stdint.h>
#include <string.h>

#define PLUGIN_ID_INVALID 0

typedef uint32_t plugin_id_t; // 32 bit for register store
typedef uint16_t event_id_t;
typedef uint16_t cthread_id_t;

typedef struct plugin_info_s {
    plugin_id_t id;
    const char *name;
    const char *version;
    int code;
} plugin_info_t;

typedef struct plugin_param_s {
    const char *key;
    const char *value;
} plugin_param_t;


#define BUS_PUBLISH_ERR_INVALID_PLUGIN_ID    1
#define BUS_PUBLISH_ERR_INVALID_DATA         2
#define BUS_PUBLISH_ERR_INVALID_LENGTH       3
#define BUS_PUBLISH_ERR_EVENT_NOT_FOUND      4
#define BUS_PUBLISH_ERR_MALLOC_FAILED        5

#define BUS_SUB_ERR_INVALID_PLUGIN   1
#define BUS_SUB_ERR_INVALID_EVENT    2
#define BUS_SUB_ERR_INVALID_CB       3
#define BUS_SUB_ERR_ALLOC_FAILED     4
#define BUS_SUB_ERR_STRDUP_FAILED    5

#endif //CORECDTL_UTILS_H