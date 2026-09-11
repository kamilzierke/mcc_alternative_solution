# AI Agent Workflow Standard

Reusable operating standard for coding agents working in repositories.

## 1. Mission

The agent should move a task from evidence to implementation, validation and a concise handoff. It must preserve user control over machine-wide, privileged, destructive and hardware-dependent operations.

The agent is autonomous inside the workspace, but consent-gated outside it.

## 2. Operating Modes

Every task starts in one of these modes:

- `AUDIT`: read-only analysis; no file edits.
- `IMPLEMENT`: make the smallest change that tests the current hypothesis.
- `VERIFY`: run focused validation and inspect evidence.
- `RELEASE`: prepare tags, packages or publishing only after explicit approval.

The agent must state the current mode before acting when the mode is not obvious.

## 3. Hard Boundaries

The agent may do without additional approval inside the workspace:

- read and edit source, tests and documentation;
- run local unit tests, linters, type checks and syntax checks;
- use simulators, mocks, fixtures and static test data;
- inspect local logs, diffs and generated test reports;
- create temporary artifacts in an ignored `artifacts/` directory.

The agent must stop and ask for approval before:

- using administrator/UAC privileges;
- accessing physical devices, serial ports, cameras, GPUs, drivers or hardware sensors;
- installing software or dependencies from the network;
- changing global PATH, PowerShell policy, registry, services or scheduled tasks;
- modifying files outside the workspace;
- deleting user data, installed copies, logs or unknown files;
- starting a process that must remain running after the current interaction;
- committing, pushing, tagging, changing branches or publishing a release;
- sending data to external services or uploading diagnostics.

The approval request must name the exact operation, target, reason and expected side effects.

## 4. Repository Intake

Before the first edit, collect only the evidence needed to form one local hypothesis:

1. `git status`, current branch and recent commits;
2. repository instructions and contribution/security documents;
3. the named file, symbol, failing test or command;
4. nearby implementation and one neighboring test/call site;
5. available validation command.

Do not perform broad exploration before identifying the controlling code path.

Record:

```text
Repository:
Branch/commit:
Task anchor:
Current behavior:
Expected behavior:
Hypothesis:
Cheapest check that can disconfirm it:
Planned first edit:
```

## 5. Change Protocol

### Phase A: Baseline

- Preserve unrelated user changes.
- Identify the narrowest test or check.
- Do not change configuration with credentials, hardware assignments or personal paths.
- Prefer a reversible, local edit.

### Phase B: Implementation

- Follow existing architecture and naming.
- Keep public behavior compatible unless a breaking change is intentional.
- Add tests proportional to risk.
- Keep unrelated refactors out of the patch.
- Add documentation for user-visible behavior.

### Phase C: Immediate Verification

After the first substantive edit, run the narrowest executable check immediately:

1. failing behavior or focused test;
2. targeted unit test;
3. narrow compile/type/lint check;
4. diff inspection only when no executable check exists.

If it fails, repair the same slice and rerun the same check before expanding scope.

### Phase D: Completion

Run, where available:

```text
syntax/compile check
focused tests
full synthetic suite
diff check
repository status
```

Do not claim hardware, GUI visual, network or release validation from synthetic tests.

## 6. Validation Tiers

Use explicit evidence levels:

### Tier 1: Synthetic

- no network;
- no administrator rights;
- no physical hardware;
- deterministic mocks, simulators and static data;
- suitable for CI.

### Tier 2: Local integration

- local GUI event loop;
- local files and bundled resources;
- optional local subprocesses;
- no external device unless approved.

### Tier 3: Hardware/system

- COM/USB devices;
- LHM, drivers or GPU telemetry;
- UAC/admin rights;
- scheduled tasks or system services.

Tier 3 requires explicit user approval and must report device identity, privilege level, duration, cleanup and final connection state.

## 7. Configuration and Secrets

- Keep machine-specific config ignored.
- Maintain a public `config.example.*`.
- Never print or commit API keys, tokens, passwords, serial numbers or full hardware inventories.
- Use temporary test configs and restore or delete them only with clear provenance.
- Use atomic writes and backup recovery for important configuration.
- Test legacy configuration formats before changing schemas.

## 8. GUI and Hardware Rules

For GUI work:

- keep slow I/O, network, COM and hardware calls out of the UI thread;
- use bounded event processing or the framework's event loop;
- provide visible states: idle, connecting, ready, updating, unavailable and error;
- provide draft/apply/revert/save semantics for configuration editors;
- test narrow windows, high DPI, long labels, keyboard focus and accessibility;
- distinguish static preview from live device preview.

For hardware work:

- validate all devices before opening the first one when possible;
- isolate transport, queue, reconnect and shutdown per device;
- one failed device must not stop healthy devices;
- always test cleanup, including port closure and worker termination;
- never assume administrator rights are available.

## 9. Multi-Display and Multi-Target Rules

Each target should own:

- configuration context;
- theme/render state;
- transport queue and worker;
- connection state;
- reconnect policy;
- history buffers;
- target-specific sensor bindings.

Shared data providers may cache identical reads, but cache keys must include all entity arguments and stable IDs.

## 10. Release Protocol

A release requires explicit approval for tag/push/publish.

Recommended sequence:

1. update changelog, docs and version;
2. run source synthetic suite;
3. build the packaged runtime in a clean environment;
4. run tests through the packaged interpreter;
5. create checksum;
6. generate release notes from the changelog;
7. create a version-matching tag;
8. publish the release asset;
9. verify the downloaded asset and test it outside the development checkout.

The release workflow should reject a tag that does not match the version file.

A portable ZIP is not a Windows installer. Document separately whether the artifact is:

- source archive;
- portable package;
- installer;
- signed installer;
- update package.

## 11. Agent Checkpoints

The agent must report after each phase:

```text
Phase:
Files changed:
Evidence collected:
Commands/tests:
Result:
Known risks:
Operations requiring approval:
Next step:
```

Before final response, report:

- exact files changed;
- tests run and results;
- what was not tested;
- remaining risks;
- whether any external/privileged operation occurred;
- suggested follow-up.

## 12. Reusable Task Prompt

Use this prompt when delegating work to an agent:

```text
Work in the current workspace only unless I explicitly approve otherwise.

Goal:
[describe user-visible outcome]

Scope:
[files/modules/features included]

Out of scope:
[unrelated refactors and risky operations]

Autonomy:
You may inspect/edit workspace files and run synthetic tests autonomously.
You must ask before using admin/UAC, hardware/COM/GPU/LHM, network installs,
system settings, files outside the workspace, destructive operations, git commit,
tag, push or release publishing.

Validation:
Run focused checks after the first edit, then the relevant full synthetic suite.
Do not claim hardware or visual validation unless it was actually performed.

Compatibility:
Preserve existing public formats and behavior unless a migration is documented.

Reporting:
After each phase report changed files, commands, results, risks and the next step.
Stop for approval whenever an operation crosses the workspace boundary.
```

## 13. Review Checklist

### Correctness

- [ ] Root cause addressed, not only symptom patched.
- [ ] Error and timeout paths are explicit.
- [ ] Concurrency and shutdown behavior are tested.
- [ ] Cache keys include all relevant identity/arguments.
- [ ] Legacy behavior is covered.

### Safety

- [ ] No secrets in source, logs or artifacts.
- [ ] No unapproved external or privileged operation.
- [ ] No destructive cleanup of unknown files.
- [ ] User changes were preserved.

### Maintainability

- [ ] Tests cover the new contract.
- [ ] Documentation matches implementation.
- [ ] User-visible changes are in changelog/docs.
- [ ] Temporary artifacts are ignored or clearly separated.

### Release

- [ ] Version and tag match.
- [ ] Clean packaged-runtime tests pass.
- [ ] Checksum exists.
- [ ] Asset type is clearly documented.
- [ ] Upgrade and rollback behavior are documented.
