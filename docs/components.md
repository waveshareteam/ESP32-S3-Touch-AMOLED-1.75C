# Components

ESP-IDF examples currently include local board, UI, and helper components under each example. The CI matrix keeps these local components in place and verifies them against ESP-IDF `v5.5.4` and `v6.0.2`.

Future component work should prefer managed Waveshare and Espressif components when an equivalent component is available and compatible with the selected CI matrix.

Keep repository-local glue such as board-specific demo composition, temporary compatibility code, and example-only assets near the example that consumes it. Reusable BSP, display, touch, sensor, audio, and bus fixes should move to the shared component source first when possible.