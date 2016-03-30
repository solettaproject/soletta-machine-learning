/*
 * This file is part of the Soletta Project
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * @defgroup Log Log
 * @brief These functions and data types are responsible for logging the SML events.
 * @{
 */

/**
 * @enum sml_log_level
 * @brief Log level types
 */
enum sml_log_level {
    SML_LOG_LEVEL_DEBUG = 1 << 0,     /**< Show debug messages. The debug messages will not be logged with SML is compiled in Release mode. */
    SML_LOG_LEVEL_INFO = 1 << 1,     /**< Show info messages. */
    SML_LOG_LEVEL_WARNING = 1 << 2, /**< Show warning messages. */
    SML_LOG_LEVEL_ERROR = 1 << 3, /**< Show error messages. */
    SML_LOG_LEVEL_CRITICAL = 1 << 4, /**< Show critical messages. */
    SML_LOG_LEVEL_ALL = (SML_LOG_LEVEL_DEBUG | SML_LOG_LEVEL_INFO | SML_LOG_LEVEL_WARNING | SML_LOG_LEVEL_ERROR | SML_LOG_LEVEL_CRITICAL) /**< Show all messages. */
};     /**< The types of log levels */

/**
 * @brief Log handler callback.
 *
 * This function is responsible for logging the message.
 * It is called every time ::sml_log_print is called.
 *
 * @param level The message log level.
 * @param msg The message itself.
 * @param data User data that was set with ::sml_log_set_log_handler.
 *
 * @see ::sml_log_set_log_handler
 */
typedef void (*sml_log_handler_cb)(enum sml_log_level level, const char *msg, void *data);

/**
 * @brief Set a log handler.
 *
 * This function is useful if one wants to log SML events in files
 * or do not want to log ::SML_LOG_LEVEL_WARNING messages, for example.
 * SML provides a default ::sml_log_handler_cb that is automatically set at
 * startup, the log level is set to ::SML_LOG_LEVEL_ALL and all messages will be
 * logged at stdout.
 *
 * @param levels The log levels that will be captured.
 * @param cb A user defined callback to a log handler.
 * @param data User data to cb.
 *
 * @see ::sml_log_handler_cb
 */
void sml_log_set_log_handler(enum sml_log_level levels, sml_log_handler_cb cb, void *data);

/**
 * @brief Prints a message with a desired log level.
 *
 * @param level The desired log level.
 * @param format A formatted string.
 * @param ... Contents of the formatted string.
 */
void sml_log_print(enum sml_log_level level, const char *format, ...);

/**
 * @brief Syntactic sugar to ::sml_log_print(SML_LOG_LEVEL_DEBUG, "debug message")
 *
 * @param ... A formatted message
 *
 * @see ::sml_log_print
 */
#define sml_debug(...) sml_log_print(SML_LOG_LEVEL_DEBUG, __VA_ARGS__)

/**
 * @brief Syntactic sugar to ::sml_log_print(SML_LOG_LEVEL_INFO, "info message")
 *
 * @param ... A formatted message
 *
 * @see ::sml_log_print
 */
#define sml_info(...) sml_log_print(SML_LOG_LEVEL_INFO, __VA_ARGS__)

/**
 * @brief Syntactic sugar to ::sml_log_print(SML_LOG_LEVEL_WARNING, "warning message")
 *
 * @param ... A formatted message
 *
 * @see ::sml_log_print
 */
#define sml_warning(...) sml_log_print(SML_LOG_LEVEL_WARNING, __VA_ARGS__)

/**
 * @brief Syntactic sugar to ::sml_log_print(SML_LOG_LEVEL_ERROR, "error message")
 *
 * @param ... A formatted message
 *
 * @see ::sml_log_print
 */
#define sml_error(...) sml_log_print(SML_LOG_LEVEL_ERROR, __VA_ARGS__)

/**
 * @brief Syntactic sugar to ::sml_log_print(SML_LOG_LEVEL_CRITICAL, "critical message")
 *
 * @param ... A formatted message
 *
 * @see ::sml_log_print
 */
#define sml_critical(...) sml_log_print(SML_LOG_LEVEL_CRITICAL, __VA_ARGS__)

/**
 * @}
 */
#ifdef __cplusplus
}
#endif
