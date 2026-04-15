# AI Coding Rules

Please adhere to the following rules when assisting with this project:

1. **Strict Obedience on Exclusions**: If instructed *not* to code a specific expression or feature, do not code it under any circumstances.
2. **No Over-Engineering**: Code *only* what was explicitly requested. Do not proactively add unrequested improvements, refactoring, or "nice-to-have" features.
3. **C-Style C++ (Sniffer & Emulators)**: For code related to the `sniffer` and `emulators` components, stick as closely as possible to C-style programming, even within `.cpp` files.
   - Using object-oriented features like classes and polymorphism is still required/allowed where appropriate.
4. **Header Includes**: Use standard C-style includes with the `.h` extension whenever possible (e.g., `<stdint.h>`, `<stdio.h>`).
5. **Avoid `std::`**: Minimize the use of the C++ Standard Library (`std::`). Exceptions are allowed *only* for specific multithreading primitives such as `std::thread` or `std::mutex` where necessary.