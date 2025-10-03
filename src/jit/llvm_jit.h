#ifndef CORECDTL_LLVM_JIT_H
#define CORECDTL_LLVM_JIT_H

#include <stdint.h>
#include <stddef.h>

typedef struct JITStub JITStub;

typedef void (*bus_cb_t)(const void* data, size_t len, void* user);
typedef int (*bus_subscribe_t)(uint32_t plugin_id, const char* event, bus_cb_t cb, void* user);

typedef void (*sched_cb_t)(const void* data, size_t len, void* user);
typedef int (*scheduler_after_ms_t)(uint32_t plugin_id, uint64_t ms, sched_cb_t cb, void* user);
typedef int (*scheduler_every_ms_t)(uint32_t plugin_id, uint64_t ms, sched_cb_t cb, void* user);
typedef int (*scheduler_cancel_t)(uint32_t plugin_id, int id);

typedef int (*hk_get_field_t)(uint32_t plugin_id, const char* type_name, const char* field_name, void* out_value);
typedef int (*hk_set_field_t)(uint32_t plugin_id, const char* type_name, const char* field_name, void* value);

typedef JITStub* (*create_api_subscribe_stub_t)(uint32_t plugin_id, bus_subscribe_t real_fn);
typedef JITStub* (*create_api_scheduler_after_stub_t)(uint32_t plugin_id, scheduler_after_ms_t real_fn);
typedef JITStub* (*create_api_scheduler_every_ms_stub_t)(uint32_t plugin_id, scheduler_every_ms_t real_fn);
typedef JITStub* (*create_api_scheduler_cancel_stub_t)(uint32_t plugin_id, scheduler_cancel_t real_fn);
typedef JITStub* (*create_api_hk_getter_stub_t)(uint32_t plugin_id, hk_get_field_t real_fn);
typedef JITStub* (*create_api_hk_setter_stub_t)(uint32_t plugin_id, hk_set_field_t real_fn);

typedef void* (*get_stub_function_t)(JITStub* stub);

typedef struct {
    create_api_subscribe_stub_t             create_api_subscribe_stub;
    create_api_scheduler_after_stub_t       create_api_scheduler_after_stub;
    create_api_scheduler_every_ms_stub_t    create_api_scheduler_every_ms_stub;
    create_api_scheduler_cancel_stub_t      create_api_scheduler_cancel_stub;
    create_api_hk_getter_stub_t             create_api_hk_getter_stub;
    create_api_hk_setter_stub_t             create_api_hk_setter_stub;

    get_stub_function_t                     get_stub_function;
} LLVMJITSymbols;

void llvm_jit_init(void);
LLVMJITSymbols* llvm_jit_get(void);
void llvm_jit_cleanup(void);

#endif // CORECDTL_LLVM_JIT_H
