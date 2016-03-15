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

#include "sml_util.h"
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sml_log.h>
#include <errno.h>

bool
is_file(const char *path)
{
    struct stat stat_result;

    if (!stat(path, &stat_result) && S_ISREG(stat_result.st_mode))
        return true;

    return false;
}

bool
file_exists(const char *path)
{
    struct stat stat_result;

    if (!stat(path, &stat_result))
        return true;
    return false;
}

bool
create_dir(const char *path)
{
    size_t i, last_i, bytes;
    char aux[PATH_MAX];

    if (strlen(path) + 1 > PATH_MAX) {
        sml_critical("Could not create dir. The path is bigger than PATH_MAX!");
        return false;
    }
    memset(aux, 0, sizeof(aux));
    for (i = 0, last_i = 0; path[i]; i++) {
        if ((path[i] == '/' && i != 0) || path[i + 1] == '\0') {
            bytes = i - last_i;
            memcpy(aux + last_i, path + last_i, path[i + 1] == '\0' ? bytes + 1 : bytes);
            last_i = i;
            /* Ignore the errors, if everything is ok is_dir() will succeed */
            (void)mkdir(aux, S_IRWXU | S_IRWXG | S_IROTH);
        }
    }
    return is_dir(path);
}

bool
is_dir(const char *path)
{
    struct stat stat_result;

    if (!stat(path, &stat_result) && S_ISDIR(stat_result.st_mode))
        return true;

    return false;
}

bool
clean_dir(const char *path, const char *prefix)
{
    DIR *dir;
    struct dirent *file;
    char buf[SML_PATH_MAX];
    int prefix_len;

    if (!prefix) {
        sml_error("Could not clear dir, missing prefix");
        return false;
    }

    dir = opendir(path);
    if (!dir) {
        sml_error("Failed to open dir %s", path);
        return false;
    }

    prefix_len = strlen(prefix);

    while ((file = readdir(dir))) {
        if (strncmp(file->d_name, prefix, prefix_len))
            continue;
        snprintf(buf, SML_PATH_MAX, "%s/%s", path, file->d_name);
        if (remove(buf)) {
            sml_error("Failed to remove %s", buf);
            closedir(dir);
            return false;
        }
    }

    closedir(dir);
    return true;
}

bool
delete_file(const char *path)
{
    if (!remove(path))
        return true;
    sml_critical("Could not remove the path:%s", path);
    return false;
}
