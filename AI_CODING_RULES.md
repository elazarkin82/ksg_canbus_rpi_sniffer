# AI Coding Rules

1. **Explicit Instruction Only**: Do not code or modify any code unless explicitly instructed to do so.
2. **Strict Scope Compliance**: Focus ONLY on what was specifically requested and agreed upon. 
   * Do not remove or modify existing comments unless explicitly told.
   * Do not "improve", refactor, or clean up code that was not part of the request.
   * Do not apply these coding rules to existing code unless explicitly asked to refactor it.
3. **Product Code Style (C-Style C++)**:
   * Stick as closely as possible to C-style programming within `.cpp` and `.hpp` files.
   * Classes and object-oriented features are allowed/required where appropriate.
   * **Avoid `std::`**: Minimize use of the C++ Standard Library. Permitted exceptions: `std::thread`, `std::mutex`, `std::chrono`, and `std::condition_variable`.
   * **Variable Declarations**: Declare variables at the beginning of their respective block `{}`. If a variable is only used within an internal block, it must be declared at the top of that specific block.
   * **For Loops**: Variables used exclusively in for-loops can be declared within the loop statement (e.g., `for (int i = 0; ...)`).
   * **Header Includes**: Use standard C-style includes with the `.h` extension whenever possible (e.g., `<stdint.h>`, `<stdio.h>`), except for the allowed C++ components (`thread`, `mutex`, `chrono`).
4. **Test Code Style**: Modern C++ (latest standards) is fully allowed and encouraged for all test-related code.
5. **Infrastructure Preservation**: NEVER remove, comment out, or modify existing `printf`, `fprintf`, or any log statements unless explicitly instructed.
6. **No Magic Numbers**: Avoid hardcoding numbers.
7. **Code Deduplication**: Extract repeating logic into dedicated private helper functions.
