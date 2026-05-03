# AI Coding Rules

1. **Strict Obedience on Exclusions**: If instructed not to code a specific expression or feature, do not code it under any circumstances.
2. **Explicit Scope Only**: Code only what was explicitly requested and agreed upon. Do not proactively add unrequested improvements, refactoring, code cleanup, or "nice-to-have" features. 
3. **C-Style C++ (Sniffer & Emulators)**: For code related to the sniffer and emulators components, stick as closely as possible to C-style programming, even within .cpp files. 
   * Using object-oriented features like classes and polymorphism is still required/allowed where appropriate.
4. **Header Includes**: Use standard C-style includes with the `.h` extension whenever possible (e.g., `<stdint.h>`, `<stdio.h>`).
5. **Avoid `std::`**: Minimize the use of the C++ Standard Library (`std::`). Exceptions are allowed only for specific multithreading primitives such as `std::thread` or `std::mutex` where necessary.
6. **Variable Declarations**: Declare variables at the beginning of their respective block (C-style), not necessarily at the beginning of the function. If a variable is used only within a specific internal block, it must be declared at the top of that specific block. 
   * **Exception**: Variables used exclusively in for-loops can (and should) be declared within the loop statement (e.g., `for (int i = 0; ...)`), unless there is a specific need to use the variable after the loop.
7. **No Magic Numbers**: Avoid hardcoding numbers (like buffer sizes). Use `sizeof()` or defined constants to prevent potential bugs if buffer sizes change in the future.
8. **Code Deduplication**: Avoid duplicating parsing or logic blocks. Extract repeating logic into dedicated private helper functions.