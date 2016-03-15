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
