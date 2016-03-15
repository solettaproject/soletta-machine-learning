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
#include <stdarg.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sml_string;

struct sml_string *sml_string_new(const char *str);
bool sml_string_append_printf(struct sml_string *sml_string, const char *format, ...);
bool sml_string_append(struct sml_string *sml_string, const char *str);
bool sml_string_append_vprintf(struct sml_string *sml_string, const char *format, va_list ap);
size_t sml_string_length(struct sml_string *sml_string);
const char *sml_string_get_string(struct sml_string *sml_string);
void sml_string_free(struct sml_string *sml_string);

#ifdef __cplusplus
}
#endif
