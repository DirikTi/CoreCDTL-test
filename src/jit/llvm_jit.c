#include "llvm_jit.h"
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __APPLE__
#define LLVM_JIT_STUB_LIB_PATH "./jit-lib/libjitcore.dylib"
#else
#define LLVM_JIT_STUB_LIB_PATH "./jit-lib/libjitcore.so"
#endif

static void* dl = NULL;
static LLVMJITSymbols* symbols = NULL;

void llvm_jit_init(void)
{
    if (symbols != NULL) {
        return;
    }

    dl = dlopen(LLVM_JIT_STUB_LIB_PATH, RTLD_NOW);
    if (!dl) {
        fprintf(stderr, "Failed to load JIT stub library: %s\n", dlerror());
        return;
    }

    symbols = calloc(1, sizeof(LLVMJITSymbols));
    if (!symbols) {
        fprintf(stderr, "Failed to allocate symbols\n");
        dlclose(dl);
        dl = NULL;
        return;
    }

#define LLVM_JIT_LOAD_SYMBOL(sym) \
symbols->sym = (typeof(symbols->sym))dlsym(dl, #sym); \
if (!symbols->sym) { \
fprintf(stderr, "Missing symbol: %s\n", #sym); \
free(symbols); \
symbols = NULL; \
dlclose(dl); \
dl = NULL; \
return; \
}

    LLVM_JIT_LOAD_SYMBOL(create_api_subscribe_stub);
    LLVM_JIT_LOAD_SYMBOL(create_api_scheduler_after_stub);
    LLVM_JIT_LOAD_SYMBOL(create_api_scheduler_every_ms_stub);
    LLVM_JIT_LOAD_SYMBOL(create_api_scheduler_cancel_stub);
    LLVM_JIT_LOAD_SYMBOL(create_api_hk_getter_stub);
    LLVM_JIT_LOAD_SYMBOL(create_api_hk_setter_stub);

    LLVM_JIT_LOAD_SYMBOL(get_stub_function);

#undef LOAD_SYMBOL
}

LLVMJITSymbols* llvm_jit_get(void)
{
    return symbols;
}

void llvm_jit_cleanup(void)
{
    if (dl) {
        dlclose(dl);
        dl = NULL;
    }

    if (symbols) {
        free(symbols);
        symbols = NULL;
    }
}
