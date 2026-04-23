# Hook Framework (Modular)

A lightweight, modular C++ hooking framework designed for performance profiling (PAPI), call-chain tracing, and dynamic argument inspection. It allows for fine-grained control over which functions are intercepted and under what calling contexts they should be recorded.

---

## 1. Features
- **Modular Design**: Decouples hook definitions from the core logic, making it easy to extend.
- **Context-Aware Tracing (Rule Map)**: Supports `Caller -> Callee` filtering to reduce noise and overhead.
- **Performance Monitoring**: Integrated with **PAPI** (Performance Application Programming Interface) for hardware counter analysis (e.g., Cycles, Cache Misses).
- **Dynamic Argument Capture**: Inspect and log function arguments with metadata.
- **Lazy Symbol Resolution**: Symbols are resolved only when the function is first invoked.

---

## 2. Build Instructions

### Prerequisites

- CMake 3.10+
- C++17 compatible compiler
- PAPI library (Optional, for performance counters)

### Compilation

```bash
cd hook
# Enable PAPI for hardware performance counters
cmake -S . -B build -DHOOK_ENABLE_PAPI=ON
cmake --build build -j$(nproc)
```
**Output**: `hook/libhook.so`

---

## 3. Rule Map (Scope Filtering)

The framework maintains a map of rules to determine when a trace should be triggered.

- **Global Trace**: If the caller list for a callee is empty, the function is always traced.
- **Restricted Trace**: If a caller list is provided, the hook only triggers if the current call site resides within the scope of one of the specified callers.

**Configuration (Declare in `hook.cpp`):**

```cpp
// Only trace "memcpy_s" when called from within Foo or Bar
HOOKFW_SET_RULE("memcpy_s",
                "MxRec::A::Foo",
                "MxRec::B::Bar");
```

---

## 4. Capturing Function Arguments

To capture arguments, provide a list of pairs containing the argument position (0-indexed) and a descriptive name.

```cpp
// Capturing the 'self' pointer at pos 0 and 'a' at pos 1
HOOKFW_INVOKE(foo, {{0, "this"}, {1, "input_val"}}, self, a);
```

---

## 5. Adding New Hook Targets

Follow this 3-step pattern in `hook.cpp` to intercept a new function:

### Step 1: Define the Function Prototype
```cpp
using foo_fn_t = int (*)(void* self, int a);
```

### Step 2: Register the Target
Use the `HOOKFW_DEFINE_TARGET` macro to provide metadata for symbol resolution.
```cpp
HOOKFW_DEFINE_TARGET(foo,            // Unique identifier
                     foo_fn_t,       // Function signature type
                     "Namespace::Class::Foo", // Human-readable name
                     "_ZN9Namespace5Class3FooEPvi", // Mangled symbol name
                     "/path/to/libtarget.so")       // Source library path
```

### Step 3: Implement the Hook Wrapper
Use the `asm` keyword to ensure the hook matches the target's mangled name.
```cpp
int __attribute__((noinline, visibility("default")))
foo_hook(void* self, int a) asm("_ZN9Namespace5Class3FooEPvi");

int foo_hook(void* self, int a) {
    // Automatically handles: symbol resolve, scope check, PAPI counters, and logging
    return HOOKFW_INVOKE(foo, {{1, "a"}}, self, a);
}
```

---

## 6. Execution

Inject the hook library into your target application using `LD_PRELOAD`:

```bash
export LD_PRELOAD=./libhook.so
./your_executable
```

---

## 7. Technical Details

### Automation Logic
When a hooked function is called, the framework automatically:
1. **One-time Symbol Resolution**: Uses `dlsym` to find the original function address.
2. **Scope Registration**: Tracks the entry/exit of functions to maintain a thread-local call stack.
3. **Rule Validation**: Checks the Rule Map to see if the current caller is authorized for tracing.
4. **PMU Output**: If PAPI is enabled and rules match, it records hardware performance counters for the duration of the function call.

### Important Notes
- **Name Mangling**: Ensure the `asm("_Z...")` string exactly matches the symbol in the target binary. Use `nm -D <library>` to verify.
- **Inlining**: If the compiler inlines the target function, the hook will not trigger. Compile the target with `-fno-inline` or `-O0` for best results during debugging.
- **Recursion**: The framework includes internal guards to prevent infinite loops if the hook logic calls the intercepted function.