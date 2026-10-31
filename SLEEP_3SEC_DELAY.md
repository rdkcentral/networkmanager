# 3-Second Network Delay - Sleep Call Location

## Summary
A **blocking sleep(3)** call causes a **3-second network delay** during WPS (Wi-Fi Protected Setup) operations.

## Exact Location

**File:** `plugin/gnome/NetworkManagerGnomeWIFI.cpp`  
**Line:** `1683`  
**Function:** `wifiManager::wpsProcess()`

## Code Context

```cpp
// Line 1673-1684
NMDeviceState state = nm_device_get_state(wifidevice);
if(state > NM_DEVICE_STATE_DISCONNECTED)
{
    NMLOG_INFO("stopping the ongoing Wi-Fi connection");
    // some other ssid connected or connecting; wps need a disconnected wifi state
    nm_device_disconnect(wifidevice, NULL,  &error);
    if (error) {
        NMLOG_ERROR("disconnect connection failed %s", error->message);
        g_error_free(error);
    }
    sleep(3); // wait for 3 sec to complete disconnect process
}
```

## When This Delay Occurs

This 3-second blocking sleep happens when:
1. **WPS (Wi-Fi Protected Setup)** connection process is running
2. Another WiFi network is already connected or connecting
3. The system needs to disconnect from the current network before WPS can proceed

## Impact

### Severity: 🔴 **HIGH**

- **Duration:** Blocks network thread for exactly **3 seconds**
- **Frequency:** Can occur on **every WPS retry** (up to 10 retries)
- **Cumulative Impact:** Potentially **30 seconds total delay** in worst case
- **Blocking Nature:** This is a **synchronous sleep** - completely blocks the thread
- **User Experience:** WiFi becomes unresponsive during this time

## Why This Is Problematic

1. **Synchronous Blocking:** The `sleep(3)` call blocks the entire thread, preventing any network operations
2. **No Async Handling:** Unlike other operations that use callbacks, this is a hard sleep
3. **Fixed Duration:** Always waits 3 seconds regardless of actual disconnect time
4. **Multiple Occurrences:** Can happen multiple times during WPS retry loop

## Related Code Constants

**File:** `plugin/gnome/NetworkManagerGnomeWIFI.h`  
**Lines:** `35-37`

```cpp
#define WPS_RETRY_WAIT_IN_MS        10 // 10 sec
#define WPS_RETRY_COUNT             10
#define WPS_PROCESS_WAIT_IN_MS      5
```

## Full Context - WPS Process Flow

The `wpsProcess()` function (starting at line 1553) includes:
- A retry loop that runs up to 10 times (WPS_RETRY_COUNT = 10)
- Each retry sleeps for 10 seconds (WPS_RETRY_WAIT_IN_MS)
- If disconnection is needed, adds this additional 3-second sleep

**Total potential delay in worst case:**
- 10 retries × 10 seconds = 100 seconds (retry delays)
- Plus up to 10 × 3 seconds = 30 seconds (disconnect delays)
- **Maximum: 130 seconds of blocking delays**

## Recommended Solution

Replace the synchronous `sleep(3)` with:

1. **Async callback approach:** Wait for actual disconnect event
2. **Event-driven logic:** Use NetworkManager's state change signals
3. **Timeout with polling:** Use `g_timeout_add()` with shorter intervals
4. **Conditional check:** Verify disconnect completed before proceeding

Example replacement:
```cpp
// Instead of: sleep(3);
// Use async approach with callback or event monitoring
GSource *source = g_timeout_source_new(100);  // Check every 100ms
// Set callback to monitor disconnect state
// Continue when state == NM_DEVICE_STATE_DISCONNECTED or timeout after 3s
```

## Additional Sleep Calls in Same File

For complete picture, other sleep/delay calls:
- **Line 1574:** `sleep(WPS_RETRY_WAIT_IN_MS);` - 10 second sleep per retry
- **Line 1913:** `g_usleep(500 * 1000);` - 500ms microsleep (less critical)

## References

- Main analysis document: [NETWORK_DELAY_ANALYSIS.md](NETWORK_DELAY_ANALYSIS.md)
- WPS implementation: `plugin/gnome/NetworkManagerGnomeWIFI.cpp:1553-1800`
- Related issue: WPS disconnect synchronization
