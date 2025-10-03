#include "heapkit.h"
#include "log.h"
#include <string.h>
#include <stdio.h>
#include "gateway.h"

#define MAX_REGISTERED_TYPES 128

static registered_type_t g_type_registry[MAX_REGISTERED_TYPES];
static size_t g_type_count = 0;
static api_type_t self_api_type = HK_API;

int hk_init(void) {
    memset(g_type_registry, 0, sizeof(g_type_registry));
    return 0;
}

size_t hk_type_size(field_type_t type)
{
    switch (type) {
        case TYPE_INT: return sizeof(int);
        case TYPE_FLOAT: return sizeof(float);
        case TYPE_BOOL: return sizeof(bool);
        case TYPE_CHAR: return sizeof(char);
        case TYPE_STRING: return sizeof(char*);
        case TYPE_PTR: return sizeof(void*);
        default: return 0;
    }
}

field_type_t hk_parse_type(const char* str, const char** out_struct_name)
{
    if (strcmp(str, "int") == 0) return TYPE_INT;
    if (strcmp(str, "float") == 0) return TYPE_FLOAT;
    if (strcmp(str, "bool") == 0) return TYPE_BOOL;
    if (strcmp(str, "char") == 0) return TYPE_CHAR;
    if (strcmp(str, "string") == 0) return TYPE_STRING;
    if (strcmp(str, "ptr") == 0) return TYPE_PTR;
    *out_struct_name = str;
    return TYPE_STRUCT;
}

int hk_type_register(plugin_id_t plugin_id, type_info_t* info)
{
    if (!info || !info->name) return -1;

    for (size_t i = 0; i < g_type_count; ++i) {
        if (g_type_registry[i].plugin_id == plugin_id &&
            strcmp(g_type_registry[i].type_info->name, info->name) == 0) {
            core_log_warn("HeapKit: Type '%s' already registered for plugin %d", info->name, plugin_id);
            return 1;
        }
    }

    if (g_type_count >= MAX_REGISTERED_TYPES) return -2;

    void* memory = calloc(1, info->size);
    if (!memory) return -3;

    g_type_registry[g_type_count].plugin_id = plugin_id;
    g_type_registry[g_type_count].type_info = info;
    g_type_registry[g_type_count].d_instance = memory;
    g_type_count++;

    core_log_debug("HeapKit: Registered type '%s' with instance for plugin %d", info->name, plugin_id);
    return 0;
}

static registered_type_t* find_registered(plugin_id_t plugin_id, const char* type_name) {
    for (size_t i = 0; i < g_type_count; i++) {
        if (g_type_registry[i].plugin_id == plugin_id &&
            strcmp(g_type_registry[i].type_info->name, type_name) == 0) {
            return &g_type_registry[i];
        }
    }
    return NULL;
}

int hk_set_field(plugin_id_t plugin_id, const char* type_name, const char* field_name, void* value)
{
    registered_type_t* reg = find_registered(plugin_id, type_name);
    if (!reg) return -1;

    for (size_t i = 0; i < reg->type_info->field_count; ++i) {
        field_info_t* field = &reg->type_info->fields[i];

        if (strcmp(field->name, field_name) == 0) {
            memcpy((char*)reg->d_instance + field->offset, value, field->size);
            if (g_gateway_connected_flag) {
                char temp_value[127];
                switch (field->type) {
                    case TYPE_INT:
                        snprintf(temp_value, sizeof(temp_value), "%s=%d;", field->name, *(int*)value);
                        break;
                    case TYPE_FLOAT:
                        snprintf(temp_value, sizeof(temp_value), "%s=%.3f;", field->name, *(float*)value);
                        break;
                    case TYPE_BOOL:
                        snprintf(temp_value, sizeof(temp_value), "%s=%s;", field->name, (*(bool*)value) ? "true" : "false");
                        break;
                    case TYPE_CHAR:
                        snprintf(temp_value, sizeof(temp_value), "%s=%c;", field->name, *(char*)value);
                        break;
                    case TYPE_STRING:
                        snprintf(temp_value, sizeof(temp_value), "%s=\"%s\";", field->name, (*(char**)value) ?: "");
                        break;
                    default:
                        continue;
                }
                char msg[GATEWAY_DTLS_MSG_LEN];
                snprintf(msg, GATEWAY_DTLS_MSG_LEN, "[set] %d [%s] %s",
                    plugin_id,
                    type_name,
                    temp_value
                );
                gateway_msg_send(self_api_type, msg);
            }

            return 0;
        }
    }

    return -2;
}

int hk_get_field(plugin_id_t plugin_id, const char* type_name, const char* field_name, void* out_value)
{
    registered_type_t* reg = find_registered(plugin_id, type_name);
    if (!reg) return -1;

    for (size_t i = 0; i < reg->type_info->field_count; ++i) {
        field_info_t* field = &reg->type_info->fields[i];
        if (strcmp(field->name, field_name) == 0) {
            memcpy(out_value, (char*)reg->d_instance + field->offset, field->size);
            return 0;
        }
    }

    return -2;
}

int hk_get_list(char *out_buf, size_t out_buf_size)
{
    strcat(out_buf, "[on_data] [hk]\n[getall]\n");

    for (size_t i = 0; i < g_type_count; ++i) {
        registered_type_t* reg = &g_type_registry[i];

        char header[64];
        snprintf(header, sizeof(header), "[plugin] [%d]\n", reg->plugin_id);
        if (strlen(out_buf) + strlen(header) >= out_buf_size)
            return -3;
        strcat(out_buf, header);

        char typeLine[1024];
        snprintf(typeLine, sizeof(typeLine), "%s ", reg->type_info->name);

        for (size_t j = 0; j < reg->type_info->field_count; ++j) {
            field_info_t* field = &reg->type_info->fields[j];
            void* ptr = (char*)reg->d_instance + field->offset;

            char temp[256];
            switch (field->type) {
                case TYPE_INT:
                    snprintf(temp, sizeof(temp), "%s=%d;", field->name, *(int*)ptr);
                    break;
                case TYPE_FLOAT:
                    snprintf(temp, sizeof(temp), "%s=%.3f;", field->name, *(float*)ptr);
                    break;
                case TYPE_BOOL:
                    snprintf(temp, sizeof(temp), "%s=%s;", field->name, (*(bool*)ptr) ? "true" : "false");
                    break;
                case TYPE_CHAR:
                    snprintf(temp, sizeof(temp), "%s=%c;", field->name, *(char*)ptr);
                    break;
                case TYPE_STRING:
                    snprintf(temp, sizeof(temp), "%s=\"%s\";", field->name, (*(char**)ptr) ?: "");
                    break;
                default:
                    continue;
            }

            if (strlen(typeLine) + strlen(temp) >= sizeof(typeLine))
                return -3;

            strcat(typeLine, temp);
        }

        if (strlen(out_buf) + strlen(typeLine) + 1 >= out_buf_size)
            return -3;

        strcat(out_buf, typeLine);
        strcat(out_buf, "\n");
    }

    printf("%s\n", out_buf);
    return 0;
}


int hk_serialize(plugin_id_t plugin_id, const char* type_name, char* out_buf, size_t out_buf_size) {
    registered_type_t* reg = find_registered(plugin_id, type_name);
    if (!reg) return -1;

    if (!out_buf) return -2;

    out_buf[0] = '\0';
    for (size_t i = 0; i < reg->type_info->field_count; ++i) {
        field_info_t* field = &reg->type_info->fields[i];
        void* ptr = (char*)reg->d_instance + field->offset;

        char temp[256];
        switch (field->type) {
            case TYPE_INT:
                snprintf(temp, sizeof(temp), "%s=%d;", field->name, *(int*)ptr);
                break;
            case TYPE_FLOAT:
                snprintf(temp, sizeof(temp), "%s=%.3f;", field->name, *(float*)ptr);
                break;
            case TYPE_BOOL:
                snprintf(temp, sizeof(temp), "%s=%s;", field->name, (*(bool*)ptr) ? "true" : "false");
                break;
            case TYPE_CHAR:
                snprintf(temp, sizeof(temp), "%s=%c;", field->name, *(char*)ptr);
                break;
            case TYPE_STRING:
                snprintf(temp, sizeof(temp), "%s=\"%s\";", field->name, (*(char**)ptr) ?: "");
                break;
            default:
                continue;
        }

        if (strlen(out_buf) + strlen(temp) >= out_buf_size)
            return -3;

        strcat(out_buf, temp);
    }

    return 0;
}

int hk_deserialize(plugin_id_t plugin_id, const char* type_name, const char* data) {
    registered_type_t* reg = find_registered(plugin_id, type_name);
    if (!reg || !data) return -1;

    char buf[1024];
    strncpy(buf, data, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';

    char* token = strtok(buf, ";");

    while (token) {
        char* eq = strchr(token, '=');
        if (!eq) {
            token = strtok(NULL, ";");
            continue;
        }

        *eq = '\0';
        const char* key = token;
        const char* val = eq + 1;

        field_info_t* field = NULL;
        for (size_t i = 0; i < reg->type_info->field_count; ++i) {
            if (strcmp(reg->type_info->fields[i].name, key) == 0) {
                field = &reg->type_info->fields[i];
                break;
            }
        }

        if (!field) {
            token = strtok(NULL, ";");
            continue;
        }

        void* ptr = (char*)reg->d_instance + field->offset;

        switch (field->type) {
            case TYPE_INT:
                *(int*)ptr = atoi(val);
                break;
            case TYPE_FLOAT:
                *(float*)ptr = strtof(val, NULL);
                break;
            case TYPE_BOOL:
                *(bool*)ptr = (strcmp(val, "true") == 0);
                break;
            case TYPE_CHAR:
                *(char*)ptr = val[0];
                break;
            case TYPE_STRING:
                if (*val == '"' && val[strlen(val) - 1] == '"') {
                    ((char**)ptr)[0] = strndup(val + 1, strlen(val) - 2);
                } else {
                    ((char**)ptr)[0] = strdup(val);
                }
                break;
            default:
                break;
        }

        token = strtok(NULL, ";");
    }

    return 0;
}

field_type_t hk_field_type(plugin_id_t plugin_id, const char* type_name, const char* field_name) {
    registered_type_t* reg = find_registered(plugin_id, type_name);
    if (!reg) return TYPE_UNKNOWN;

    for (size_t i = 0; i < reg->type_info->field_count; ++i) {
        if (strcmp(reg->type_info->fields[i].name, field_name) == 0)
            return reg->type_info->fields[i].type;
    }

    return TYPE_UNKNOWN;
}

bool hk_field_exists(plugin_id_t plugin_id, const char* type_name, const char* field_name) {
    return hk_field_type(plugin_id, type_name, field_name) != TYPE_UNKNOWN;
}
