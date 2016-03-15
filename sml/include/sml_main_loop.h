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
#include <stdbool.h>

#include <sml.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * @defgroup Mainloop Mainloop
 * @brief The sml_main_loop is responsible for providing a mainloop using glib's mainloop.
 * It is also possible to schedule a timer to automatically call ::sml_process
 * @{
 */

/**
 * @brief Init the SML mainloop.
 *
 * @remark The SML mainloop is refcounted, so the number of calls to ::sml_main_loop_init
 * must match the number of calls to ::sml_main_loop_shutdown
 *
 * @return @c 0 on success.
 * @return @c A number < @c 0 on failure.
 *
 * @see ::sml_main_loop_shutdown
 * @see ::sml_main_loop_run
 */
int sml_main_loop_init(void);

/**
 * @brief Start the SML mainloop.
 *
 * @see ::sml_main_loop_quit
 */
void sml_main_loop_run(void);

/**
 * @brief Stops the SML mainloop.
 *
 * @see ::sml_main_loop_run
 * @see ::sml_main_loop_shutdown
 */
void sml_main_loop_quit(void);

/**
 * @brief Clean up the SML mainloop.
 *
 * @see ::sml_main_loop_init
 */
void sml_main_loop_shutdown(void);

/**
 * @brief Schedule a timer to call ::sml_process automatically
 *
 * @param sml The ::sml_object object.
 * @param timeout The interval between ::sml_process calls (milliseconds).
 * @return A timeout_id on success.
 * @return -1 on error.
 *
 * @see ::sml_process
 * @see ::sml_main_loop_unschedule_sml_process
 * @see ::sml_object
 */
int sml_main_loop_schedule_sml_process(struct sml_object *sml, unsigned int timeout);

/**
 * @brief Unschedule the SML mainloop.
 *
 * This function will unschedule the ::sml_process calls.
 *
 * @param sml_timeout_id The timeout_id returned by ::sml_main_loop_schedule_sml_process.
 * @return @c true on success.
 * @return @c false on failure.
 *
 * @see ::sml_main_loop_schedule_sml_process
 * @see ::sml_process
 */
bool sml_main_loop_unschedule_sml_process(int sml_timeout_id);

/**
 * @}
 */
#ifdef __cplusplus
}
#endif
