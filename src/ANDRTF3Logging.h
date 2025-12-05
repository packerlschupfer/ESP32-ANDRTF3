#ifndef ANDRTF3_LOGGING_H
#define ANDRTF3_LOGGING_H

#include <esp_log.h>  // Required for ESP_LOG_* constants

#define ANDRTF3_LOG_TAG "ANDRTF3"

// Define log levels based on debug flag
#ifdef ANDRTF3_DEBUG
    // Debug mode: Show all levels
    #define ANDRTF3_LOG_LEVEL_E ESP_LOG_ERROR
    #define ANDRTF3_LOG_LEVEL_W ESP_LOG_WARN
    #define ANDRTF3_LOG_LEVEL_I ESP_LOG_INFO
    #define ANDRTF3_LOG_LEVEL_D ESP_LOG_DEBUG
    #define ANDRTF3_LOG_LEVEL_V ESP_LOG_VERBOSE
#else
    // Release mode: Only Error, Warn, Info
    #define ANDRTF3_LOG_LEVEL_E ESP_LOG_ERROR
    #define ANDRTF3_LOG_LEVEL_W ESP_LOG_WARN
    #define ANDRTF3_LOG_LEVEL_I ESP_LOG_INFO
    #define ANDRTF3_LOG_LEVEL_D ESP_LOG_NONE  // Suppress
    #define ANDRTF3_LOG_LEVEL_V ESP_LOG_NONE  // Suppress
#endif

// Route to custom logger or ESP-IDF
#ifdef USE_CUSTOM_LOGGER
    #include <LogInterface.h>
    #define ANDRTF3_LOG_E(...) LOG_WRITE(ANDRTF3_LOG_LEVEL_E, ANDRTF3_LOG_TAG, __VA_ARGS__)
    #define ANDRTF3_LOG_W(...) LOG_WRITE(ANDRTF3_LOG_LEVEL_W, ANDRTF3_LOG_TAG, __VA_ARGS__)
    #define ANDRTF3_LOG_I(...) LOG_WRITE(ANDRTF3_LOG_LEVEL_I, ANDRTF3_LOG_TAG, __VA_ARGS__)
    #define ANDRTF3_LOG_D(...) LOG_WRITE(ANDRTF3_LOG_LEVEL_D, ANDRTF3_LOG_TAG, __VA_ARGS__)
    #define ANDRTF3_LOG_V(...) LOG_WRITE(ANDRTF3_LOG_LEVEL_V, ANDRTF3_LOG_TAG, __VA_ARGS__)
#else
    // ESP-IDF logging with compile-time suppression
    #define ANDRTF3_LOG_E(...) ESP_LOGE(ANDRTF3_LOG_TAG, __VA_ARGS__)
    #define ANDRTF3_LOG_W(...) ESP_LOGW(ANDRTF3_LOG_TAG, __VA_ARGS__)
    #define ANDRTF3_LOG_I(...) ESP_LOGI(ANDRTF3_LOG_TAG, __VA_ARGS__)
    #ifdef ANDRTF3_DEBUG
        #define ANDRTF3_LOG_D(...) ESP_LOGD(ANDRTF3_LOG_TAG, __VA_ARGS__)
        #define ANDRTF3_LOG_V(...) ESP_LOGV(ANDRTF3_LOG_TAG, __VA_ARGS__)
    #else
        #define ANDRTF3_LOG_D(...) ((void)0)
        #define ANDRTF3_LOG_V(...) ((void)0)
    #endif
#endif

// Feature-specific debug helpers
#ifdef ANDRTF3_DEBUG
    // Timing macros for performance debugging
    #define ANDRTF3_TIME_START() TickType_t _andrtf3_start = xTaskGetTickCount()
    #define ANDRTF3_TIME_END(msg) ANDRTF3_LOG_D("Timing: %s took %lu ms", msg, pdTICKS_TO_MS(xTaskGetTickCount() - _andrtf3_start))
#else
    #define ANDRTF3_TIME_START() ((void)0)
    #define ANDRTF3_TIME_END(msg) ((void)0)
#endif

#endif // ANDRTF3_LOGGING_H
