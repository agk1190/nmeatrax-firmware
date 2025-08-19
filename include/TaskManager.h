/**
 * NMEATrax Task Manager
 * 
 * @authors Alex Klouda, Greyson Stelmaschuk
 * 
 * Centralizes FreeRTOS task lifecycle management and reduces global task handles.
 */

#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

enum class TaskType {
    WEB_TASK,
    NMEA_TASK,
    BACKGROUND_TASK,
    LOGGING_TASK,
    WEB_SEND_TASK
};

struct TaskInfo {
    TaskHandle_t handle;
    const char* name;
    uint32_t stackSize;
    UBaseType_t priority;
    BaseType_t coreId;
    bool isRunning;
    
    TaskInfo() : handle(nullptr), name(""), stackSize(0), priority(0), coreId(-1), isRunning(false) {}
};

class TaskManager {
public:
    static TaskManager& getInstance();
    
    // Task lifecycle management
    bool createTask(TaskType type, TaskFunction_t taskFunction, void* parameters = nullptr);
    bool deleteTask(TaskType type);
    bool suspendTask(TaskType type);
    bool resumeTask(TaskType type);
    bool restartTask(TaskType type, TaskFunction_t taskFunction, void* parameters = nullptr);
    
    // Task status
    bool isTaskRunning(TaskType type) const;
    TaskHandle_t getTaskHandle(TaskType type) const;
    
    // Cleanup all tasks
    void deleteAllTasks();
    
    // Task configuration
    void setTaskConfig(TaskType type, const char* name, uint32_t stackSize, UBaseType_t priority, BaseType_t coreId = -1);

private:
    TaskManager() = default;
    ~TaskManager() = default;
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;
    
    TaskInfo tasks[5]; // One for each TaskType
    bool initialized = false;
    
    void initializeDefaults();
    int getTaskIndex(TaskType type) const;
};

#endif // TASK_MANAGER_H