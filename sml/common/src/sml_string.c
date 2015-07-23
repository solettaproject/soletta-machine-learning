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

#include <macros.h>
#include <sml_string.h>
#include <string.h>
#include <stdio.h>

#define DEFAULT_SIZE (10)

struct sml_string {
    char *str;
    size_t len;
};

struct sml_string *
sml_string_new(const char *str)
{
    struct sml_string *sml_string = calloc(1, sizeof(struct sml_string));

    ON_NULL_RETURN_VAL(sml_string, NULL);
    if (str) {
        sml_string->str = strdup(str);
        if (!sml_string->str) {
            free(sml_string);
            return NULL;
        }
        sml_string->len = strlen(str);
    }
    return sml_string;
}

int
_sml_string_create_formatted_str(char **buf, const char *format, va_list ap)
{
    va_list aq;
    char *new_buf;
    int r, len;

    len = DEFAULT_SIZE;
    *buf = NULL;

    for (;;) {
        new_buf = realloc(*buf, sizeof(char) * len);

        if (!new_buf)
            goto err_exit;

        *buf = new_buf;

        va_copy(aq, ap);
        r = vsnprintf(*buf, len, format, aq);
        va_end(aq);

        if (r < 0)
            goto err_exit;

        if (r < len)
            break;
        len = r + 1;
    }

    return strlen(*buf);

err_exit:
    free(*buf);
    *buf = NULL;
    return -1;
}

bool
sml_string_append(struct sml_string *sml_string, const char *str)
{
    size_t len;
    char *buf;

    ON_NULL_RETURN_VAL(sml_string, false);
    ON_NULL_RETURN_VAL(str, false);
    len = strlen(str);

    buf = realloc(sml_string->str, sizeof(char) * (len + sml_string->len + 1));
    if (!buf)
        return false;
    sml_string->str = buf;
    memcpy(sml_string->str + sml_string->len, str, len + 1);
    sml_string->len += len;
    return true;
}

bool
sml_string_append_printf(struct sml_string *sml_string, const char *format, ...)
{
    va_list ap;
    bool r;

    ON_NULL_RETURN_VAL(sml_string, false);
    ON_NULL_RETURN_VAL(format, false);

    va_start(ap, format);
    r = sml_string_append_vprintf(sml_string, format, ap);
    va_end(ap);
    return r;
}

bool
sml_string_append_vprintf(struct sml_string *sml_string, const char *format,
    va_list ap)
{
    char *buf, *new_str;
    int len;
    bool r;

    ON_NULL_RETURN_VAL(sml_string, false);
    ON_NULL_RETURN_VAL(format, false);

    r = false;
    buf = NULL;

    len = _sml_string_create_formatted_str(&buf, format, ap);

    if (len < 0)
        goto exit;

    new_str = realloc(sml_string->str,
        sizeof(char) * (len + sml_string->len) + 1);
    if (!new_str)
        goto exit;
    memcpy(new_str + sml_string->len, buf, len + 1);
    sml_string->str = new_str;
    sml_string->len += len;
    r = true;
exit:
    free(buf);
    return r;
}

size_t
sml_string_length(struct sml_string *sml_string)
{
    ON_NULL_RETURN_VAL(sml_string, 0);
    return sml_string->len;
}

const char *
sml_string_get_string(struct sml_string *sml_string)
{
    ON_NULL_RETURN_VAL(sml_string, NULL);
    return sml_string->str;
}

void
sml_string_free(struct sml_string *sml_string)
{
    ON_NULL_RETURN(sml_string);
    free(sml_string->str);
    free(sml_string);
}
