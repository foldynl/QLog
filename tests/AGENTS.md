# QLog Test Agent Instructions

These rules apply to `tests/` and supplement the repository-root `AGENTS.md`.

- Each test suite is a standalone qmake project collected by `tests/tests.pro`.
- Add unit tests by default only for non-GUI classes and logic with minimal external dependencies.
- Do not add GUI, widget, or end-to-end UI tests unless the user explicitly requests them.
- Prefer a focused regression test that covers the root cause, failure path, and relevant boundary without duplicating the implementation.
- Reuse existing fixtures, stubs, and adjacent test patterns before adding new test infrastructure.
- Do not create production helpers, public APIs, abstractions, or behavior branches solely to make a test convenient.
- Keep tests deterministic and independent of network access, physical radio hardware, local operator configuration, wall-clock timing, and execution order unless the test is explicitly scoped to such integration.
- Test and stub source files follow adjacent instrumentation conventions and are not required to add `MODULE_IDENTIFICATION` or `FCT_IDENTIFICATION` unless explicitly requested.
- Run only focused tests justified by the change unless the user requests a broader suite. Report exactly which suites and cases ran and what remains unverified.
