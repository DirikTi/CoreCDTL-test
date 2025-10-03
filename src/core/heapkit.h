#ifndef CORECDTL_HEAPKIT_H
#define CORECDTL_HEAPKIT_H

#include <stdbool.h>
#include "core_utils.h"

#define HEAPKIT_MAX_FIELD 10

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_STRING,
    TYPE_STRUCT,
    TYPE_PTR,
    TYPE_UNKNOWN
} field_type_t;

typedef struct {
    const char* name;
    field_type_t type;
    size_t offset;
    size_t size;
    const char* type_name;
} field_info_t;

typedef struct {
    const char* name;
    size_t size;
    size_t field_count;
    field_info_t* fields;
    bool is_simple;
    field_type_t base_type;
} type_info_t;

typedef struct {
    plugin_id_t plugin_id;
    type_info_t* type_info;
    void* d_instance;
} registered_type_t;

// === Initialization ===
int hk_init(void);

// === Type registration ===
int hk_type_register(plugin_id_t plugin_id, type_info_t* info);

// === Field utilities ===
field_type_t hk_field_type(plugin_id_t plugin_id, const char* type_name, const char* field_name);
bool hk_field_exists(plugin_id_t plugin_id, const char* type_name, const char* field_name);

// === Getters & Setters ===
int hk_set_field(plugin_id_t pid, const char* type_name, const char* field_name, void* value);
int hk_get_field(plugin_id_t pid, const char* type_name, const char* field_name, void* out_value);

// === Serialization ===
int hk_serialize(plugin_id_t plugin_id, const char* type_name, char* out_buf, size_t out_buf_size);
int hk_deserialize(plugin_id_t plugin_id, const char* type_name, const char* data);
int hk_get_list(char *out_buf, size_t out_buf_size);

// === Helpers ===
size_t hk_type_size(field_type_t type);
field_type_t hk_parse_type(const char* str, const char** out_struct_name);

typedef int (*api_hk_get_field_fn)(const char* type_name, const char* field_name, void* value);
typedef int (*api_hk_set_field_fn)(const char* type_name, const char* field_name, void* out_value);

#endif // CORECDTL_HEAPKIT_H
