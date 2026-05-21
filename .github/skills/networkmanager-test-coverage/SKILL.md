---
name: networkmanager-test-coverage
description: 'Create and improve NetworkManager L1/L2 tests and increase coverage. Use for adding new unit/integration tests, extending existing test suites, updating test CMake wiring, and validating coverage impact for libnm/rdk/legacy paths.'
argument-hint: 'Describe feature/path to test, target layer (L1 or L2), and backend (rdk/libnm/legacy).'
user-invocable: true
---

# NetworkManager Test Coverage Skill

## What This Skill Does
This skill helps add and improve test coverage for this repository with the same patterns already used in current tests.

It is specific to:
- L1 tests in `tests/l1Test`
- L2 tests in `tests/l2Test/{rdk,libnm,legacy}`
- Existing mock/wrap style in `tests/mocks`

## When To Use
- Add tests for a new API added to `INetworkManager`/JSON-RPC.
- Increase coverage for edge cases in connectivity, STUN, Wi-Fi, or interface state logic.
- Add backend-specific behavior tests (RDK IARM vs GNOME libnm).
- Validate that CMake test targets include new test sources.

## Current Repository Test Patterns (Baseline)
- L1 examples:
  - `tests/l1Test/l1_test_connectivity.cpp`
  - `tests/l1Test/l1_test_stunclient.cpp`
- L2 RDK examples:
  - `tests/l2Test/rdk/l2_test_rdkproxy.cpp`
  - `tests/l2Test/rdk/l2_test_rdkproxyEvent.cpp`
- L2 libnm examples:
  - `tests/l2Test/libnm/l2_test_libnmproxy.cpp`
  - `tests/l2Test/libnm/l2_test_libnmproxyWifi.cpp`
- L2 legacy examples:
  - `tests/l2Test/legacy/l2_test_LegacyPlugin_NetworkAPIs.cpp`
  - `tests/l2Test/legacy/l2_test_LegacyPlugin_WiFiManagerAPIs.cpp`

## Procedure

### 1. Decide Test Layer
Choose L1 when testing isolated classes/logic with minimal framework setup.

Choose L2 when validating plugin method registration, JSON-RPC behavior, lifecycle paths, or backend integration logic with mocks.

### 2. Map Code Path To Test
Trace method flow using this order:
1. `plugin/NetworkManagerJsonRpc.cpp`
2. `plugin/NetworkManagerImplementation.cpp`
3. Backend file:
   - `plugin/rdk/NetworkManagerRDKProxy.cpp`, or
   - `plugin/gnome/NetworkManagerGnomeProxy.cpp` (+ related gnome files)

### 3. Add/Extend L1 Tests
1. Add or extend source in `tests/l1Test`.
2. Keep tests focused and deterministic; avoid external network dependency.
3. If new file is added, include it in `tests/l1Test/CMakeLists.txt` under `class_l1_test` (or the relevant test target).
4. Follow existing `TEST_F` naming style and assert both success and failure/invalid input paths.

### 4. Add/Extend L2 Tests
1. Pick backend folder:
   - `tests/l2Test/rdk`
   - `tests/l2Test/libnm`
   - `tests/l2Test/legacy`
2. Reuse existing fixture style that sets up:
   - plugin instance
   - handler/dispatcher/service mocks
   - backend wrappers/mocks from `tests/mocks`
3. Add tests for:
   - registered method presence
   - JSON parameter validation
   - success + error return paths
   - notifications/events where relevant
4. Update the corresponding backend `CMakeLists.txt` with new test sources.

### 5. Keep CMake Wiring Correct
Verify:
- `tests/l2Test/CMakeLists.txt` still routes correctly via `ENABLE_GNOME_NETWORKMANAGER` and `ENABLE_LEGACY_PLUGINS`.
- New test files are added only to the correct backend target.
- `--coverage` flags remain enabled for coverage-producing targets.

### 6. Run Build and Tests
For CMake projects, use CMake tools integration:
1. Build with CMake build tooling.
2. Run tests with CMake test tooling.
3. Fix compile/test failures before reporting completion.

### 7. Coverage Uplift Checklist
For each changed feature, ensure tests cover:
- valid input path
- invalid/missing input path
- backend unavailable/failure path
- state transition/event path (if applicable)
- regression for previously fixed bug

## Quality Bar
- No flaky timing/network dependency in unit tests.
- Clear, behavior-oriented test names.
- Minimal duplication of fixture code.
- New tests should fail before fix and pass after fix whenever feasible.

## Expected Output From This Skill
Return:
1. files added/updated
2. why each test was added
3. build/test result summary
4. coverage delta summary (if measurable in environment)