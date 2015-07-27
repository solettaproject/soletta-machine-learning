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

#include "sml_util.h"
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sml_log.h>

bool
is_file(const char *path)
{
    struct stat stat_result;

    if (!stat(path, &stat_result) && S_ISREG(stat_result.st_mode))
        return true;

    return false;
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
