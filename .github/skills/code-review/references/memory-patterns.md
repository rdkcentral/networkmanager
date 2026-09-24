# Memory Safety Patterns for Code Review

## Pattern 1: Allocation with Error Check

### ✅ CORRECT
```c
char* buffer = (char*)malloc(size);
if (!buffer) {
    T2Error("Failed to allocate %zu bytes\n", size);
    return ERR_MEMORY_ALLOCATION_FAILED;
}
// Use buffer
free(buffer);
```

### ❌ INCORRECT
```c
char* buffer = (char*)malloc(size);
strcpy(buffer, data);  // Crash if malloc failed
free(buffer);
```

## Pattern 2: Error Path Cleanup (Single Exit)

### ✅ CORRECT
```c
int process_data(const char* input) {
    int ret = SUCCESS;
    char* buffer = NULL;
    FILE* fp = NULL;
    
    buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        ret = ERR_NO_MEMORY;
        goto cleanup;
    }
    
    fp = fopen(input, "r");
    if (!fp) {
        ret = ERR_FILE_OPEN;
        goto cleanup;
    }
    
    // Process data...
    
cleanup:
    if (buffer) free(buffer);
    if (fp) fclose(fp);
    return ret;
}
```

### ❌ INCORRECT (Memory Leak)
```c
int process_data(const char* input) {
    char* buffer = malloc(BUFFER_SIZE);
    FILE* fp = fopen(input, "r");
    
    if (!fp) return ERR_FILE_OPEN;  // Leaked buffer
    
    // Process data...
    
    free(buffer);
    fclose(fp);
    return SUCCESS;
}
```

## Pattern 3: String Handling

### ✅ CORRECT
```c
char dest[MAX_SIZE];
strncpy(dest, source, MAX_SIZE - 1);
dest[MAX_SIZE - 1] = '\0';  // Ensure NULL termination
```

### ✅ BETTER (Helper Function)
```c
void safe_strcpy(char* dest, const char* src, size_t dest_size) {
    if (dest_size > 0) {
        strncpy(dest, src, dest_size - 1);
        dest[dest_size - 1] = '\0';
    }
}
```

### ❌ INCORRECT
```c
char dest[MAX_SIZE];
strcpy(dest, source);  // Buffer overflow if source > MAX_SIZE
```

## Pattern 4: Dynamic String Allocation

### ✅ CORRECT
```c
size_t len = strlen(source) + 1;  // +1 for NULL terminator
char* dest = (char*)malloc(len);
if (!dest) {
    return ERR_NO_MEMORY;
}
memcpy(dest, source, len);
// Later: free(dest);
```

### ❌ INCORRECT (Off-by-one)
```c
char* dest = (char*)malloc(strlen(source));  // Missing +1
strcpy(dest, source);  // Buffer overflow
```

## Pattern 5: Avoid Use-After-Free

### ✅ CORRECT
```c
free(ptr);
ptr = NULL;  // Invalidate pointer

// Later...
if (ptr != NULL) {
    // Safe: won't use freed memory
}
```

### ❌ INCORRECT
```c
free(ptr);
// ptr still points to freed memory

// Later...
strcpy(ptr, data);  // Use-after-free!
```

## Pattern 6: Double-Free Prevention

### ✅ CORRECT
```c
void cleanup(Context* ctx) {
    if (ctx) {
        if (ctx->buffer) {
            free(ctx->buffer);
            ctx->buffer = NULL;  // Prevent double-free
        }
        if (ctx->data) {
            free(ctx->data);
            ctx->data = NULL;
        }
    }
}
```

### ❌ INCORRECT
```c
void cleanup(Context* ctx) {
    free(ctx->buffer);  // What if already freed?
    free(ctx->data);    // Potential double-free
}
```

## Pattern 7: Array Bounds Checking

### ✅ CORRECT
```c
for (int i = 0; i < count && i < MAX_ITEMS; i++) {
    array[i] = data[i];
}
```

### ❌ INCORRECT
```c
for (int i = 0; i < count; i++) {
    array[i] = data[i];  // Buffer overflow if count > MAX_ITEMS
}
```

## Pattern 8: Safe realloc

### ✅ CORRECT
```c
char* temp = (char*)realloc(buffer, new_size);
if (!temp) {
    T2Error("realloc failed\n");
    free(buffer);  // Original buffer still valid
    return ERR_NO_MEMORY;
}
buffer = temp;
```

### ❌ INCORRECT (Memory Leak)
```c
buffer = (char*)realloc(buffer, new_size);
if (!buffer) {
    // Lost reference to original buffer!
    return ERR_NO_MEMORY;
}
```

## Pattern 9: Struct with Dynamic Members

### ✅ CORRECT
```c
typedef struct {
    char* name;
    char* value;
} Item;

void free_item(Item* item) {
    if (item) {
        free(item->name);
        free(item->value);
        free(item);
    }
}

Item* create_item(const char* name, const char* value) {
    Item* item = (Item*)calloc(1, sizeof(Item));
    if (!item) return NULL;
    
    item->name = strdup(name);
    if (!item->name) {
        free(item);
        return NULL;
    }
    
    item->value = strdup(value);
    if (!item->value) {
        free(item->name);
        free(item);
        return NULL;
    }
    
    return item;
}
```

## Pattern 10: Avoid Stack Overflow

### ✅ CORRECT (Small Array)
```c
char buffer[256];  // Small, OK on stack
```

### ✅ CORRECT (Large Array)
```c
char* buffer = (char*)malloc(LARGE_SIZE);  // Heap for large data
if (!buffer) return ERR_NO_MEMORY;
// Use buffer...
free(buffer);
```

### ❌ INCORRECT (Stack Overflow Risk)
```c
char buffer[1024 * 1024];  // 1MB on stack - too large!
```

## Pattern 11: Const Correctness (Avoid Unexpected Modifications)

### ✅ CORRECT
```c
void process(const char* input) {
    // Can't modify input
    char* copy = strdup(input);
    if (copy) {
        modify(copy);
        free(copy);
    }
}
```

## Pattern 12: NULL Safety in Cleanup

### ✅ CORRECT
```c
void cleanup(Data* data) {
    if (data == NULL) return;  // Guard against NULL
    
    if (data->buffer) free(data->buffer);
    if (data->file) fclose(data->file);
    if (data->mutex) {
        pthread_mutex_destroy(data->mutex);
        free(data->mutex);
    }
}
```

## Red Flags to Look For

1. **malloc/calloc without NULL check**
2. **strcpy/strcat/sprintf (use bounded versions)**
3. **Return before cleanup on error path**
4. **Pointer used after free (no NULL assignment)**
5. **Array index without bounds check**
6. **Large arrays on stack**
7. **realloc result assigned directly to original pointer**
8. **Missing free for every malloc**
9. **Buffer size calculations with off-by-one errors**
10. **Strdup without considering NULL return**

## Valgrind Signals to Watch

When reviewing changes, recommend valgrind if:
- New malloc/calloc/realloc/free calls
- Complex pointer manipulation
- String handling code
- Data structure modifications
- Error path additions

```bash
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         ./test_binary
```

Look for:
- **Definitely lost**: Memory leak (must fix)
- **Indirectly lost**: Leaked structure with allocated members
- **Possibly lost**: Pointer arithmetic issue
- **Still reachable**: Not freed but still tracked (often acceptable for globals)
- **Invalid read/write**: Buffer overflow or use-after-free
