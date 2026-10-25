# Agent instructions

Read docs/SAFETY.md and docs/AGENT-GUIDE.md before editing. Default tasks are offline source/tests only. Do not load a module, access BARs, change bindings, reboot, modify boot settings or run firmware without explicit authorization and verified recovery access.

- `make test`: CPU-only, firmware-free tests.
- `make test-private ASSETS=...`: CPU-only protocol model with private verified inputs.
- `make module`: compile only, execution disabled by default.
- Do not weaken guards to make another board run.
- Keep runtime source and model tests synchronized; label simulations vs hardware evidence.
- Never commit local/, assets/, binaries, dumps, full inventories, serials, UUIDs, IPs, credentials or external firmware.
- No automatic public hardware runner in CI.
- Preserve attribution/license texts when adapting upstream code.
- Do not change benchmark methodology while claiming UI-only improvements.
