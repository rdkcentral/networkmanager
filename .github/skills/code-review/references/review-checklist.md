# Code Review Checklist for Telemetry Embedded Systems

## General Requirements

- [ ] Code follows project naming conventions
- [ ] Comments explain WHY, not WHAT
- [ ] No debug/dead code committed
- [ ] Commit messages are meaningful
- [ ] Changes align with PR description

## Memory Safety (C/C++)

### Allocations
- [ ] All `malloc`/`calloc`/`realloc` return values checked
- [ ] Every allocation has a corresponding `free`
- [ ] No memory leaks on error paths
- [ ] Error paths clean up in reverse order
- [ ] Use of `strdup` is justified (prefer stack when possible)

### Pointers
- [ ] Pointers initialized to NULL
- [ ] NULL checks before dereference
- [ ] No use-after-free scenarios
- [ ] No double-free possible
- [ ] Pointer invalidation after free (`ptr = NULL`)

### String Operations
- [ ] `strcpy` → replaced with `strncpy` or bounds-checked
- [ ] `sprintf` → replaced with `snprintf`
- [ ] `strcat` → buffer size validated
- [ ] `gets` → NEVER used (immediate rejection)
- [ ] String buffers always NULL-terminated

### Buffer Safety
- [ ] Array bounds validated before access
- [ ] Loop conditions prevent overruns
- [ ] `memcpy` size validated, no overlap
- [ ] Fixed-size buffers avoid magic numbers (use sizeof)

## Thread Safety

### Shared Data
- [ ] Shared variables protected by mutex
- [ ] Lock ordering is consistent (prevent deadlocks)
- [ ] Critical sections are minimal
- [ ] Read-only access considered for atomics

### Synchronization
- [ ] Mutex initialization checked (`pthread_mutex_init`)
- [ ] Mutexes properly destroyed on cleanup
- [ ] Condition variables paired with predicates
- [ ] No locks held across blocking operations unless necessary

### Thread Management
- [ ] Thread creation specifies stack size
- [ ] Thread joins or detaches (no zombie threads)
- [ ] Thread-local storage used appropriately
- [ ] No busy-wait loops (use condition variables)

## Resource Constraints

### Memory Usage
- [ ] Prefer stack allocation over heap for small objects
- [ ] Memory pools considered for frequent allocations
- [ ] Large buffers justified by requirements
- [ ] No unbounded growth (arrays, queues, caches)

### CPU Usage
- [ ] Algorithms are O(n) or better where possible
- [ ] No unnecessary loops or redundant operations
- [ ] Expensive operations avoided in hot paths
- [ ] Consider caching for repeated calculations

## Error Handling

### Return Values
- [ ] All function return values checked
- [ ] Error codes are meaningful and documented
- [ ] Errors propagated to caller when appropriate
- [ ] No silent failures (all errors logged)

### Logging
- [ ] Error conditions logged with context
- [ ] Log levels appropriate (ERROR, WARN, INFO, DEBUG)
- [ ] No sensitive data in logs
- [ ] Logs sufficient for field debugging

### Failure Modes
- [ ] All error paths tested
- [ ] System degrades gracefully
- [ ] No unbounded retries
- [ ] Timeouts implemented where needed

## API Design

### Backward Compatibility
- [ ] Existing function signatures unchanged (or versioned)
- [ ] Struct layouts maintain ABI compatibility
- [ ] Enum values not reordered
- [ ] New APIs clearly documented

### Function Design
- [ ] Functions do one thing well (single responsibility)
- [ ] Function names are descriptive
- [ ] Parameter validation at entry
- [ ] Output parameters clearly documented

### Header Files
- [ ] Include guards present
- [ ] Minimize dependencies (forward declarations)
- [ ] Public vs private APIs separated
- [ ] No implementation in headers (except inline)

## Platform Independence

### Portable Code
- [ ] No platform-specific types without abstraction
- [ ] Endianness handled explicitly if needed
- [ ] Size assumptions avoided (use sizeof, stdint.h)
- [ ] Conditional compilation minimized

### System Calls
- [ ] System calls error-checked
- [ ] Alternatives available for missing APIs
- [ ] File paths use portable separators
- [ ] Network byte order handled for network data

## Build System

### Makefile Changes (*.am)
- [ ] Dependencies complete and minimal
- [ ] Compiler flags justified
- [ ] Link order correct
- [ ] No absolute paths

### Configuration (*.ac)
- [ ] Feature detection used (not hard-coded)
- [ ] Optional dependencies handled
- [ ] Cross-compilation supported
- [ ] Platform detection accurate

## Testing

### Unit Tests
- [ ] New functionality has unit tests
- [ ] Edge cases covered
- [ ] Negative tests included (error paths)
- [ ] Mocks used appropriately (not excessively)

### Test Quality
- [ ] Tests are deterministic (no flakiness)
- [ ] Test names describe what is tested
- [ ] Assertions have meaningful messages
- [ ] Setup/teardown prevent state leaks

### Coverage
- [ ] All new functions tested
- [ ] Branch coverage >80% for new code
- [ ] Critical paths have integration tests
- [ ] Regression tests for bug fixes

## Documentation

### Code Documentation
- [ ] Public functions documented (purpose, params, return)
- [ ] Complex algorithms explained
- [ ] Assumptions documented
- [ ] TODOs tracked with tickets

### External Documentation
- [ ] README.md updated if needed
- [ ] API documentation updated
- [ ] Configuration examples provided
- [ ] Migration guide for breaking changes

## Embedded-Specific Concerns

### Telemetry 2.0 Framework
- [ ] Profile configurations validated against schema
- [ ] Report generation doesn't block data collection
- [ ] Scheduler changes maintain timing guarantees
- [ ] Bus interface changes compatible with both CCSP and rbus

### RDK Integration
- [ ] Changes tested on target hardware (or simulator)
- [ ] No increase in boot time
- [ ] Watchdog timeouts respected
- [ ] Log rotation considerations

### Performance
- [ ] Startup time impact measured
- [ ] Memory footprint delta calculated
- [ ] CPU usage profiled for new loops/threads
- [ ] No I/O in critical paths unless unavoidable

## Security

### Input Validation
- [ ] All external inputs validated
- [ ] Buffer sizes enforced
- [ ] No arbitrary command execution
- [ ] Path traversal prevented

### Data Protection 
- [ ] No hardcoded credentials
- [ ] Sensitive data redacted in logs
- [ ] Privacy controls respected
- [ ] No unnecessary privilege escalation

## Risk Assessment Questions

1. **What is the blast radius if this change has a bug?**
   - Single profile? All profiles? System stability?

2. **Can this change cause a device to become unresponsive?**
   - Deadlocks? Infinite loops? Resource exhaustion?

3. **How will this fail in production?**
   - Graceful degradation? Hard crash? Silent corruption?

4. **What's the rollback plan?**
   - Configuration revert? Firmware downgrade? Feature flag?

5. **What operational visibility exists?**
   - Logs? Metrics? Debug capabilities?
