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
    case SML_LOG_LEVEL_WARNING:
        fprintf(stdout, "(**SML Warning**) %s\n", msg);
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
