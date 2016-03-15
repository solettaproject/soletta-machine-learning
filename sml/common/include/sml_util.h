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
#include <sml.h>

#ifdef __cplusplus
extern "C" {
#endif
#define SML_PATH_MAX (4096)

bool is_file(const char *path);
bool is_dir(const char *path);
bool clean_dir(const char *path, const char *prefix);
bool delete_file(const char *path);
bool file_exists(const char *path);
bool create_dir(const char *path);

#ifdef __cplusplus
}
#endif
