# Agents.md

## Purpose

This file defines the working rules for any agent contributing to this project. The agent must follow these instructions strictly and avoid making unrelated or unsolicited changes.

## Core Working Rules

1. **Do not code unless explicitly asked**

   The agent must not write, modify, refactor, delete, or generate code unless the user has explicitly requested coding work.

2. **Stay focused on the requested task**

   The agent must work only on the specific task that was requested.

   Do not:
    - Improve unrelated code.
    - Refactor code unless requested.
    - Remove comments unless requested.
    - Rename files, classes, methods, or variables unless requested.
    - Change formatting outside the requested scope.
    - Make product, architecture, or UX changes that were not requested.

3. **Preserve existing intent and context**

   The agent must respect the existing product logic, hierarchy, naming, structure, and business intent of the project. Any change must fit into the current product behavior and project organization.

## Android Development Rules

1. **Java and C/C++ only**

   Android framework development (Activities, Fragments, Services, etc.) must be done in **Java**, not Kotlin. Native logic and performance-critical components must be implemented in **C/C++** via JNI.

2. **Brace style**

   Use a new line for braces, unless the entire statement can cleanly fit on a single line.

   Preferred style:

   ```java
   if (condition)
   {
       doSomething();
   }
   else
   {
       doSomethingElse();
   }
   ```

   This rule also applies to:
    - `else`
    - `catch`
    - `finally`
    - `while` in `do-while`
    - class declarations
    - method declarations
    - loops
    - conditional blocks
    - anonymous classes

   Example:

   ```java
   try
   {
       runTask();
   }
   catch (Exception e)
   {
       handleError(e);
   }
   finally
   {
       cleanup();
   }
   ```

   Example for `do-while`:

   ```java
   do
   {
       processItem();
   }
   while (hasMoreItems());
   ```

3. **Single-line exception**

   A block may remain on one line only when the whole statement is short, clear, and already fits naturally on one line.

   Example:

   ```java
   if (isReady) { start(); }
   ```

4. **No Native-to-Java calls**

   It is strictly forbidden to call Java methods or access Java fields from C/C++ code. JNI usage must be strictly unidirectional: Java calls Native. Native code must never call back into Java.

## Project Paths

*Note: All paths below are relative to the Android project root (`external_server/android`).*

The Git repository root is located at:

`../../`

The corresponding Python version that the Android application should align with is located at:

`../python`

## Android SOW Documentation

The Android SOW document is located at:

`../../documentations/androidSOW.md`

The agent must review this file before making meaningful Android-related changes.

If a meaningful product, architecture, flow, requirement, or behavior change is made, the agent must update `androidSOW.md` accordingly, unless the user explicitly says not to update it.

## Product Discipline and Hierarchy

The agent must maintain strict product discipline and respect the existing hierarchy of the project.

Before making any change, the agent must consider:

- The current product behavior.
- The existing project structure.
- The intended user flow.
- The relationship between screens, components, services, and data models.
- Whether the requested change belongs in the specific file or layer being modified.

The agent must not flatten, bypass, or redesign the project hierarchy unless explicitly requested.

## Scope Control

When performing a task, the agent must:

1. Identify the exact requested change.
2. Modify only the files required for that change.
3. Avoid unrelated cleanup.
4. Avoid opportunistic improvements.
5. Keep existing comments, behavior, and structure unless the task requires changing them.
6. Explain any required additional change before making it, when possible.

## Communication Rules

The agent should be direct and task-focused.

When responding, the agent should:

- State what was changed.
- Mention which files were affected.
- Mention whether `androidSOW.md` was updated, if relevant.
- Clearly state if no code changes were made.

The agent should not suggest broad improvements, rewrites, or refactors unless the user asks for them.