/**
 * NMEATrax Task Manager Implementation
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * Centralizes FreeRTOS task lifecycle management and reduces global task handles.
 */

#include "TaskManager.h"

TaskManager& TaskManager::getInstance() {
    static TaskManager instance;
    if (!instance.initialized) {
        instance.initializeDefaults();
        instance.initialized = true;
    }
    return instance;
}

bool TaskManager::createTask(TaskType type, TaskFunction_t taskFunction, void* parameters) {
    int index = getTaskIndex(type);
    if (index < 0) {
        return false;
    }
    
    TaskInfo& task = tasks[index];
    
    // Delete existing task if running
    if (task.isRunning && task.handle != nullptr) {
        vTaskDelete(task.handle);
        task.handle = nullptr;
        task.isRunning = false;
    }
    
    BaseType_t result;
    if (task.coreId >= 0) {
        result = xTaskCreatePinnedToCore(
            taskFunction,
            task.name,
            task.stackSize,
            parameters,
            task.priority,
            &task.handle,
            task.coreId
        );
    } else {
        result = xTaskCreate(
            taskFunction,
            task.name,
            task.stackSize,
            parameters,
            task.priority,
            &task.handle
        );
    }
    
    if (result == pdPASS) {
        task.isRunning = true;
        Serial.print("Created task: ");
        Serial.println(task.name);
        return true;
    } else {
        Serial.print("Failed to create task: ");
        Serial.println(task.name);
        return false;
    }
}

bool TaskManager::deleteTask(TaskType type) {
    int index = getTaskIndex(type);
    if (index < 0) {
        return false;
    }
    
    TaskInfo& task = tasks[index];
    if (task.handle != nullptr && task.isRunning) {
        vTaskDelete(task.handle);
        task.handle = nullptr;
        task.isRunning = false;
        Serial.print("Deleted task: ");
        Serial.println(task.name);
        return true;
    }
    return false;
}

bool TaskManager::suspendTask(TaskType type) {
    int index = getTaskIndex(type);
    if (index < 0) {
        return false;
    }
    
    TaskInfo& task = tasks[index];
    if (task.handle != nullptr && task.isRunning) {
        vTaskSuspend(task.handle);
        return true;
    }
    return false;
}

bool TaskManager::resumeTask(TaskType type) {
    int index = getTaskIndex(type);
    if (index < 0) {
        return false;
    }
    
    TaskInfo& task = tasks[index];
    if (task.handle != nullptr && task.isRunning) {
        vTaskResume(task.handle);
        return true;
    }
    return false;
}

bool TaskManager::restartTask(TaskType type, TaskFunction_t taskFunction, void* parameters) {
    deleteTask(type);
    return createTask(type, taskFunction, parameters);
}

bool TaskManager::isTaskRunning(TaskType type) const {
    int index = getTaskIndex(type);
    if (index < 0) {
        return false;
    }
    return tasks[index].isRunning;
}

TaskHandle_t TaskManager::getTaskHandle(TaskType type) const {
    int index = getTaskIndex(type);
    if (index < 0) {
        return nullptr;
    }
    return tasks[index].handle;
}

void TaskManager::deleteAllTasks() {
    for (int i = 0; i < 5; i++) {
        if (tasks[i].isRunning && tasks[i].handle != nullptr) {
            vTaskDelete(tasks[i].handle);
            tasks[i].handle = nullptr;
            tasks[i].isRunning = false;
        }
    }
}

void TaskManager::setTaskConfig(TaskType type, const char* name, uint32_t stackSize, UBaseType_t priority, BaseType_t coreId) {
    int index = getTaskIndex(type);
    if (index < 0) {
        return;
    }
    
    TaskInfo& task = tasks[index];
    task.name = name;
    task.stackSize = stackSize;
    task.priority = priority;
    task.coreId = coreId;
}

void TaskManager::initializeDefaults() {
    // Web Task
    setTaskConfig(TaskType::WEB_TASK, "webTask", 4096, 2);
    
    // NMEA Task
    setTaskConfig(TaskType::NMEA_TASK, "nmeaTask", 8192, 1);
    
    // Background Task
    setTaskConfig(TaskType::BACKGROUND_TASK, "bgTasks", 8192, 3);
    
    // Logging Task
    setTaskConfig(TaskType::LOGGING_TASK, "recordingTask", 4096, 5);
    
    // Web Send Task (pinned to core 1)
    setTaskConfig(TaskType::WEB_SEND_TASK, "sendDataTask", 4096, 1, 1);
}

int TaskManager::getTaskIndex(TaskType type) const {
    switch (type) {
        case TaskType::WEB_TASK: return 0;
        case TaskType::NMEA_TASK: return 1;
        case TaskType::BACKGROUND_TASK: return 2;
        case TaskType::LOGGING_TASK: return 3;
        case TaskType::WEB_SEND_TASK: return 4;
        default: return -1;
    }
}