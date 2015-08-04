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

#include <stdbool.h>
#include <glib.h>
#include <glib-unix.h>

#include <sml_main_loop.h>
#include <sml.h>
#include <sml_log.h>
#include <macros.h>

static GMainLoop *loop;
static int _init_count;

static gboolean
_on_signal(gpointer ptr)
{
    sml_debug("Got signal, quit");
    sml_main_loop_quit();
    return true;
}

API_EXPORT int
sml_main_loop_init(void)
{
    _init_count++;
    if (_init_count > 1)
        return 0;

    loop = g_main_loop_new(NULL, false);
    if (!loop) {
        sml_critical("Failed to create main loop");
        _init_count = 0;
        return errno ? -errno : -ENOENT;
    }

    g_unix_signal_add(SIGINT, _on_signal, loop);
    g_unix_signal_add(SIGTERM, _on_signal, loop);

    sml_debug("Main loop initialized");
    return 0;
}

API_EXPORT void
sml_main_loop_run(void)
{
    if (_init_count == 0) {
        sml_critical("Init main loop first");
        return;
    }
    sml_debug("Run main loop");
    g_main_loop_run(loop);
}

API_EXPORT void
sml_main_loop_quit(void)
{
    if (_init_count == 0) {
        sml_critical("No main loop initialized");
        return;
    }
    g_main_loop_quit(loop);
    sml_debug("Main loop quit");
}

API_EXPORT void
sml_main_loop_shutdown(void)
{
    if (_init_count == 0) {
        sml_critical("Init main loop first");
        return;
    }
    if (--_init_count > 0) return;

    g_main_loop_unref(loop);
    loop = NULL;
    sml_debug("Main loop shutdown");
}

static gboolean
_process_timeout(gpointer data)
{
    struct sml_object *sml = data;

    if (sml_process(sml) < 0) {
        sml_critical("Failed to process, removing timer");
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

API_EXPORT int
sml_main_loop_schedule_sml_process(struct sml_object *sml, unsigned int timeout)
{
    int timeout_id;

    if (!sml) {
        sml_critical("No sml, no process scheduled");
        return -1;
    }

    timeout_id = g_timeout_add(timeout, _process_timeout, sml);

    if (timeout_id > 0) {
        sml_debug("Scheduled process timeout with id: %d", timeout_id);
        return timeout_id;
    }

    sml_critical("Could not schedule process timeout");
    return -1;
}

API_EXPORT bool
sml_main_loop_unschedule_sml_process(int sml_timeout_id)
{
    sml_debug("Removing scheduled process timeout %d", sml_timeout_id);
    return g_source_remove(sml_timeout_id);
}
