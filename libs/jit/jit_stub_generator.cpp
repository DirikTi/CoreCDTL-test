#include "jit_stub_generator.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Support/TargetSelect.h>

using namespace llvm;
using namespace llvm::orc;

struct JITStub {
    std::unique_ptr<LLJIT> jit;
    void* fn_ptr;
};

extern "C" void* get_stub_function(JITStub* stub) {
    if (!stub) return nullptr;
    return stub->fn_ptr;
}

extern "C" void destroy_stub_function(JITStub* stub) {
    delete stub;
}

extern "C" {

    JITStub* create_api_subscribe_stub(uint32_t plugin_id, bus_subscribe_t real_fn) {
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();

        auto ctx = std::make_unique<LLVMContext>();
        auto mod = std::make_unique<Module>("stub_mod", *ctx);
        IRBuilder<> builder(*ctx);

        // Tip tanımları
        auto voidPtrTy = builder.getPtrTy(); // i8*
        auto i32Ty = builder.getInt32Ty();   // i32

        // JIT fonksiyon tipi: (const char*, void*, void*) -> int
        auto stubFnType = FunctionType::get(
            i32Ty, {voidPtrTy, voidPtrTy, voidPtrTy}, false);

        // Fonksiyon oluştur
        Function* stubFn = Function::Create(
            stubFnType, Function::ExternalLinkage, "api_subscribe_stub", mod.get());

        // Parametreleri al
        auto argIter = stubFn->arg_begin();
        Value* eventName = &*argIter++;
        Value* cb = &*argIter++;
        Value* user = &*argIter++;

        // entry bloğu oluştur
        auto entry = BasicBlock::Create(*ctx, "entry", stubFn);
        builder.SetInsertPoint(entry);

        // plugin_id sabiti
        Value* pidVal = builder.getInt32(plugin_id);

        // orijinal bus_subscribe fonksiyonu tipi
        auto busType = FunctionType::get(
            i32Ty, {i32Ty, voidPtrTy, voidPtrTy, voidPtrTy}, false);

        // Gerçek fonksiyon pointer'ını LLVM değeri haline getir
        auto fnPtrVal = ConstantInt::get(
            Type::getInt64Ty(*ctx), reinterpret_cast<uint64_t>(real_fn));
        auto fnPtr = builder.CreateIntToPtr(fnPtrVal, busType->getPointerTo());

        // Fonksiyonu çağır
        auto callInst = builder.CreateCall(
            busType, fnPtr, {pidVal, eventName, cb, user});

        // Sonucu döndür
        builder.CreateRet(callInst);

        // JIT oluştur
        auto jitOrErr = LLJITBuilder().create();
        if (!jitOrErr) return nullptr;

        auto jit = std::move(*jitOrErr);
        if (auto err = jit->addIRModule(ThreadSafeModule(std::move(mod), std::move(ctx)))) {
            return nullptr;
        }

        auto sym = jit->lookup("api_subscribe_stub");
        if (!sym) return nullptr;

        auto addr = *sym;

        // JIT ile birlikte fn pointer döndür
        auto* stub = new JITStub();
        stub->jit = std::move(jit);
        stub->fn_ptr = reinterpret_cast<void*>(addr.getValue());

        return stub;
    }
}

extern "C" {
    JITStub* create_api_scheduler_after_stub(uint32_t plugin_id, scheduler_after_ms_t real_fn) {
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();

        auto ctx = std::make_unique<LLVMContext>();
        auto mod = std::make_unique<Module>("stub_mod", *ctx);
        IRBuilder<> builder(*ctx);

        // Tip tanımları
        auto voidPtrTy = builder.getPtrTy(); // i8*
        auto i32Ty = builder.getInt32Ty();
        auto i64Ty = builder.getInt64Ty();// i32

        // JIT fonksiyon tipi: (const char*, void*, void*) -> int
        auto stubFnType = FunctionType::get(
            i32Ty, {i64Ty, voidPtrTy, voidPtrTy}, false);

        // Fonksiyon oluştur
        Function* stubFn = Function::Create(
            stubFnType, Function::ExternalLinkage, "api_scheduler_after_ms_stub", mod.get());

        // Parametreleri al
        auto argIter = stubFn->arg_begin();
        Value* ms = &*argIter++;
        Value* cb = &*argIter++;
        Value* user = &*argIter++;

        // entry bloğu oluştur
        auto entry = BasicBlock::Create(*ctx, "entry", stubFn);
        builder.SetInsertPoint(entry);

        // plugin_id sabiti
        Value* pidVal = builder.getInt32(plugin_id);

        // orijinal bus_subscribe fonksiyonu tipi
        auto busType = FunctionType::get(
            i32Ty, {i32Ty, i64Ty, voidPtrTy, voidPtrTy}, false);

        // Gerçek fonksiyon pointer'ını LLVM değeri haline getir
        auto fnPtrVal = ConstantInt::get(
            Type::getInt64Ty(*ctx), reinterpret_cast<uint64_t>(real_fn));
        auto fnPtr = builder.CreateIntToPtr(fnPtrVal, busType->getPointerTo());

        // Fonksiyonu çağır
        auto callInst = builder.CreateCall(
            busType, fnPtr, {pidVal, ms, cb, user});

        // Sonucu döndür
        builder.CreateRet(callInst);

        // JIT oluştur
        auto jitOrErr = LLJITBuilder().create();
        if (!jitOrErr) return nullptr;

        auto jit = std::move(*jitOrErr);
        if (auto err = jit->addIRModule(ThreadSafeModule(std::move(mod), std::move(ctx)))) {
            return nullptr;
        }

        auto sym = jit->lookup("api_scheduler_after_ms_stub");
        if (!sym) return nullptr;

        auto addr = *sym;

        // JIT ile birlikte fn pointer döndür
        auto* stub = new JITStub();
        stub->jit = std::move(jit);
        stub->fn_ptr = reinterpret_cast<void*>(addr.getValue());

        return stub;
    }
}


extern "C" {
    JITStub* create_api_scheduler_every_ms_stub(uint32_t plugin_id, scheduler_every_ms_t real_fn) {
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();

        auto ctx = std::make_unique<LLVMContext>();
        auto mod = std::make_unique<Module>("stub_mod", *ctx);
        IRBuilder<> builder(*ctx);

        // Tip tanımları
        auto voidPtrTy = builder.getPtrTy(); // i8*
        auto i32Ty = builder.getInt32Ty();
        auto i64Ty = builder.getInt64Ty();// i32

        // JIT fonksiyon tipi: (const char*, void*, void*) -> int
        auto stubFnType = FunctionType::get(
            i32Ty, {i64Ty, voidPtrTy, voidPtrTy}, false);

        // Fonksiyon oluştur
        Function* stubFn = Function::Create(
            stubFnType, Function::ExternalLinkage, "api_scheduler_every_ms_stub", mod.get());

        // Parametreleri al
        auto argIter = stubFn->arg_begin();
        Value* ms = &*argIter++;
        Value* cb = &*argIter++;
        Value* user = &*argIter++;

        // entry bloğu oluştur
        auto entry = BasicBlock::Create(*ctx, "entry", stubFn);
        builder.SetInsertPoint(entry);

        // plugin_id sabiti
        Value* pidVal = builder.getInt32(plugin_id);

        // orijinal bus_subscribe fonksiyonu tipi
        auto busType = FunctionType::get(
            i32Ty, {i32Ty, i64Ty, voidPtrTy, voidPtrTy}, false);

        // Gerçek fonksiyon pointer'ını LLVM değeri haline getir
        auto fnPtrVal = ConstantInt::get(
            Type::getInt64Ty(*ctx), reinterpret_cast<uint64_t>(real_fn));
        auto fnPtr = builder.CreateIntToPtr(fnPtrVal, busType->getPointerTo());

        // Fonksiyonu çağır
        auto callInst = builder.CreateCall(
            busType, fnPtr, {pidVal, ms, cb, user});

        // Sonucu döndür
        builder.CreateRet(callInst);

        // JIT oluştur
        auto jitOrErr = LLJITBuilder().create();
        if (!jitOrErr) return nullptr;

        auto jit = std::move(*jitOrErr);
        if (auto err = jit->addIRModule(ThreadSafeModule(std::move(mod), std::move(ctx)))) {
            return nullptr;
        }

        auto sym = jit->lookup("api_scheduler_every_ms_stub");
        if (!sym) return nullptr;

        auto addr = *sym;

        // JIT ile birlikte fn pointer döndür
        auto* stub = new JITStub();
        stub->jit = std::move(jit);
        stub->fn_ptr = reinterpret_cast<void*>(addr.getValue());

        return stub;
    }
}

extern "C" {
    JITStub* create_api_scheduler_cancel_stub(uint32_t plugin_id, scheduler_cancel_t real_fn) {
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();

        auto ctx = std::make_unique<LLVMContext>();
        auto mod = std::make_unique<Module>("stub_mod", *ctx);
        IRBuilder<> builder(*ctx);

        // Tip tanımları
        auto i32Ty = builder.getInt32Ty();

        // JIT fonksiyon tipi: (const char*, void*, void*) -> int
        auto stubFnType = FunctionType::get(
            i32Ty, {i32Ty}, false);

        // Fonksiyon oluştur
        Function* stubFn = Function::Create(
            stubFnType, Function::ExternalLinkage, "api_scheduler_cancel_stub", mod.get());

        // Parametreleri al
        auto argIter = stubFn->arg_begin();
        Value* cancel_id = &*argIter++;

        // entry bloğu oluştur
        auto entry = BasicBlock::Create(*ctx, "entry", stubFn);
        builder.SetInsertPoint(entry);

        // plugin_id sabiti
        Value* pidVal = builder.getInt32(plugin_id);

        // orijinal bus_subscribe fonksiyonu tipi
        auto busType = FunctionType::get(
            i32Ty, {i32Ty, i32Ty}, false);

        // Gerçek fonksiyon pointer'ını LLVM değeri haline getir
        auto fnPtrVal = ConstantInt::get(
            Type::getInt64Ty(*ctx), reinterpret_cast<uint64_t>(real_fn));
        auto fnPtr = builder.CreateIntToPtr(fnPtrVal, busType->getPointerTo());

        // Fonksiyonu çağır
        auto callInst = builder.CreateCall(
            busType, fnPtr, {pidVal, cancel_id});

        // Sonucu döndür
        builder.CreateRet(callInst);

        // JIT oluştur
        auto jitOrErr = LLJITBuilder().create();
        if (!jitOrErr) return nullptr;

        auto jit = std::move(*jitOrErr);
        if (auto err = jit->addIRModule(ThreadSafeModule(std::move(mod), std::move(ctx)))) {
            return nullptr;
        }

        auto sym = jit->lookup("api_scheduler_cancel_stub");
        if (!sym) return nullptr;

        auto addr = *sym;

        auto* stub = new JITStub();
        stub->jit = std::move(jit);
        stub->fn_ptr = reinterpret_cast<void*>(addr.getValue());

        return stub;
    }
}

extern "C" {
    JITStub* create_api_hk_getter_stub(uint32_t plugin_id, hk_get_field_t real_fn) {
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();

        auto ctx = std::make_unique<LLVMContext>();
        auto mod = std::make_unique<Module>("stub_mod", *ctx);
        IRBuilder<> builder(*ctx);

        // Tip tanımları
        auto i32Ty = builder.getInt32Ty();
        auto voidPtrTy = builder.getPtrTy(); // i8*

        // JIT fonksiyon tipi: (const char*, void*, void*) -> int
        auto stubFnType = FunctionType::get(
            i32Ty, {voidPtrTy, voidPtrTy, voidPtrTy}, false);

        // Fonksiyon oluştur
        Function* stubFn = Function::Create(
            stubFnType, Function::ExternalLinkage, "api_get_field_stub", mod.get());

        // Parametreleri al
        auto argIter = stubFn->arg_begin();
        Value* type_name = &*argIter++;
        Value* field_name = &*argIter++;
        Value* d_value = &*argIter++;

        // entry bloğu oluştur
        auto entry = BasicBlock::Create(*ctx, "entry", stubFn);
        builder.SetInsertPoint(entry);

        // plugin_id sabiti
        Value* pidVal = builder.getInt32(plugin_id);

        // orijinal bus_subscribe fonksiyonu tipi
        auto busType = FunctionType::get(
            i32Ty, {i32Ty, voidPtrTy, voidPtrTy, voidPtrTy}, false);

        // Gerçek fonksiyon pointer'ını LLVM değeri haline getir
        auto fnPtrVal = ConstantInt::get(
            Type::getInt64Ty(*ctx), reinterpret_cast<uint64_t>(real_fn));
        auto fnPtr = builder.CreateIntToPtr(fnPtrVal, busType->getPointerTo());

        // Fonksiyonu çağır
        auto callInst = builder.CreateCall(
            busType, fnPtr, {pidVal, type_name, field_name, d_value});

        // Sonucu döndür
        builder.CreateRet(callInst);

        // JIT oluştur
        auto jitOrErr = LLJITBuilder().create();
        if (!jitOrErr) return nullptr;

        auto jit = std::move(*jitOrErr);
        if (auto err = jit->addIRModule(ThreadSafeModule(std::move(mod), std::move(ctx)))) {
            return nullptr;
        }

        auto sym = jit->lookup("api_get_field_stub");
        if (!sym) return nullptr;

        auto addr = *sym;

        auto* stub = new JITStub();
        stub->jit = std::move(jit);
        stub->fn_ptr = reinterpret_cast<void*>(addr.getValue());

        return stub;
    }
}

extern "C" {
    JITStub* create_api_hk_setter_stub(uint32_t plugin_id, hk_set_field_t real_fn) {
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();

        auto ctx = std::make_unique<LLVMContext>();
        auto mod = std::make_unique<Module>("stub_mod", *ctx);
        IRBuilder<> builder(*ctx);

        auto i32Ty = builder.getInt32Ty();
        auto voidPtrTy = builder.getPtrTy(); // i8*

        auto stubFnType = FunctionType::get(
            i32Ty, {voidPtrTy, voidPtrTy, voidPtrTy}, false);

        Function* stubFn = Function::Create(
            stubFnType, Function::ExternalLinkage, "api_set_field_stub", mod.get());

        auto argIter = stubFn->arg_begin();
        Value* type_name = &*argIter++;
        Value* field_name = &*argIter++;
        Value* set_value = &*argIter++;

        auto entry = BasicBlock::Create(*ctx, "entry", stubFn);
        builder.SetInsertPoint(entry);

        Value* pidVal = builder.getInt32(plugin_id);

        auto busType = FunctionType::get(
            i32Ty, {i32Ty, voidPtrTy, voidPtrTy, voidPtrTy}, false);

        auto fnPtrVal = ConstantInt::get(
            Type::getInt64Ty(*ctx), reinterpret_cast<uint64_t>(real_fn));
        auto fnPtr = builder.CreateIntToPtr(fnPtrVal, busType->getPointerTo());

        auto callInst = builder.CreateCall(
            busType, fnPtr, {pidVal, type_name, field_name, set_value});

        builder.CreateRet(callInst);

        auto jitOrErr = LLJITBuilder().create();
        if (!jitOrErr) return nullptr;

        auto jit = std::move(*jitOrErr);
        if (auto err = jit->addIRModule(ThreadSafeModule(std::move(mod), std::move(ctx)))) {
            return nullptr;
        }

        auto sym = jit->lookup("api_set_field_stub");
        if (!sym) return nullptr;

        auto addr = *sym;

        auto* stub = new JITStub();
        stub->jit = std::move(jit);
        stub->fn_ptr = reinterpret_cast<void*>(addr.getValue());

        return stub;
    }
}