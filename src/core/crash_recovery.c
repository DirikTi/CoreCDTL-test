#include <stddef.h>
#include <unistd.h>
#include "platform.h"
#include "log.h"
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <scheduler.h>

#if OS_DARWIN
#define _XOPEN_SOURCE 700
#endif
#include "crash_recovery.h"
#include "event_bus.h"

#define RAW_BUFFER_SIZE (uint32_t)0b0000001000000000

#ifdef __APPLE__
struct sigcontext {
    __uint64_t fault_address;
    __uint64_t regs[31];
    __uint64_t sp;
    __uint64_t pc;
    __uint64_t pstate;
    __uint8_t  __reserved[4096] __attribute__((__aligned__(16)));
};
#endif

static void crash_handler(int sig, siginfo_t *info, void *context);

void crash_recovery_init(void)
{
    struct sigaction sa;
    sa.sa_sigaction = crash_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);

    core_log_info("CRASH_RECOVERY: initialized");
}

static void crash_handler(int sig, siginfo_t *info, void *ucontext)
{
    ucontext_t *uc = (ucontext_t *)ucontext;

    struct sigcontext *sc = (struct sigcontext *)&uc->uc_mcontext;
    #ifdef __APPLE__
        uint64_t *sp = (uint64_t*)sc->sp;
    #elif __linux__
        uint64_t *sp = (uint64_t*)sc->rsp;
    #else
    #error "Unsupported platform for sigcontext"
    #endif

    for (int i = 0; i < RAW_BUFFER_SIZE - sizeof(error_context_t); i++) {
        error_context_t *ctx = (error_context_t *)((uint8_t *)sp + i);

        if (ctx->marker == SCHEDULER_ERROR_MARKER) {
            write(STDERR_FILENO, "[crash_handler] Found SCHEDULER_ERROR_MARKER\n\n", 45);

            ctx->signal = sig;
            ctx->signal_code = info->si_code;
            ctx->fault_address = info->si_addr;

            siglongjmp(scheduler_thread_jmp_env, 1);
        }

        if (ctx->marker == EVENT_BUS_ERROR_MARKER) {
            write(STDERR_FILENO, "[crash_handler] Found event_bust_ERROR_MARKER\n", 45);

            ctx->signal = sig;
            ctx->signal_code = info->si_code;
            ctx->fault_address = info->si_addr;

            siglongjmp(event_thread_jmp_env, 1);
        }
    }

    write(STDERR_FILENO, "[crash_handler] No marker found. Exiting...\n", 44);

    _exit(1);
}