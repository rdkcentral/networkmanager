---
applyTo: "source/test/**/*.cpp,source/test/**/*.h"
---

# C++ Testing Standards (Google Test)

## Test Framework

Use Google Test (gtest) and Google Mock (gmock) for all C++ test code.

## Test Organization

### File Structure
- One test file per source file: `foo.c` → `test/FooTest.cpp`
- Test fixtures for complex setups
- Mocks in separate files when reusable

```cpp
// GOOD: Test file structure
// filepath: source/test/utils/UtilsTest.cpp

extern "C" {
#include <utils/vector.h>
#include <utils/t2collection.h>
}

#include <gtest/gtest.h>
#include <gmock/gmock.h>

class UtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test resources
    }
    
    void TearDown() override {
        // Clean up test resources
    }
};

TEST_F(UtilsTest, VectorCreateAndDestroy) {
    Vector* vec = Vector_Create();
    ASSERT_NE(nullptr, vec);
    
    Vector_Destroy(vec, nullptr);
    // No memory leaks!
}
```

## Testing Patterns

### Test C Code from C++
- Wrap C headers in `extern "C"` blocks
- Use RAII in tests for automatic cleanup
- Mock C functions using gmock when needed

```cpp
extern "C" {
#include "telemetry2_0.h"
}

#include <gtest/gtest.h>

class TelemetryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize telemetry
        t2_init("test_component");
    }
    
    void TearDown() override {
        // Clean up
        t2_uninit();
    }
};

TEST_F(TelemetryTest, EventSubmission) {
    T2ERROR result = t2_event_s("TEST_EVENT", "test_value");
    EXPECT_EQ(T2ERROR_SUCCESS, result);
}
```

### Memory Leak Testing
- All tests must pass valgrind
- Use RAII wrappers for C resources
- Verify cleanup in TearDown

```cpp
// GOOD: RAII wrapper for C resource
class FileHandle {
    FILE* file_;
public:
    explicit FileHandle(const char* path, const char* mode)
        : file_(fopen(path, mode)) {}
    
    ~FileHandle() {
        if (file_) fclose(file_);
    }
    
    FILE* get() const { return file_; }
    bool valid() const { return file_ != nullptr; }
};

TEST(FileTest, ReadConfig) {
    FileHandle file("/tmp/config.json", "r");
    ASSERT_TRUE(file.valid());
    // file automatically closed when test exits
}
```

### Mocking External Dependencies

```cpp
// GOOD: Mock for system calls
class MockScheduler {
public:
    MOCK_METHOD(T2ERROR, registerProfile, 
                (const char*, unsigned int, unsigned int, 
                 bool, bool, bool, unsigned int, char*));
    MOCK_METHOD(T2ERROR, unregisterProfile, (const char*));
};

TEST(SchedulerTest, ProfileRegistration) {
    MockScheduler mock;
    
    EXPECT_CALL(mock, registerProfile(
        "profile1", 300, 0, false, true, false, 0, nullptr))
        .WillOnce(testing::Return(T2ERROR_SUCCESS));
    
    T2ERROR result = mock.registerProfile(
        "profile1", 300, 0, false, true, false, 0, nullptr);
    
    EXPECT_EQ(T2ERROR_SUCCESS, result);
}
```

## Test Quality Standards

### Coverage Requirements
- All public functions must have tests
- Test both success and failure paths
- Test boundary conditions
- Test error handling

### Test Naming
```cpp
// Pattern: TEST(ComponentName, BehaviorBeingTested)

TEST(Vector, CreateReturnsNonNull) { ... }
TEST(Vector, DestroyHandlesNull) { ... }
TEST(Vector, PushIncrementsSize) { ... }
TEST(Utils, ParseConfigInvalidJson) { ... }
```

### Assertions
- Use `ASSERT_*` when test can't continue after failure
- Use `EXPECT_*` when subsequent checks are still valuable
- Provide helpful failure messages

```cpp
// GOOD: Informative assertions
ASSERT_NE(nullptr, ptr) << "Failed to allocate " << size << " bytes";
EXPECT_EQ(expected, actual) << "Mismatch at index " << i;
EXPECT_TRUE(condition) << "Context: " << debug_info;
```

## Running Tests

### Build Tests
```bash
./configure --enable-gtest
make check
```

### Memory Checking
```bash
valgrind --leak-check=full --show-leak-kinds=all \
         ./source/test/telemetry_gtest_report
```

### Test Output
- Use `GTEST_OUTPUT=xml:results.xml` for CI integration
- Check return code: 0 = all passed
