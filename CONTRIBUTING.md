# Contributing to CoreCDTL

Thank you for your interest in contributing to CoreCDTL! We aim for maximum performance, minimum energy consumption, and high reliability. Your contribution should reflect these core values.

Please read our [Non-Commercial Ecosystem License](LICENSE.md) before starting any work.

---

## 1. Getting Started & Communication

We encourage contributors to discuss large changes or new features before opening a Pull Request (PR).

* **Small Fixes/Typos:** Feel free to open a PR directly.
* **Large Features/Architectural Changes:** Please open a **GitHub Issue** first to discuss the design and scope. **There is currently no external chat channel (Discord/Slack); all formal discussions should happen on GitHub Issues.**

### Core Development vs. External Modules

* **Core Code Contribution:** We welcome contributions aimed at **bug fixes, performance optimizations, and critical feature extensions** to the core runtime.
* **New External Modules/Plugins:** Adding entirely new, complex modules requires prior communication with the core team. **Only organization members are permitted to directly commit new module integration files (e.g., modifying `CMakeLists.txt`).**

---

## 2. Code Standards & Quality

CoreCDTL enforces strict coding standards to maintain its high-performance and low-leakage profile. **Contributions that do not adhere to these rules will be rejected.**

### A. Memory Management (The Zero-Leak Rule)

* **Resource and Memory Leaks are Strictly Forbidden.**
* Every malloc(), realloc(), or similar dynamic allocation **must** be paired with a corresponding free().
* **Long-Term Allocation:** If memory is intentionally allocated for the lifetime of the application (e.g., initial setup structures), this must be clearly documented. This rule also applies to custom memory management, such as **Arena allocation.**

### B. Formatting & Style

To ensure consistent and readable code across all platforms:

* **Indentation:** Use the **TAB character** for all indentation levels. (Contributors should configure their editor to display the TAB character as 4 spaces.)
* **Brace Style:** Use **Allman Style** for braces: The opening brace `{` must be on a new line for functions and all control blocks (`if`, `for`, `while`).
* **Max Line Length:** Keep lines to a maximum of **80 characters** for readability.
* **Build System:** You must not modify `CMakeLists.txt` files unless explicitly instructed or agreed upon with the core team.

### C. C Standard Compliance
* **C Standard:** CoreCDTL is written in **C11** (with C99 fallback compatibility).
* **Static Analysis:** Run `clang-format` before submitting. Tools like `cppcheck` are recommended.

### D. Testing & Validation
All contributions must be validated with **unit tests**.

* Every PR must include **tests for new features** or **regressions for bug fixes**.
* Tests are located in the `tests/` directory and use the **Unity test framework**.
* Run the full suite locally before submitting:
  ```bash
  make runTests
  ctest --output-on-failure
  ```
* **Zero test failures are required** before a PR will be considered.
* PRs will automatically trigger the test suite via **CI/CD pipelines**. If tests fail, the PR cannot be merged.
* Follow the **AAA Pattern** (Arrange → Act → Assert). Use `setUp()` and `tearDown() for initialization/cleanup if necessary.

---

## 3. Naming Conventions

All code must follow these specific naming conventions:

| Item Type                 | CoreCDTL Standard Rule        | Examples                                |
|---------------------------| ----------------------------- | --------------------------------------- |
| Structures (struct)       | lowercase snake_case          | `struct field_info` (without typedef)     |
| Enums (enum)              | UPPERCASE snake_case | `TYPE_INT`, `TYPE_FLOAT`                   |
| Typedef Structures/Enums  | lowercase snake_case + _t     | `field_type_t`, `field_info_t`, `type_info_t` |
| Typedef Function Pointers | lowercase snake_case + _fn    | `gateway_pm_get_list_fn`                  |
| Macros/Constants          | UPPERCASE ALL_CAPS            | `GATEWAY_DTLS_MSG_LEN`, `EVENT_BUS_API`     |

**Note:** Functions taking no parameters **must** explicitly use `void` (e.g., void bus_shutdown(void)).

---

## 4. Submitting a Pull Request (PR)

1. **Fork** the repository and create your feature branch (e.g., `git checkout -b feature/my-new-feature`). Use descriptive names, avoid `temp` or `test`.
2. Ensure your code adheres to **all coding standards and guidelines** described above.
3. **Testing:** You are required to run local tests against any module you modified.
    * Unit tests must pass before submitting.
    * If adding new functionality, corresponding new tests are required.
4. Open a PR targeting the `main` branch. Provide a clear description of the problem solved and the performance/reliability gains achieved.

**Commit Messages:** Use concise, descriptive commit messages. Following [Conventional Commits](https://www.conventionalcommits.org) is encouraged.

***Contributions that do not adhere to the Zero-Leak Rule or Code Standards will be closed until resolved.***

---

### Need More Guidance?

Detailed documentation on API usage, build processes, and in-depth developer information is available in the **Developer Guide**:

* [Contributing Guide](https://docs.corecdtl.com/core/contributing/)
