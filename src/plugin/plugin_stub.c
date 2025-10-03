#include "event_bus.h"
#include "heapkit.h"
#include "../jit/llvm_jit.h"
#include "log.h"
#include "plugin_api.h"
#include "plugin_manager.h"
#include "scheduler.h"

static int plugin_stub_scheduler_every(plugin_handle_t *h);
static int plugin_stub_scheduler_after(plugin_handle_t *h);
static int plugin_stub_scheduler_cancel(plugin_handle_t *h);
static int plugin_stub_event_bus_subscribe(plugin_handle_t *h);
static int plugin_stub_hk_get_field(plugin_handle_t *h);
static int plugin_stub_hk_set_field(plugin_handle_t *h);

int plugin_stub_setup(plugin_handle_t *h)
{

    if (plugin_stub_event_bus_subscribe(h) != 0) return 1;
    if (plugin_stub_scheduler_every(h) != 0) return 2;
    if (plugin_stub_scheduler_after(h) != 0) return 3;
    if (plugin_stub_scheduler_cancel(h) != 0) return 4;
    if (plugin_stub_hk_get_field(h) != 0) return 5;
    if (plugin_stub_hk_set_field(h) != 0) return 6;

    h->core_api.publish = bus_publish;
    h->core_api.get_plugin_id = plugin_get_p_id;

    return 0;
}

static int plugin_stub_scheduler_every(plugin_handle_t *h)
{
    LLVMJITSymbols* llvmjit_symbols = llvm_jit_get();
    if (!llvmjit_symbols) return 1;

    JITStub* stub = llvmjit_symbols->create_api_scheduler_every_ms_stub(h->info.id, scheduler_every_ms);

    if (!stub) {
        core_log_error("Plugin_Stub: Can't create scheduler_every stub");
        return 1;
    }

    api_scheduler_every_ms_fn every_ms = (api_scheduler_every_ms_fn)llvmjit_symbols->get_stub_function(stub);
    h->core_api.timer_every_ms = every_ms;

    return 0;
}


static int plugin_stub_scheduler_after(plugin_handle_t *h) {
    LLVMJITSymbols* jit = llvm_jit_get();
    if (!jit) return 1;

    JITStub* stub = jit->create_api_scheduler_after_stub(h->info.id, scheduler_after_ms);
    if (!stub) {
        core_log_error("Plugin_Stub: Can't create scheduler_after stub");
        return 1;
    }

    api_scheduler_after_ms_fn after_ms = (api_scheduler_after_ms_fn)jit->get_stub_function(stub);
    h->core_api.timer_after_ms = after_ms;

    return 0;
}

static int plugin_stub_scheduler_cancel(plugin_handle_t *h) {
    LLVMJITSymbols* jit = llvm_jit_get();
    if (!jit) return 1;

    JITStub* stub = jit->create_api_scheduler_cancel_stub(h->info.id, scheduler_cancel);
    if (!stub) {
        core_log_error("Plugin_Stub: Can't create scheduler_cancel stub");
        return 1;
    }

    api_scheduler_cancel_fn cancel = (api_scheduler_cancel_fn)jit->get_stub_function(stub);
    h->core_api.timer_cancel = cancel;

    return 0;
}

static int plugin_stub_event_bus_subscribe(plugin_handle_t *h) {
    LLVMJITSymbols* jit = llvm_jit_get();
    if (!jit) return 1;

    JITStub* stub = jit->create_api_subscribe_stub(h->info.id, bus_subscribe);
    if (!stub) {
        core_log_error("Plugin_Stub: Can't create event_bus subscribe stub");
        return 1;
    }

    api_bus_subscribe_fn subs = (api_bus_subscribe_fn)jit->get_stub_function(stub);
    h->core_api.subscribe = subs;

    return 0;
}

static int plugin_stub_hk_get_field(plugin_handle_t *h) {
    LLVMJITSymbols* jit = llvm_jit_get();
    if (!jit) return 1;

    JITStub* stub = jit->create_api_hk_getter_stub(h->info.id, hk_get_field);
    if (!stub) {
        core_log_error("Plugin_Stub: Can't create heapkit getter stub");
        return 1;
    }

    api_hk_get_field_fn get = (api_hk_get_field_fn)jit->get_stub_function(stub);
    h->core_api.hk_getter = get;

    return 0;
}

static int plugin_stub_hk_set_field(plugin_handle_t *h) {
    LLVMJITSymbols* jit = llvm_jit_get();
    if (!jit) return 1;

    JITStub* stub = jit->create_api_hk_setter_stub(h->info.id, hk_set_field);
    if (!stub) {
        core_log_error("Plugin_Stub: Can't create heapkit setter stub");
        return 1;
    }

    api_hk_set_field_fn set = (api_hk_set_field_fn)jit->get_stub_function(stub);
    h->core_api.hk_setter = set;

    return 0;
}
