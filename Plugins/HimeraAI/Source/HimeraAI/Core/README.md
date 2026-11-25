# Vendored HIMERA algorithm core

Do not edit these files here. They are copied from the AI repository
(`HIMERA-Autonomous-AI`) so that there is one source of truth.

- `include/himera/*.hpp`, `src/*.cpp` come from the commit named in `VENDORED_FROM.txt`.
- Update with `python Tools/sync_core.py <path-to-ai-repo>` and detect drift with
  `python Tools/sync_core.py --check <path-to-ai-repo>`.
- Nothing in this folder may include an Unreal header.
