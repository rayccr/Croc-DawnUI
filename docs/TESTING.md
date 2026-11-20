# Windows development checks

Configure with CROCD_ENABLE_SELFTEST=ON and CROCD_BUILD_TESTS=ON in a separate build/output directory. Set FLOWDROP_CROC_ENGINE_LIB to the matching DLL. Keep normal release switches OFF.

- CrocRegression.exe uses FLOWDROP_ENGINE_BACKEND=process and the generated FakeCroc test executable.
- CrocDllRegression.exe <existing writable test parent> <croc.exe path> uses a local relay.
- CrocDllLanRegression.exe <existing writable test parent> checks the in-process DLL LAN path and ports.
- CrocQueueRegression.exe <existing writable test parent> <croc.exe path> checks queue admission and recovery. Allow time for the 30-second retention case.
- Croc-DawnUI.exe --engine-selftest <new absolute test directory> checks controller-driven local transfer. The path must not already exist.

Set FLOWDROP_CONFIG_PATH, FLOWDROP_HISTORY_PATH and FLOWDROP_TASKS_PATH to isolated files for other UI experiments. Never use your real transfer history for tests. Local success does not imply cross-machine or poor-network acceptance.
