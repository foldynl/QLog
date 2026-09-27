# QLog Model Agent Instructions

These rules apply to `models/` and supplement the repository-root `AGENTS.md`.

- Preserve Qt model/view contracts and existing role, index, and selection behavior.
- Prefer targeted `dataChanged()` emissions over full model resets.
- Use the matching begin/end insert, remove, move, and reset notifications, and keep their ranges exact.
- Avoid `beginResetModel()` and `endResetModel()` unless the model genuinely cannot describe the change more narrowly.
- Do not emit model or domain signals in tight loops when the same semantics can be preserved by aggregating the change and emitting once.
- Avoid per-row database queries, repeated conversions, and repeated full-model scans. Evaluate model changes with realistic logbook or spot volume.
- Keep the model backed by its authoritative data source; do not add mirrored state or caches without a measured need and a clear invalidation path.
- Model source files follow adjacent instrumentation conventions and are not required to add `MODULE_IDENTIFICATION` or `FCT_IDENTIFICATION` unless explicitly requested.
- Prefer focused non-GUI tests for model logic when a regression test is warranted. Do not expose new production APIs solely for testing.
