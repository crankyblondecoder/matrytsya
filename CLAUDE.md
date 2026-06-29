# Coding Style

This is a C++ codebase. Follow the conventions below exactly when writing new code.

## Naming

| Thing | Convention | Example |
|---|---|---|
| Classes | `PascalCase` | `GraphNode`, `ThreadPool` |
| Public methods | `camelCase` | `createEdge()`, `getRunning()` |
| Protected hook methods | `_camelCase` (single underscore) | `_start()`, `_applyAction()` |
| Private internal methods | `__camelCase` (double underscore) | `__removeEdge()`, `__complete()` |
| Member variables | `_camelCase` (single underscore) | `_refCount`, `_threadRunning` |
| Macros and enum values | `ALL_CAPS` | `EDGE_ARRAY_SIZE`, `PING_GRAPH_ACTION` |
| Class files | `PascalCase.cpp` / `PascalCase.hpp` | `GraphNode.cpp`, `ThreadBase.hpp` |
| Module-level headers | `camelCase.hpp` | `thread.hpp`, `graphActionFlagRegister.hpp` |

## Header Files

Use `#ifndef`/`#define`/`#endif` guards — not `#pragma once`. The guard name is the class name in `ALL_CAPS_H`:

```cpp
#ifndef GRAPH_NODE_H
#define GRAPH_NODE_H
// ...
#endif
```

Forward-declare classes before including their headers in `.hpp` files where possible. Keep includes minimal in headers; pull them into the `.cpp` instead.

## Class Layout

```cpp
class ClassName : public BaseClass
{
    public:

        ClassName();

        void publicMethod();

    protected:

        virtual ~ClassName();

        virtual void _hookMethod() = 0;

    private:

        // Disable copying.
        ClassName(const ClassName& copyFrom);
        ClassName& operator= (const ClassName& copyFrom);

        /// Member variable docs use triple-slash.
        int _memberVariable = 0;
};
```

Rules:
- Order of sections: `public`, `protected`, `private`.
- Access specifiers are indented to the same level as member declarations (one tab in, not flush-left).
- Copy constructor and assignment operator are disabled by declaring them `private` without defining them.
- If the class is ref-counted, the destructor must be `protected virtual`.
- No blank line between the class name line and the opening `{`.

## Indentation

Tabs for structural indentation throughout. Do not mix spaces as structural indentation.

## Arrow Operator

Always put a space before and after `->`:

```cpp
edge -> decrRef();          // correct
edge->decrRef();            // wrong
```

## Include Order in `.cpp` Files

1. The matching `.hpp` (first, always)
2. Other local project headers
3. Standard library / system headers

C headers are included directly (`<errno.h>`, `<time.h>`), not via C++ wrappers (`<cerrno>`).

## Comments

- Doxygen `/** */` blocks for classes and public/protected methods. Use `@param`, `@returns`, `@note`, `@throw`.
- `///` for member variable documentation.
- `//` for inline implementation comments explaining non-obvious logic.
- Section dividers: `// -- Section name --`

```cpp
/**
 * Brief description.
 * @param foo What foo is.
 * @returns What is returned.
 * @note Important constraint.
 * @throw SomeException When and why.
 */
void method(int foo);
```

Do not comment what the code does — only why, and only when non-obvious.

## Synchronisation

Include `"thread.hpp"` to get the `SYNC` macro. Use it as a scoped lock:

```cpp
{ SYNC(_lock)

    // Only read/write thread-shared data here.
    // Do not call external functions inside a SYNC block.
}
```

The opening brace and `SYNC` are on the same line. The closing brace is on its own line. The body is indented one level inside the block.

## Memory Management

Reference-counted heap objects inherit from `RefCounted`. Rules:
- Call `incrRef()` before storing a pointer; call `decrRef()` when done with it.
- Before returning a ref-counted pointer from a function, `incrRef()` it; the caller is responsible for the `decrRef()`.
- Handle classes (e.g., `GraphNodeHandle`, `GraphEdgeHandle`) provide RAII management — prefer them over raw pointers.
- Delete ref-counted objects outside of `SYNC` blocks to avoid re-entry.

## Error Handling

Each module defines its own exception class that inherits from `Exception`:

```cpp
class GraphException : public Exception
{
    public:

        enum Error
        {
            UNKNOWN,
            /// Brief description.
            SPECIFIC_ERROR
        };

        virtual ~GraphException(){}

        GraphException(Error error) : Exception(Exception::GRAPH), _error{error} {}

    private:

        Error _error;
};
```

- Enum members are `ALL_CAPS`.
- Document each enum value with `///`.
- Constructor uses brace initialisation for base and member init.

## Platform Abstraction

Concrete platform implementations are hidden behind `typedef`:

```cpp
typedef ThreadMutexPthread ThreadMutex;
```

Consumers use only the typedef name. This makes swapping implementations (e.g., for embedded targets without pthreads) a one-line change.

## Method Visibility Conventions

- `public`: The caller-facing API.
- `protected virtual _method()`: Hooks that subclasses must or may implement.
- `private __method()`: Internal helpers; double underscore signals "not for subclasses".
- `final` on public methods that must not be overridden.
- `virtual` and `override` are both written on overriding methods.

## Bit-Field Flag Registers

Action flags and similar bit fields live in a dedicated `*FlagRegister.hpp` file as `#define` macros with explicit hex values. Each bit position is reserved as a commented-out line even if unused, so the register is easy to extend:

```cpp
#define PING_GRAPH_ACTION       0x00000001
#define SERIALISABLE_GRAPH_ACTION 0x00000002

//#define _GRAPH_ACTION         0x00000004
```
