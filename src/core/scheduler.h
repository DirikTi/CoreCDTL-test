#ifndef CORE_SCHEDULER_H
#define CORE_SCHEDULER_H

#include <setjmp.h>
#include <stdint.h>
#include <stddef.h>

#include "core_utils.h"

extern __thread sigjmp_buf scheduler_thread_jmp_env;
#define SCHEDULER_ERROR_MARKER 0xBEADBEEFCAFEBABEULL

typedef void (*sched_cb_t)(const void* data, size_t len, void* user);

int scheduler_init(void);
void scheduler_shutdown(void);

// returns timer id >= 1 on success
int scheduler_after_ms(plugin_id_t plugin_id, uint64_t ms, sched_cb_t cb, void* user);
int scheduler_every_ms(plugin_id_t plugin_id, uint64_t ms, sched_cb_t cb, void* user);
int scheduler_cancel(plugin_id_t plugin_id, int id);

int scheduler_get_list(char *out_buf, size_t buf_len);

typedef int (*api_scheduler_after_ms_fn)(uint64_t, sched_cb_t, void *);
typedef int (*api_scheduler_every_ms_fn)(uint64_t, sched_cb_t, void *);
typedef int (*api_scheduler_cancel_fn)(int id);

#endif // CORE_SCHEDULER_H
