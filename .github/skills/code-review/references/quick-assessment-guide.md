# Quick Assessment Guide

Fast reference for generating code reviews. Use this to streamline the analysis process.

## Risk Scoring Matrix

### Scope Score (0-10)
| Files Changed | Modules | LOC | Score |
|---------------|---------|-----|-------|
| 1-5 | 1 | <100 | 1-2 |
| 6-15 | 1-2 | 100-500 | 3-5 |
| 16-30 | 2-4 | 500-1000 | 6-8 |
| >30 | >4 | >1000 | 9-10 |

### Criticality Score (0-10)
| Component Type | Examples | Score |
|----------------|----------|-------|
| Peripheral | Test code, docs, comments | 1-2 |
| Configuration | Config files, schemas | 3-4 |
| Utilities | Helper functions, parsing | 5-6 |
| Core Logic | Scheduler, profile mgmt, report gen | 7-8 |
| Critical Path | Bus interface, data collection | 9-10 |

### Complexity Score (0-10)
| Change Type | Indicators | Score |
|-------------|------------|-------|
| Trivial | Typo, comment, doc | 1 |
| Simple | Single function, <20 LOC | 2-3 |
| Moderate | Multi-function, refactor | 4-6 |
| Complex | Algorithm change, state machine | 7-8 |
| Very Complex | Concurrency change, protocol change | 9-10 |

**Complexity Indicators:**
- Cyclomatic complexity >15: +2
- Nesting depth >4: +1  
- New thread/lock: +2
- Recursive function: +1

### Safety Score (0-10)
| Issues Found | Score |
|--------------|-------|
| None (clean review) | 0 |
| Minor style issues | 1-2 |
| Missing NULL checks | 3-4 |
| Potential race condition | 5-6 |
| Memory leak confirmed | 7-8 |
| Deadlock scenario | 9-10 |

**Auto-score based on findings:**
- Each memory leak: +2
- Each race condition: +2
- Each potential deadlock: +3
- Each buffer overflow: +3
- Use-after-free: +4

**Coverity Defects (from PR comments):**
- Check for comments from user "rdkcmf-jenkins" or titles starting with "Coverity Issue"
- Each HIGH severity Coverity defect: +3
- Each MEDIUM severity Coverity defect: +2
- Each LOW severity Coverity defect: +1
- Cap at 10 for safety score

### Testing Score (0-10)
| Test Status | Score |
|-------------|-------|
| Comprehensive (>90% coverage, integration tests) | 0-1 |
| Good (>75% coverage, unit tests complete) | 2-3 |
| Basic (>50% coverage, some tests) | 4-6 |
| Minimal (<50% coverage, few tests) | 7-8 |
| None (no tests added) | 9-10 |

---

## Risk Level Calculation

**Total Score** = Scope + Criticality + Complexity + Safety + Testing

| Total | Risk Level | Symbol | Action |
|-------|-----------|---------|---------|
| 0-10 | LOW | 🟢 | Standard review |
| 11-20 | MEDIUM | 🟡 | Careful review, validate with tools |
| 21-35 | HIGH | 🔴 | Deep review, testing required |
| 36-50 | CRITICAL | ⚫ | Block merge, full analysis needed |

---

## Module Classification

Map file paths to modules for change tree:

| Path Pattern | Module | Criticality |
|--------------|--------|-------------|
| `source/bulkdata/*.c` | Bulk Data Collection | High |
| `source/scheduler/*.c` | Scheduling Engine | Critical |
| `source/reportgen/*.c` | Report Generation | High |
| `source/ccspinterface/*.c` | Bus Interface | Critical |
| `source/protocol/http/*.c` | HTTP Transport | High |
| `source/protocol/rbusMethod/*.c` | rbus Transport | High |
| `source/t2parser/*.c` | Profile Parser | Medium |
| `source/dcautil/*.c` | DCA Utilities | Medium |
| `source/utils/*.c` | Common Utilities | Medium |
| `source/privacycontrol/*.c` | Privacy Control | High |
| `source/t2dm/*.c` | Data Model Plugin | Medium |
| `source/t2ssp/*.c` | SSP Main | High |
| `source/test/*.cpp` | Unit Tests | Low |
| `source/testApp/*.c` | Test Application | Low |
| `*.am`, `*.ac` | Build System | Medium |
| `config/*` | Configuration | Medium |
| `schemas/*` | JSON Schemas | Low |
| `docs/*` | Documentation | Low |

---

## Change Indicators (for Mermaid diagrams)

**Symbols:**
- ✅ Test coverage added (positive)
- 🟢 Low risk change
- 🟡 Medium risk (review carefully)
- ⚠️ Safety concern flagged
- 🔴 High risk (requires attention)
- ⚫ Critical issue (blocking)

**Mermaid CSS Classes:**
```css
classDef critical fill:#ff6b6b,stroke:#c92a2a,color:#fff      /* Red - High risk */
classDef warning fill:#ffd43b,stroke:#f08c00,color:#000       /* Yellow - Warning */
classDef medium fill:#ffa94d,stroke:#fd7e14,color:#000        /* Orange - Medium */
classDef safe fill:#51cf66,stroke:#2f9e44,color:#fff          /* Green - Safe/Tests */
classDef neutral fill:#e0e0e0,stroke:#9e9e9e,color:#000       /* Gray - Neutral */
```

---

## Quick Memory Safety Check

When analyzing C files, scan for these patterns:

### 🔴 RED FLAGS (must fix)
- `strcpy`, `strcat`, `sprintf`, `gets`
- `malloc`/`calloc` without NULL check
- Return before `free` on error path
- Pointer used after `free`
- Array access without bounds check
- `realloc` result assigned directly to original pointer

### 🟡 YELLOW FLAGS (verify carefully)
- `strncpy`, `snprintf` (check NULL termination)
- `memcpy` (check for overlap, size validation)
- Large stack arrays (>4KB)
- Complex pointer arithmetic
- cJSON functions (check for free)
- Vector/list operations (cleanup elements?)

### ✅ GREEN PATTERNS (good)
- `goto cleanup` single-exit pattern
- Pointer set to NULL after free
- Defensive NULL checks before use
- Bounded string operations with explicit NULL
- RAII wrappers (C++ tests)

---

## Quick Thread Safety Check

### 🔴 RED FLAGS (must fix)
- Shared global modified without mutex
- Multiple locks acquired in different orders
- Condition variable without predicate loop
- Long operation under lock (>10ms)
- Thread created without join or detach
- Recursive mutex usage

### 🟡 YELLOW FLAGS (verify carefully)
- New mutex or lock introduced
- Lock held across function call
- Callback invoked under lock
- Complex lock/unlock patterns
- Lock with goto (verify error paths)

### ✅ GREEN PATTERNS (good)
- Consistent lock ordering documented
- Short critical sections (<1ms)
- Condition variable with while loop
- Thread stack size explicitly set
- Lock released before blocking operation
- Timeout on lock acquisition

---

## API Compatibility Quick Check

### Breaking Changes (high risk)
- ✗ Function signature modified
- ✗ Struct member reordered
- ✗ Enum value changed
- ✗ Public function deleted
- ✗ Return value semantics changed

### Safe Changes (low risk)
- ✓ New function added
- ✓ New struct member at end
- ✓ New enum value at end
- ✓ Internal function changed
- ✓ Implementation detail changed

---

## Priority Flagging

When generating recommendations, prioritize:

### P0 (MUST FIX - blocking)
- Deadlock scenarios
- Memory corruption (buffer overflow, use-after-free)
- NULL pointer dereferences
- Race conditions in critical path
- API/ABI breakage

### P1 (SHOULD FIX - before merge)
- Memory leaks
- Missing error handling
- Test coverage gaps (<80% on new code)
- Documentation missing
- Performance regressions

### P2 (CONSIDER - post-merge)
- Code style inconsistencies
- Minor optimization opportunities
- Refactoring suggestions
- Additional test scenarios
- Enhanced error messages

---

## Review Flow Checklist

Follow this order for efficient review generation:

1. **Fetch PR metadata** (title, description, files changed)
2. **Calculate risk scores** (use matrices above)
3. **Classify files by module** (use module table)
4. **Generate change tree** (ASCII visualization)
5. **Analyze each file**:
   - Run memory safety patterns
   - Run thread safety patterns
   - Check against common pitfalls
   - Calculate local risk
6. **Identify cross-cutting concerns** (build, config, tests)
7. **Aggregate findings** (group by severity)
8. **Generate recommendations** (prioritize by P0/P1/P2)
9. **Create checklist** (actionable items)
10. **Write executive summary** (2-3 sentences)

---

## Common Review Shortcuts

### For small PRs (<5 files, <100 LOC)
- Skip detailed module breakdown
- Focus on changed functions only
- Brief risk assessment
- Quick checklist

### For doc-only PRs
- Verify accuracy
- Check formatting
- Ensure completeness
- Skip code analysis sections

### For test-only PRs
- Verify test quality (determinism, coverage)
- Check for test resource leaks
- Ensure setup/teardown balanced
- Brief risk summary only

### For config-only PRs
- Validate against schema
- Check backward compatibility
- Verify default values
- Document impact

---

## Time-Saving Tips

1. **Use grep patterns** for quick scans:
   ```bash
   grep -rn "malloc\|calloc\|free" source/
   grep -rn "pthread_mutex\|pthread_cond" source/
   grep -rn "strcpy\|sprintf\|gets" source/
   ```

2. **Focus on diff hunks**, not entire files

3. **Reuse analysis** from previous similar PRs

4. **Skip generated code** (e.g., protocol buffers)

5. **Batch similar issues** rather than listing individually

6. **Reference line numbers** for specific issues only

7. **Use tables** for repetitive data (better than prose)

---

## Output Structure Template

```markdown
# Title + Overview (5% of content)
## Executive Summary (5%)
## Risk Assessment (10%)
## Change Tree (5%)
## Module Analysis (50%)
  - Per module: files, changes, impacts
## Cross-Cutting (10%)
## Regression Analysis (10%)
## Recommendations (5%)
## Checklist (5%)
```

Aim for 1500-3000 words total, adjust based on PR size.
