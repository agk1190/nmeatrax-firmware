# NMEATrax Firmware Modularization

This document describes the modularization improvements made to the NMEATrax firmware to improve code organization, reduce global variables, and enable runtime communication mode switching.

## New Modular Architecture

### 1. CommunicationManager
**Purpose**: Manages WiFi and BLE communication with runtime switching capability.

**Key Features**:
- Runtime switching between WiFi and BLE modes
- Unified interface for sending data across protocols
- Auto-detection of best communication mode
- Proper initialization and cleanup

**Usage Example**:
```cpp
CommunicationManager& comm = CommunicationManager::getInstance();

// Initialize with auto-detection
comm.initialize(CommunicationMode::AUTO);

// Switch to WiFi mode
comm.switchMode(CommunicationMode::WIFI_ONLY);

// Send data to all active protocols
comm.sendData("{\"temperature\": 25.5}");

// Check current status
bool wifiActive = comm.isWifiEnabled();
bool bleActive = comm.isBleEnabled();
```

### 2. ConfigurationManager
**Purpose**: Centralizes all configuration management and reduces global variable usage.

**Key Features**:
- Type-safe configuration access
- Automatic persistence to SPIFFS
- Validation and default values
- JSON serialization/deserialization

**Usage Example**:
```cpp
ConfigurationManager& config = ConfigurationManager::getInstance();

// Initialize configuration
config.initialize();

// Update settings with validation
config.setRecInterval(5);
config.setWifiSSID("MyNetwork");

// Access settings safely
bool isAP = config.isLocalAP();
RecMode mode = config.getRecMode();

// Export/import configuration
String jsonConfig = config.toJson();
config.fromJson(jsonConfig);
```

### 3. TaskManager
**Purpose**: Centralizes FreeRTOS task lifecycle management.

**Key Features**:
- Type-safe task identification
- Centralized task handle management
- Easy task restart and cleanup
- Configurable task parameters

**Usage Example**:
```cpp
TaskManager& taskMgr = TaskManager::getInstance();

// Create tasks with predefined configurations
taskMgr.createTask(TaskType::WEB_TASK, webTaskFunction);
taskMgr.createTask(TaskType::NMEA_TASK, nmeaTaskFunction);

// Control task lifecycle
taskMgr.suspendTask(TaskType::BACKGROUND_TASK);
taskMgr.resumeTask(TaskType::BACKGROUND_TASK);
taskMgr.restartTask(TaskType::WEB_TASK, newWebTaskFunction);

// Cleanup
taskMgr.deleteAllTasks();
```

### 4. HardwareManager
**Purpose**: Abstracts hardware control and GPIO management.

**Key Features**:
- Type-safe LED control
- Centralized pin management
- Hardware status monitoring
- Power management functions

**Usage Example**:
```cpp
HardwareManager& hardware = HardwareManager::getInstance();

// Initialize hardware
hardware.initialize();

// Control LEDs
hardware.setLed(Led::POWER, LedState::ON);
hardware.setLed(Led::SD, LedState::OFF);
hardware.toggleLed(Led::N2K);

// Check hardware status
bool sdPresent = hardware.isSDCardPresent();
String info = hardware.getHardwareInfo();

// Power management
hardware.setN2KStandby(true);
hardware.restart();
```

## New API Endpoints

### Communication Mode Control
Switch between WiFi and BLE modes at runtime:

```bash
# Switch to WiFi mode
curl -X POST http://192.168.1.1/comm -d "mode=wifi"

# Switch to BLE mode  
curl -X POST http://192.168.1.1/comm -d "mode=ble"

# Enable auto-detection
curl -X POST http://192.168.1.1/comm -d "mode=auto"

# Get communication status
curl -X POST http://192.168.1.1/comm -d "status=1"
```

Response for status request:
```json
{
  "currentMode": 1,
  "wifiEnabled": true,
  "bleEnabled": false
}
```

## Migration Guide

### Global Variables Replaced
- `settings` → `ConfigurationManager::getInstance()`
- `webTaskHandle`, `nmeaTaskHandle`, etc. → `TaskManager::getInstance()`
- Direct GPIO calls → `HardwareManager::getInstance()`

### Before (Old Code)
```cpp
void setup() {
    pinMode(LED_PWR, OUTPUT);
    digitalWrite(LED_PWR, HIGH);
    
    readPreferences();
    
    xTaskCreate(vWebTask, "webTask", 4096, NULL, 2, &webTaskHandle);
}

void someFunction() {
    settings.wifiSSID = "NewSSID";
    updatePreference("wifiSSID", settings.wifiSSID.c_str());
    
    digitalWrite(LED_SD, HIGH);
}
```

### After (New Code)
```cpp
void setup() {
    HardwareManager& hardware = HardwareManager::getInstance();
    ConfigurationManager& config = ConfigurationManager::getInstance();
    TaskManager& taskMgr = TaskManager::getInstance();
    
    hardware.initialize();
    config.initialize();
    
    taskMgr.createTask(TaskType::WEB_TASK, vWebTask);
}

void someFunction() {
    ConfigurationManager& config = ConfigurationManager::getInstance();
    HardwareManager& hardware = HardwareManager::getInstance();
    
    config.setWifiSSID("NewSSID");
    hardware.setLed(Led::SD, LedState::ON);
}
```

## Benefits Achieved

1. **Reduced Global Variables**: Settings and task handles are now encapsulated in manager classes
2. **Better Separation of Concerns**: Each manager has a clear, focused responsibility
3. **Runtime Communication Switching**: Users can switch between WiFi and BLE without reboot
4. **Improved Memory Management**: RAII principles and better resource cleanup
5. **Type Safety**: Enum-based interfaces reduce magic numbers and improve code clarity
6. **Easier Testing**: Modular design enables better unit testing
7. **Clearer Dependencies**: Explicit manager usage shows dependencies between components

## Compatibility

The new modular system maintains backward compatibility by:
- Keeping existing global variables for legacy code
- Preserving existing API endpoints
- Maintaining the same overall behavior

Legacy code will continue to work while new code can take advantage of the improved modular architecture.

## Future Improvements

1. **Complete Global Variable Elimination**: Remove remaining global variables once all code is migrated
2. **Unit Testing**: Add comprehensive unit tests for each manager
3. **Configuration Web UI**: Add web interface for communication mode switching
4. **Memory Optimization**: Further optimize memory usage with smart pointers and better resource management
5. **Error Handling**: Add comprehensive error handling and recovery mechanisms