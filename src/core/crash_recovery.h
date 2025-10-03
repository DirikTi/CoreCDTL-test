#ifndef CORECDTL_CRASH_RECOVERY_H
#define CORECDTL_CRASH_RECOVERY_H

void crash_recovery_init(void);

typedef struct {
    uint64_t marker;
    int signal;
    int signal_code;
    void *fault_address;
} error_context_t;

#endif //CORECDTL_CRASH_RECOVERY_H