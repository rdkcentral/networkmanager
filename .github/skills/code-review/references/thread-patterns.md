# Thread Safety Patterns for Code Review

## Pattern 1: Mutex-Protected Shared Data

### ✅ CORRECT
```c
typedef struct {
    pthread_mutex_t mutex;
    int counter;
    char* shared_data;
} SharedContext;

void increment_counter(SharedContext* ctx) {
    pthread_mutex_lock(&ctx->mutex);
    ctx->counter++;
    pthread_mutex_unlock(&ctx->mutex);
}
```

### ❌ INCORRECT (Race Condition)
```c
typedef struct {
    int counter;  // No mutex protection
} SharedContext;

void increment_counter(SharedContext* ctx) {
    ctx->counter++;  // Race condition!
}
```

## Pattern 2: Consistent Lock Ordering (Avoid Deadlock)

### ✅ CORRECT
```c
// Always acquire in same order: mutex_a then mutex_b
void operation1() {
    pthread_mutex_lock(&mutex_a);
    pthread_mutex_lock(&mutex_b);
    // Critical section
    pthread_mutex_unlock(&mutex_b);
    pthread_mutex_unlock(&mutex_a);
}

void operation2() {
    pthread_mutex_lock(&mutex_a);  // Same order
    pthread_mutex_lock(&mutex_b);
    // Critical section
    pthread_mutex_unlock(&mutex_b);
    pthread_mutex_unlock(&mutex_a);
}
```

### ❌ INCORRECT (Deadlock Risk)
```c
void operation1() {
    pthread_mutex_lock(&mutex_a);
    pthread_mutex_lock(&mutex_b);
    // ...
}

void operation2() {
    pthread_mutex_lock(&mutex_b);  // Opposite order - DEADLOCK!
    pthread_mutex_lock(&mutex_a);
    // ...
}
```

## Pattern 3: Condition Variable Usage

### ✅ CORRECT
```c
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
bool ready = false;

// Producer
void signal_ready() {
    pthread_mutex_lock(&mutex);
    ready = true;  // Set predicate
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}

// Consumer
void wait_ready() {
    pthread_mutex_lock(&mutex);
    while (!ready) {  // WHILE loop, not IF
        pthread_cond_wait(&cond, &mutex);
    }
    // Ready is true here
    pthread_mutex_unlock(&mutex);
}
```

### ❌ INCORRECT (Missing Predicate)
```c
void wait_ready() {
    pthread_mutex_lock(&mutex);
    pthread_cond_wait(&cond, &mutex);  // No predicate check!
    // Spurious wakeup possible
    pthread_mutex_unlock(&mutex);
}
```

## Pattern 4: Thread Creation with Stack Size

### ✅ CORRECT (Embedded System)
```c
pthread_t thread;
pthread_attr_t attr;

pthread_attr_init(&attr);
pthread_attr_setstacksize(&attr, 256 * 1024);  // 256KB explicit

int ret = pthread_create(&thread, &attr, thread_func, arg);
if (ret != 0) {
    T2Error("pthread_create failed: %d\n", ret);
}
pthread_attr_destroy(&attr);
```

### ❌ INCORRECT (Default Stack)
```c
pthread_t thread;
pthread_create(&thread, NULL, thread_func, arg);  // Unknown stack size
```

## Pattern 5: Initialization Safety

### ✅ CORRECT
```c
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t once_control = PTHREAD_ONCE_INIT;

void init_once() {
    // Initialization code (runs exactly once)
}

void safe_init() {
    pthread_once(&once_control, init_once);
}
```

### ✅ CORRECT (Dynamic Init)
```c
pthread_mutex_t* mutex;

int init_mutex() {
    mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if (!mutex) return -1;
    
    int ret = pthread_mutex_init(mutex, NULL);
    if (ret != 0) {
        free(mutex);
        return -1;
    }
    return 0;
}
```

### ❌ INCORRECT
```c
pthread_mutex_t mutex;  // Not initialized!
pthread_mutex_lock(&mutex);  // Undefined behavior
```

## Pattern 6: Read-Write Lock for Read-Heavy Workloads

### ✅ CORRECT (Multiple Readers)
```c
pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

void read_data() {
    pthread_rwlock_rdlock(&rwlock);
    // Read shared data (multiple readers OK)
    pthread_rwlock_unlock(&rwlock);
}

void write_data() {
    pthread_rwlock_wrlock(&rwlock);
    // Modify shared data (exclusive)
    pthread_rwlock_unlock(&rwlock);
}
```

## Pattern 7: Atomic Operations (Lockless)

### ✅ CORRECT (C11 Atomics)
```c
#include <stdatomic.h>

atomic_int counter = ATOMIC_VAR_INIT(0);

void increment() {
    atomic_fetch_add(&counter, 1);  // Thread-safe
}

int get_value() {
    return atomic_load(&counter);
}
```

### ✅ CORRECT (GCC Built-ins for embedded)
```c
static int counter = 0;

void increment() {
    __sync_fetch_and_add(&counter, 1);  // Thread-safe because the built-in is atomic
}
```

## Pattern 8: Thread Cleanup

### ✅ CORRECT
```c
typedef struct {
    pthread_t thread;
    bool running;
    pthread_mutex_t mutex;
} ThreadContext;

void stop_thread(ThreadContext* ctx) {
    pthread_mutex_lock(&ctx->mutex);
    ctx->running = false;
    pthread_mutex_unlock(&ctx->mutex);
    
    pthread_join(ctx->thread, NULL);  // Wait for termination
    pthread_mutex_destroy(&ctx->mutex);
}
```

### ❌ INCORRECT (Resource Leak)
```c
void stop_thread(ThreadContext* ctx) {
    ctx->running = false;  // No synchronization
    // Missing pthread_join - zombie thread!
}
```

## Pattern 9: Minimize Lock Hold Time

### ✅ CORRECT
```c
void process_item(Queue* queue) {
    Item* item = NULL;
    
    pthread_mutex_lock(&queue->mutex);
    item = dequeue(queue);
    pthread_mutex_unlock(&queue->mutex);  // Release early
    
    if (item) {
        expensive_operation(item);  // Done outside lock
        free(item);
    }
}
```

### ❌ INCORRECT
```c
void process_item(Queue* queue) {
    pthread_mutex_lock(&queue->mutex);
    Item* item = dequeue(queue);
    
    if (item) {
        expensive_operation(item);  // Holding lock too long!
    }
    
    pthread_mutex_unlock(&queue->mutex);
}
```

## Pattern 10: Thread-Local Storage

### ✅ CORRECT (C11 standard — use when the toolchain/runtime supports C11 TLS)
```c
#include <threads.h>  /* C11 threads API; availability varies by embedded toolchain */

_Thread_local char error_buffer[256];  /* Standard C11 TLS, but not universally available on all embedded toolchains */

void set_error(const char* msg) {
    strncpy(error_buffer, msg, sizeof(error_buffer) - 1);
    error_buffer[sizeof(error_buffer) - 1] = '\0';
}

const char* get_error() {
    return error_buffer;  /* Thread-safe */
}
```

### ✅ CORRECT (POSIX — maximum portability for pre-C11 and embedded toolchains)
```c
#include <pthread.h>

static pthread_key_t  error_key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

static void make_key(void) {
    pthread_key_create(&error_key, free);
}

void set_error(const char* msg) {
    pthread_once(&key_once, make_key);
    char* buf = pthread_getspecific(error_key);
    if (!buf) {
        buf = malloc(256);
        if (!buf) return;
        pthread_setspecific(error_key, buf);
    }
    strncpy(buf, msg, 255);
    buf[255] = '\0';
}

const char* get_error() {
    pthread_once(&key_once, make_key);
    return pthread_getspecific(error_key);  /* Thread-safe */
}
```

### ⚠️ COMPILER EXTENSION ONLY (`__thread`)
```c
/* __thread is a GCC/Clang extension — not standard C.
 * Avoid in new code; prefer _Thread_local (C11) or pthread_key_t (POSIX). */
__thread char error_buffer[256];
```

## Pattern 11: Lock Timeout (Avoid Infinite Wait)

### ✅ CORRECT (use `CLOCK_MONOTONIC` to avoid NTP/time-change disruption)
```c
struct timespec timeout;
/* CLOCK_MONOTONIC is preferred over CLOCK_REALTIME: it is not affected by
 * NTP adjustments or manual clock changes, so the timeout is always
 * relative to wall-clock elapsed time. */
clock_gettime(CLOCK_MONOTONIC, &timeout);
timeout.tv_sec += 5;  // 5 second timeout

int ret = pthread_mutex_timedlock(&mutex, &timeout);
if (ret == ETIMEDOUT) {
    T2Error("Mutex lock timeout\n");
    return ERR_TIMEOUT;
}
```

> **Note:** `pthread_mutex_timedlock` requires an *absolute* time based on
> `CLOCK_REALTIME` per POSIX. To combine the monotonic elapsed measurement
> with the required real-time absolute value, compute the delta from
> `CLOCK_MONOTONIC` and add it to the current `CLOCK_REALTIME` value, or use
> a platform-specific extension such as
> `pthread_condattr_setclock(CLOCK_MONOTONIC)` with a condition variable.
> On Linux (glibc ≥ 2.30) `pthread_mutex_clocklock` accepts `CLOCK_MONOTONIC`
> directly and is the cleanest solution when available.

## Pattern 12: Volatile for Hardware Registers

### ✅ CORRECT
```c
volatile uint32_t* const hardware_reg = (uint32_t*)0x40000000;

void write_register(uint32_t value) {
    *hardware_reg = value;  // Compiler won't optimize away
}

uint32_t read_register() {
    return *hardware_reg;  // Always reads from hardware
}
```

### ❌ INCORRECT (Shared Variable Between Threads)
```c
volatile int counter = 0;  // volatile != thread-safe!

// Still need mutex or atomics for thread safety
```

## Red Flags to Look For

1. **Shared data without mutex protection**
2. **Multiple locks acquired in different orders**
3. **Condition variable without predicate loop**
4. **Missing pthread_join (zombie threads)**
5. **Mutex not initialized before use**
6. **Lock held across blocking I/O**
7. **Recursive locks (potential for deadlock)**
8. **Global state modified without synchronization**
9. **Memory barriers missing in lockless code**
10. **ThreadSanitizer warnings ignored**

## Common Deadlock Scenarios

### Scenario 1: Lock Ordering Violation
```c
// Thread A: Lock(A) → Lock(B)
// Thread B: Lock(B) → Lock(A)
// Result: DEADLOCK
```

### Scenario 2: Lock and Wait
```c
pthread_mutex_lock(&mutex);
pthread_join(thread, NULL);  // Waiting for thread that needs mutex
pthread_mutex_unlock(&mutex);  // DEADLOCK
```

### Scenario 3: Callback Under Lock
```c
pthread_mutex_lock(&mutex);
user_callback();  // Callback tries to lock same mutex
pthread_mutex_unlock(&mutex);  // DEADLOCK
```

## Testing for Thread Safety

### Helgrind (Valgrind)
```bash
valgrind --tool=helgrind ./program
```
Detects:
- Data races
- Lock order violations
- Improper use of POSIX threading primitives

### ThreadSanitizer (TSan)
```bash
gcc -fsanitize=thread -g program.c -o program
./program
```
Detects:
- Data races
- Deadlocks
- Use of uninitialized mutexes

## Telemetry 2.0 Specific Patterns

### TimeoutThread Pattern
```c
// TimeoutThread uses condition variable with timeout
pthread_mutex_lock(&profile->mutex);
while (profile->running) {
    struct timespec timeout;
    // Calculate next timeout
    int ret = pthread_cond_timedwait(&profile->cond, 
                                      &profile->mutex, 
                                      &timeout);
    if (ret == ETIMEDOUT) {
        // Trigger report generation
    }
}
pthread_mutex_unlock(&profile->mutex);
```

### rbus Async Handler Pattern
```c
// Short-lived thread for async rbus operations
void* asyncMethodHandler(void* arg) {
    Message* msg = (Message*)arg;
    
    // Process message (no shared state modifications)
    rbus_sendData(msg->method, msg->data);
    
    // Cleanup
    free(msg->data);
    free(msg);
    
    return NULL;
}
```

### Profile State Protection
```c
// All profile state changes must be protected
pthread_mutex_lock(&profile->mutex);
profile->state = STATE_COLLECTING;
profile->last_report_time = time(NULL);
pthread_mutex_unlock(&profile->mutex);
```

## Review Checklist for Threading Changes

- [ ] Shared data identified and protected
- [ ] Lock ordering documented and consistent
- [ ] Condition variables used with predicate loop
- [ ] Thread stack size specified
- [ ] Thread cleanup (join/detach) implemented
- [ ] Mutexes initialized and destroyed
- [ ] No locks held across blocking operations
- [ ] Timeouts prevent infinite waits
- [ ] Error paths release locks
- [ ] Tested with helgrind/TSan
