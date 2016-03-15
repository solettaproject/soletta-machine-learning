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

#include <config.h>
#include <macros.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <sml_log.h>
#include <sml_string.h>

static void
_sml_log_default_handler(enum sml_log_level level, const char *msg, void *data)
{
    switch (level) {
    case SML_LOG_LEVEL_DEBUG:
        fprintf(stdout, "(**SML Debug**) %s\n", msg);
        break;
    case SML_LOG_LEVEL_INFO:
        fprintf(stdout, "(**SML Info**) %s\n", msg);
        break;
    case SML_LOG_LEVEL_WARNING:
        fprintf(stdout, "(**SML Warning**) %s\n", msg);
        break;
    case SML_LOG_LEVEL_ERROR:
        fprintf(stderr, "(**SML Error**) %s\n", msg);
        break;
    case SML_LOG_LEVEL_CRITICAL:
        fprintf(stderr, "(**SML Critical**) %s\n", msg);
        break;
    default:
        fprintf(stdout, "(**SML Unknown log level**) %s\n", msg);
    }
}

static sml_log_handler_cb _handler_cb = _sml_log_default_handler;
static void *_handler_data = NULL;
static enum sml_log_level _log_levels = SML_LOG_LEVEL_ALL;

API_EXPORT void
sml_log_set_log_handler(enum sml_log_level levels, sml_log_handler_cb cb,
    void *data)
{
    _handler_cb = cb;
    _handler_data = data;
    _log_levels = levels;
}

API_EXPORT void
sml_log_print(enum sml_log_level level, const char *format, ...)
{
    struct sml_string *str;
    va_list ap;

    if (!_handler_cb)
        return;

#ifndef Debug
    if ((level & SML_LOG_LEVEL_DEBUG))
        return;
#endif

    if (!(_log_levels & level))
        return;

    str = sml_string_new(NULL);

    if (!str)
        return;

    va_start(ap, format);

    if (sml_string_append_vprintf(str, format, ap))
        _handler_cb(level, sml_string_get_string(str), _handler_data);

    sml_string_free(str);
    va_end(ap);
}
