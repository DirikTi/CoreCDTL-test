#ifndef CORE_RUNTIME_H
#define CORE_RUNTIME_H

#include <signal.h>

#include "core_api.h"

#ifdef __APPLE__
#define PID_FILE_PATH "/tmp/corecdtl.pid"
#define FIFO_PATH "/tmp/corecdtl_pipe"
#define PID_DIR "/tmp"
#elif __linux__
#define PID_FILE_PATH "/run/corecdtl/corecd.pid"
#define FIFO_PATH "/run/corecdtl/corecd_pipe"
#define PID_DIR "/run/corecdtl"
#else
#define PID_FILE_PATH "./corecdtl.pid"
#define PID_DIR "./"
#endif


extern void *g_bus_publish_return_after_cb_addr;
extern volatile sig_atomic_t g_ficus_enabled;

// Initialize core subsystems (logging, event bus, scheduler, registry)
int core_init(void);

// Get a pointer to the live core API (for plugins/tests)
const core_api_t* core_get_api(void);

// Shutdown core subsystems
void core_shutdown(void);

#endif // CORE_RUNTIME_H
