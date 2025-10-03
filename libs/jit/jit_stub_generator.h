#pragma once

#include <stdint.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

    // Event Bus
    typedef void (*bus_cb_t)(const void* data, size_t len, void* user);
    typedef int (*bus_subscribe_t)(uint32_t plugin_id, const char* event, bus_cb_t cb, void* user);

    // Scheduler
    typedef void (*sched_cb_t)(const void* data, size_t len, void* user);
    typedef int (*scheduler_after_ms_t)(uint32_t plugin_id, uint64_t ms, sched_cb_t cb, void* user);
    typedef int (*scheduler_every_ms_t)(uint32_t plugin_id, uint64_t ms, sched_cb_t cb, void* user);
    typedef int (*scheduler_cancel_t)(uint32_t plugin_id, int id);

    // Heapkit
    typedef int (*hk_get_field_t)(uint32_t plugin_id, const char* type_name, const char* field_name, void* out_value);
    typedef int (*hk_set_field_t)(uint32_t plugin_id, const char* type_name, const char* field_name, void* value);

    // C tarafında opaque struct olarak tanımlıyoruz
    typedef struct JITStub JITStub;

    JITStub* create_api_subscribe_stub(uint32_t plugin_id, bus_subscribe_t real_fn);
    JITStub* create_api_scheduler_after_stub(uint32_t plugin_id, scheduler_after_ms_t real_fn);
    JITStub* create_api_scheduler_every_ms_stub(uint32_t plugin_id, scheduler_every_ms_t real_fn);
    JITStub* create_api_scheduler_cancel_stub(uint32_t plugin_id, scheduler_cancel_t real_fn);

    // HK Api
    JITStub* create_api_hk_getter_stub(uint32_t plugin_id, hk_get_field_t real_fn);
    JITStub* create_api_hk_setter_stub(uint32_t plugin_id, hk_set_field_t real_fn);


    // JIT fonksiyonuna erişim sağlar
    // - Dönüş: int (*)(const char* event, void* cb, void* user)
    void* get_stub_function(JITStub* stub);

    // Temizlik için stub nesnesini yok eder
    void destroy_stub_function(JITStub* stub);

#ifdef __cplusplus
}
#endif
