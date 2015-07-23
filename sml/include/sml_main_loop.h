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
