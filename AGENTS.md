<!-- codebase-memory-mcp:start -->
For structural codebase exploration, use the installed `codebase-memory` skill.
<!-- codebase-memory-mcp:end -->

# QLog Agent Instructions

## Scope and Priorities

- These instructions apply to all work in this repository.
- Keep repository-facing documentation, comments, identifiers, and developer messages in English.
- Follow the established style in adjacent code unless this document defines a stricter rule.
- Treat these rules as prospective. Do not rewrite unrelated legacy code merely to make it conform.
- Prefer the smallest semantically complete change that solves the verified root cause and preserves existing behavior.
- Correctness and data integrity are non-negotiable. Optimize performance and simplicity within those constraints.
- Treat performance, maintainability, and readability as joint design constraints. A fast patch that is difficult to maintain, or a clean-looking patch that adds avoidable runtime cost, is not acceptable.
- Directory-specific `AGENTS.md` files refine these rules for `ui/`, `models/`, `tests/`, and `res/sql/`. Rules not explicitly refined there continue to apply unchanged.

## Project Context

- QLog is a HAM radio logging application written in C++11 with Qt5 and Qt6 support.
- Supported platforms are Linux, Windows, and macOS.
- The build system is qmake.
- Operator build command: `cd build && qmake ../QLog.pro && make -j$(nproc)`.
- Run build and check commands from `build/` so generated artifacts do not spill into the repository root.
- Do not run a full compilation or the complete test suite unless the user explicitly asks. Report any verification that was not run.
- Main directories:
  - `core/`: application infrastructure and core logic.
  - `data/`: domain data, profiles, and shared data services.
  - `ui/`: dialogs, widgets, controllers, and Qt Designer forms.
  - `models/`: Qt SQL and item models.
  - `awards/`: award definitions and calculations.
  - `rig/`, `rotator/`, `cwkey/`: hardware integration.
  - `service/`: online services and network integrations.
  - `logformat/`: import and export formats.
  - `tests/`: Qt Test projects.
  - `i18n/`: translations.
  - `res/sql/`: database migrations.

## Working Rules

- A request to review, diagnose, explain, or design is read-only unless the user also requests implementation.
- Approval of a design or deferred plan does not authorize implementation in a later task without an explicit request to continue.
- For a non-trivial implementation, state a concise plan before editing, then implement it without unnecessary ceremony.
- When implementation is explicitly requested and the scope is clear, make the change; do not stop at another proposal or ask questions that do not affect the result.
- Implement exactly the requested or approved items. Do not silently broaden a numbered selection, a narrow fix, or a review into adjacent cleanup.
- Treat user corrections and rejected approaches as design constraints. Do not reintroduce a rejected solution without new evidence and a clear reason.
- Do not change wire protocols, database schemas, persisted formats, or public behavior unless the request requires it.
- Preserve existing manual workflows, persisted settings, defaults, and backward-compatible behavior unless the requested change explicitly replaces them.
- Preserve all user changes. Never revert or rewrite unrelated worktree changes.
- If an unexpected change overlaps the files or behavior needed for the task, stop and ask how to proceed. If it is unrelated, preserve it and work around it.
- Read-only Git inspection is allowed when useful. Do not perform state-changing Git operations unless the user requests them.
- When moving or renaming a file or directory already tracked by Git, always use `git mv`; do not use a filesystem-only move followed by delete/add.
- Do not use destructive commands such as `git reset --hard` or `git checkout --` unless explicitly requested.
- Use `rg` and `rg --files` for text and file searches.
- Preserve semantic diffs. Do not run broad formatters or create formatting-only churn in pre-existing code.
- For comment-only or similarly trivial non-behavioral edits, inspect the text and diff directly; do not run builds, tests, linters, or formatters.
- Do not update user documentation, the wiki, release notes, or changelogs for an ordinary code change unless the user explicitly requests documentation work.
- Separate verified facts from inference. Do not present a suspected cause, platform behavior, or hardware behavior as confirmed without evidence that actually covers it.

## Architecture Overview

### Database

- `LogDatabase::instance()` owns the database lifecycle.
- `DBSchemaMigration` manages schema versions through `res/sql/migration_NNN.sql` resources.
- `LogParam` is a static persisted-settings interface backed by `log_param` and a local cache.

### Core Patterns

- Signals and slots are the primary UI/core boundary.
- Qt SQL MVC is used throughout the application.
- Online services share behavior through `GenericQSOUploader`, `GenericQSLDownloader`, and `GenericCallbook`.
- Reuse existing data flow, QObject ownership, lifecycle, signals, and APIs before introducing a parallel mechanism.

## Implementation Quality

- Start with the simplest design that is semantically complete.
- Avoid new state, caches, timers, lifecycle management, signal fan-out, helper APIs, abstractions, defensive branches, or changed files unless a concrete behavior requires them.
- Prefer a direct fix at the source of incorrect state or behavior over a compensating mechanism elsewhere.
- Trace the complete data, signal, ownership, and lifecycle path before changing one layer. A local fix is incomplete if another layer still overrides, discards, duplicates, or tears down the state.
- Treat ownership, shared access, and destruction order as one design decision. Before adding a registry, global pointer, or other lifecycle bridge, check whether existing framework ownership already provides the required lifetime, choose one authoritative owner, and encode the invariant in the API so construction order cannot silently change behavior.
- Evaluate the real operator workflow and durable output, not only an intermediate preview, isolated widget, or happy path.
- Helpers must have a clear purpose: eliminate meaningful duplication, isolate a coherent rule, or materially improve readability.
- Do not introduce helpers that add redundant traversals, allocations, conversions, queries, copies, or hidden state.
- Keep helpers private unless an existing external consumer requires a wider API.
- Put validation, normalization, ranges, and other domain invariants in the authoritative domain type or shared path. Do not duplicate the same rule mechanically across callers.
- Every new enum value, flag, signal, API, or state field must have a concrete consumer in the requested behavior. Do not add unused scaffolding for possible future work.
- Performance is a first-class requirement. Consider CPU cost, memory use, allocations, database access, signal volume, startup cost, and UI responsiveness for every non-trivial change.
- Avoid work that can perceptibly block the GUI thread. Reuse an existing worker or asynchronous lifecycle when one exists; do not add threading machinery for work that is demonstrably cheap.
- For uncertain SQL, API, performance, or architecture decisions, use a short isolated probe or benchmark before editing production code.
- Before handoff, perform a simplification pass. Challenge every added line and mechanism, remove anything not required, and re-check that the minimal patch still covers the root cause.

## C++ and Qt Style

- Classes, enums, and type aliases use `PascalCase`.
- Methods, variables, parameters, and members use `camelCase`; do not use the `m_` prefix.
- Constants and enum values use `UPPER_SNAKE_CASE`.
- Keep code compatible with C++11. Do not introduce a later language feature without an explicitly requested project-wide standard change.
- Prefer Qt types, containers, ownership facilities, and APIs over STL equivalents where Qt provides a practical solution:
  - Prefer `QString`, `QByteArray`, `QList`, `QMap`, `QHash`, `QStringList`, `QPair`, `QFile`, and `QDir`.
  - Avoid unnecessary conversion between Qt and STL types.
  - Use STL when Qt has no suitable equivalent, an external API requires it, or the STL solution is clearly safer or more efficient.
- Prefer explicit types when they make ownership or behavior clearer. Use `auto` when the type is already obvious, for lambdas, or when spelling the type would add noise or force a less efficient construct.
- Pass non-trivial objects by `const &` unless ownership transfer or modification is intended.
- Mark non-mutating methods `const`.
- Prefer `const` locals when values do not change.
- Prefer guard clauses and early returns over deep nesting.
- Use range-for where appropriate; do not use `Q_FOREACH` or `std::for_each`.
- Do not introduce anonymous namespaces. Prefer private class functions, private static members, or class-local constants.
- Do not refactor an existing anonymous namespace unless it is part of the requested change.

## Formatting

- Use Allman braces.
- Put spaces inside conditions and loop parentheses:

```cpp
if ( condition )
{
    doSomething();
}
```

- Do not use braces for a single-statement body:

```cpp
if ( condition )
    doSomething();
```

## Logging and Identification

- Every new production `.cpp` file outside `models/` must place `MODULE_IDENTIFICATION` after its includes and before implementation code:

```cpp
MODULE_IDENTIFICATION("qlog.<folder>.<classname_lowercase>");
```

- Every new or modified out-of-line production function outside `models/` must start with `FCT_IDENTIFICATION;` wherever the macro can be used meaningfully, including constructors, destructors, slots, and private helpers:

```cpp
FCT_IDENTIFICATION;
```

- Test and model files follow their adjacent conventions and are not required to add these macros unless explicitly requested.
- Log diagnostically useful input parameters immediately after `FCT_IDENTIFICATION` when they help trace behavior:

```cpp
FCT_IDENTIFICATION;
qCDebug(function_parameters) << parameter1 << parameter2;
```

- Do not log secrets, credentials, large payloads, or high-frequency values without a concrete diagnostic need.
- Use `qCDebug(runtime)` for useful runtime state, decisions, transitions, and diagnostics.
- Runtime debug logging must be selective. Do not add routine noise, duplicate information, or logging in hot loops unless it is needed to diagnose that loop.
- Use `qCWarning(runtime)` for recoverable warnings and categorized critical logging for serious failures.
- Do not add uncategorized `qDebug()` or `qWarning()`, or use `std::cout` or `std::cerr` in production code.

## Signals and Slots

- Use new-style C++ connect syntax:

```cpp
connect(source, &Source::signalName, target, &Target::slotName);
```

- Do not add `SIGNAL()` or `SLOT()` macro connections.
- Prefer a direct signal-to-slot connection over a lambda that only forwards the same call. Use a lambda when argument adaptation or real local behavior is required.
- Avoid broad signal rewiring. Reuse an existing notification path when it already expresses the required lifecycle.

## Internationalization

- QLog is a multilingual application. Every string that can be shown to the user must be translatable, regardless of which directory or application layer produces it.
- Use `tr()` for every user-visible string in C++, including messages, errors, status text, model data, service responses, import/export feedback, and text passed later to GUI code. Do not limit this requirement to classes under `ui/`.
- User-visible text in `.ui` files must remain translatable.
- Do not update translation catalogs or edit `i18n/*.ts` files as part of ordinary feature, fix, or UI work.
- Translation catalogs are updated manually as a dedicated release-stage task. Preserve existing `.ts` files unless the user explicitly requests translation maintenance.

## Qt5 and Qt6 Compatibility

- Evaluate every code and UI change against both Qt5 and Qt6, including the different supported minor versions used by QLog. Do not assume that an API available in the newest Qt6 is available in Qt5 or an older Qt6.
- Prefer one API that works on both Qt5 and Qt6. Use `#if QT_VERSION` only when no common API is practical.
- Keep version-specific branches small, local, and behaviorally equivalent. Do not duplicate whole implementations merely to handle an API difference.
- Avoid or replace incompatible/deprecated APIs in new code:
  - Use `QRegularExpression`, not `QRegExp`.
  - Use `QString::asprintf()`, not `QString::sprintf()`.
  - Use `QString()`, not `QString::null`.
  - Use `QStringView` or `QString`, not `QStringRef`.
  - Use `QList`, not `QLinkedList`.
  - Use `QMultiHash`, not `QHash::insertMulti()`.
  - Use `QMultiMap`, not `QMap::insertMulti()`.
  - Use `QScreen` or `QApplication::primaryScreen()`, not `QDesktopWidget`.
  - Use `QFontMetrics::horizontalAdvance()` with a compatibility guard where required, not `width()`.
  - Use `QWheelEvent::angleDelta().y()`, not `delta()`.
  - Use `QMouseEvent::position().toPoint()` with a Qt-version guard where required, not Qt6-sensitive `pos()` code.
  - Use `Qt::MiddleButton`, not `Qt::MidButton`.

## Memory Management

- Prefer Qt parent-child ownership for QObject trees.
- Qt requires a `QObject` and its parent to live in the same thread. Preserve this invariant when assigning a parent or moving an object between threads.
- Do not manually delete a QObject owned by a parent.
- Raw pointers are acceptable for non-owning references.
- Use an owning smart pointer only when ownership is explicit and Qt parent ownership is not appropriate.
- Avoid ownership wrappers or cleanup helpers that duplicate Qt lifecycle management.

## Event Loops and Reentrancy

- Do not introduce `QCoreApplication::processEvents()`, a nested `QEventLoop`, or `Qt::BlockingQueuedConnection` unless a verified requirement makes it unavoidable.
- When one of these mechanisms is necessary, keep it local and document the concrete reentrancy or deadlock constraint that justifies it.

## SQL and Database Rules

- Use prepared, parameterized queries and `bindValue()` for data values.
- Never concatenate user-controlled or data values into SQL. SQL identifiers that cannot be bound must come from validated, controlled values.
- Check `prepare()`, `exec()`, transaction, commit, rollback, and database-open results when their failure affects correctness.
- Log SQL failures with `qCWarning(runtime)` and `query.lastError()`.
- Propagate write failures to the caller when correctness depends on the write.
- Roll back transactions after failed partial writes.
- A `QSqlDatabase` connection and queries created from it may be accessed only from the thread that owns that connection. Do not share a connection or query object across threads; create a separate named connection in each thread that needs database access.
- Every schema change must follow the complete migration chain defined in `res/sql/AGENTS.md`.

## Project Integration

- Inspect an adjacent implementation before adding a new class or file so ownership, instrumentation, naming, and build integration follow the local pattern.
- Register new production `.cpp`, `.h`, and `.ui` files in the appropriate `SOURCES`, `HEADERS`, and `FORMS` sections of `QLog.pro`.
- Register new embedded resources in the appropriate `.qrc` file.
- When renaming a class, method, slot, signal, widget, or resource, update declarations, definitions, `.ui` connections, build files, and all call sites consistently.
- Do not leave unused declarations, enum values, signals, resources, compatibility branches, or partially wired feature paths.
- Do not edit qmake, `uic`, `moc`, or `rcc` generated output such as `Makefile`, `ui_*.h`, `moc_*`, `qrc_*`, object files, or files under `build/`. Edit the authoritative source and regenerate output in the build directory.
- Do not add a third-party library, bundled source tree, runtime executable, system service, or new package dependency without explicit approval.
- Before adding an approved dependency, verify its Qt5/Qt6 compatibility, supported-platform availability, packaging impact, maintenance status, and license compatibility.

## Performance

- Prefer algorithms whose cost scales predictably with logbook and spot volume.
- Avoid N+1 database access, repeated parsing, repeated regular-expression construction, repeated container scans, unnecessary temporary containers, and avoidable deep copies.
- Batch database work, model notifications, and signal emission where semantics allow.
- Keep data close to its authoritative owner and avoid mirrored state that requires synchronization.
- Do not add a cache or precomputed index without a demonstrated need, a measurable benefit, and a clear invalidation lifecycle.
- Measure or use a focused probe before accepting a more complex optimization. Retain the simpler implementation when the performance benefit is speculative or negligible.
- Optimize the actual hot path, but do not trade correctness or maintainability for speculative micro-optimization.

## Comments and Documentation

- Add comments where they preserve information that the code alone cannot express clearly: intent, invariants, non-obvious constraints, compatibility reasons, protocol requirements, or why a simpler-looking approach is incorrect.
- Comments should explain why, not narrate what the next line does.
- Keep comments concise and update or remove them when the underlying behavior changes.
- Do not add comments to obvious code merely to increase comment coverage.
- Code comments are part of maintainability; external user documentation is maintained manually and is out of scope unless explicitly requested.

## Verification and Handoff

- The operator normally performs compilation and full-suite verification.
- For a behavior change, run only the focused checks justified by its risk unless the user asks for broader verification.
- State exactly what each check proves. A successful build does not prove runtime behavior, an offscreen Linux test does not prove macOS or Windows behavior, and a simulator does not prove physical hardware behavior.
- Report unverified platform, hardware, interactive GUI, signing, packaging, or external-service behavior explicitly rather than implying full validation.

## Definition of Done

A change is ready for handoff only when:

- the requested behavior is complete through its full data, signal, ownership, persistence, and user-visible path where applicable;
- the final simplification pass found no unnecessary state, helper, API, branch, dependency, changed file, or formatting churn;
- new files and resources are integrated into qmake and resource manifests, and generated files were not edited;
- existing behavior, stored data, manual workflows, backward compatibility, and unrelated worktree changes remain preserved unless the request explicitly changes them;
- applicable Qt5/Qt6, performance, thread-affinity, SQL, i18n, and failure-path concerns were reviewed; and
- focused validation was performed in proportion to risk, with every unrun or unverified check reported accurately.

## Review Checklist

Before handing off code, inspect the changed scope for:

- missing `MODULE_IDENTIFICATION` or `FCT_IDENTIFICATION` where required;
- useful `qCDebug(function_parameters)` coverage and excessive `qCDebug(runtime)` noise;
- new anonymous namespaces or avoidable public helpers;
- unnecessary STL use where a practical Qt type or API exists;
- inefficient helpers, redundant passes, copies, allocations, queries, or hidden state;
- avoidable GUI-thread work or performance that degrades with realistic logbook size;
- incomplete end-to-end signal, ownership, state, or lifecycle handling;
- regressions to existing manual workflows, persisted settings, defaults, or backward compatibility;
- Qt5-only or Qt6-only APIs without a compatibility path;
- a QObject parent/child relationship that crosses threads;
- shared cross-thread `QSqlDatabase` connections or queries;
- avoidable `processEvents()`, nested event loops, or blocking queued connections;
- ignored SQL failures or incomplete rollback handling;
- user-visible C++ strings from any application layer that do not use `tr()`, or user-visible `.ui` text that is not translatable;
- old-style signal/slot connections in new code;
- incidental `.ts` file changes;
- missing comments for non-obvious constraints or comments that merely restate code;
- new files missing from `QLog.pro` or resources missing from a `.qrc` file;
- edits to generated files or unapproved new dependencies;
- unused scaffolding or partially wired API, signal, enum, or state additions;
- validation claims that exceed what the performed checks demonstrate;
- behavior changes outside the requested scope;
- unrelated formatting churn; and
- mechanisms that can be removed during the final simplification pass.

When the user asks for a review, report findings first, ordered by severity, with file and line references when available. Examine failure paths, data integrity, crashes, loops, compatibility, performance, and user-visible side effects before polishing the happy path.
