# QLog UI Agent Instructions

These rules apply to `ui/` and supplement the repository-root `AGENTS.md`.

## Designer and C++ Responsibilities

- Put all statically known widgets, actions, layouts, spacers, tab order, and suitable properties in the Qt Designer `.ui` file whenever practical.
- All UI elements must be present and visible while editing the `.ui` file. Do not mark a widget invisible in the `.ui` file merely because it is hidden at runtime.
- Control initial or conditional runtime visibility in C++ after `setupUi()` with `setVisible()`, `setHidden()`, or the existing state-update path.
- Create widgets in C++ only when their number, type, or ownership is genuinely dynamic at runtime.
- Put straightforward static widget connections in the `.ui` file when that is clearer than C++.
- Keep dynamic, lifetime-dependent, conditional, lambda-based, and business-logic connections in C++.
- Keep presentation-only properties in the `.ui` file. Keep behavior and runtime state decisions in C++.

## UI and UX

- UI behavior must remain compatible with both Qt5 and Qt6.
- Keep the UI clear, low-friction, intentional, and consistent with surrounding QLog dialogs and widgets.
- Preserve familiar workflows and control placement unless a requested behavior requires a redesign. QLog users expect a clean, stable, and predictable interface.
- Prefer established amateur-radio terminology over software, database, protocol, or implementation terminology in labels, actions, tooltips, and user-facing messages.
- Keep operator-facing text concise and action-oriented. Put technical diagnostic detail in categorized logs rather than exposing internal terminology in the GUI.
- Preserve keyboard navigation, sensible tab order, and standard Qt interaction patterns.
- Judge UI changes in the complete operator workflow, including what remains visible, printable, persisted, or actionable after a dialog or preview closes.

## Layout, Fonts, and Display Scaling

- Use layouts and `QSizePolicy` instead of fixed positions or fixed widget sizes whenever practical.
- Respect the system font, palette, widget style, and dark/light appearance. Do not assume a specific font family, point size, theme, or platform style.
- Keep layouts usable with high-DPI scaling and with translated text substantially longer than English.
- Prefer scalable or existing theme-aware resources. Avoid adding raster assets that only look correct at one scale unless the design specifically requires them.
- Use hardcoded geometry or pixel metrics only for a concrete visual requirement that layouts, style metrics, or content-based sizing cannot express clearly.

## Internationalization

- Write complete translatable sentences or labels. Do not assemble user-visible text from separately translated fragments; use placeholders such as `%1` where values must be inserted.
- Keep user-visible `.ui` properties translatable unless the value is intentionally language-independent.
- When changing UI text or layout, account for longer translations and avoid unnecessary fixed widths, clipping, and truncation.
- Do not edit `i18n/*.ts` files during ordinary UI work; translation catalogs are updated manually at the release stage unless explicitly requested.

## UI Verification

- Do not add GUI, widget, or end-to-end UI tests unless the user explicitly requests them.
- For changed `.ui` files, validate the XML and review the Designer structure and connections directly.
- Report interactive behavior, scaling, theme, and platform combinations that were not actually verified.

Before handoff, also check for static UI structure or simple connections unnecessarily implemented in C++, runtime-hidden widgets marked invisible in Designer, technical terminology exposed to operators, fragmented translatable strings, and layouts that assume English text length.
