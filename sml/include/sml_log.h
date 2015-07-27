/*
 * This file is part of the Soletta Project
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
    SML_LOG_LEVEL_DEBUG = 1 << 0,     /**< Show debug messages. The debug messages will not be loged with SML is compiled in Release mode. */
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
 * This function is usefull if one wants to log SML events in files
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
 * @brief Syntatic sugar to ::sml_log_print(SML_LOG_LEVEL_DEBUG, "debug message")
 *
 * @param ... A formatted message
 *
 * @see ::sml_log_print
 */
#define sml_debug(...) sml_log_print(SML_LOG_LEVEL_DEBUG, __VA_ARGS__)

/**
 * @brief Syntatic sugar to ::sml_log_print(SML_LOG_LEVEL_INFO, "info message")
 *
 * @param ... A formatted message
 *
 * @see ::sml_log_print
 */
#define sml_info(...) sml_log_print(SML_LOG_LEVEL_INFO, __VA_ARGS__)

/**
 * @brief Syntatic sugar to ::sml_log_print(SML_LOG_LEVEL_WARNING, "warning message")
 *
 * @param ... A formatted message
 *
 * @see ::sml_log_print
 */
#define sml_warning(...) sml_log_print(SML_LOG_LEVEL_WARNING, __VA_ARGS__)

/**
 * @brief Syntatic sugar to ::sml_log_print(SML_LOG_LEVEL_ERROR, "error message")
 *
 * @param ... A formatted message
 *
 * @see ::sml_log_print
 */
#define sml_error(...) sml_log_print(SML_LOG_LEVEL_ERROR, __VA_ARGS__)

/**
 * @brief Syntatic sugar to ::sml_log_print(SML_LOG_LEVEL_CRITICAL, "critical message")
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
