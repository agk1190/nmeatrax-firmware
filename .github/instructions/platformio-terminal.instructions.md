---
description: "Use when running PlatformIO build, upload, monitor, inspect, test, or any pio CLI command in this firmware workspace. Enforces terminal selection for PlatformIO operations."
name: "PlatformIO Terminal Rule"
---
# PlatformIO Terminal Rule

- For any command that uses the pio CLI, use the VS Code terminal named PlatformIO CLI.
- Do not run pio commands in PowerShell (or other non-PlatformIO terminals) unless the user explicitly asks.
- Before running a pio command, confirm the active terminal is PlatformIO CLI.
- If PlatformIO CLI is unavailable, state that clearly and ask whether to proceed in another terminal.
- Applies to examples such as: pio run, pio run -t upload, pio device monitor, pio test, pio pkg, and pio project commands.
