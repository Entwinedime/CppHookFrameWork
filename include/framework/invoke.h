#pragma once

#include "framework/hook_arg_info.h"
#include "framework/hook_target.h"
#include "framework/relation_rules.h"
#include "framework/scoped_timer.h"
#include <initializer_list>
#include <stdint.h>
#include <type_traits>
#include <utility>


namespace HookFrameWork {

__attribute__((always_inline)) inline uintptr_t CurrentCallerPc() {
    void * return_addr{ __builtin_return_address(0) };
    if (return_addr == nullptr) { return 0; }
    return reinterpret_cast<uintptr_t>(__builtin_extract_return_addr(return_addr));
}

template <typename FnType, typename... Args>
__attribute__((always_inline)) inline auto InvokeHook(HookTarget<FnType> & target, std::initializer_list<HookArgInfo> trace_args,
                                                      Args &&... args) -> decltype(target.Original()(std::forward<Args>(args)...)) {
    FnType real_fn{ target.Original() };
    const uintptr_t caller_pc{ CurrentCallerPc() };

    if (!RelationRules::Get().ShouldTrace(target.TraceName(), caller_pc)) {
        if constexpr (std::is_void<decltype(real_fn(std::forward<Args>(args)...))>::value) {
            real_fn(std::forward<Args>(args)...);
            return;
        }
        else { return real_fn(std::forward<Args>(args)...); }
    }

    ScopedTimer timer(target.TraceName(), trace_args, std::forward<Args>(args)...);
    if constexpr (std::is_void<decltype(real_fn(std::forward<Args>(args)...))>::value) {
        real_fn(std::forward<Args>(args)...);
        return;
    }
    else { return real_fn(std::forward<Args>(args)...); }
}

} // namespace HookFrameWork
