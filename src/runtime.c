/*
 * The MIT License
 *
 * Copyright (c) 2021 Bogdan Migunov bogdanmigunov@yandex.ru
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <stdio.h>

#include "mpd-fnscroller.h"
#include "runtime.h"




static char runtime_dir_path[PATH_STRING_SIZE] = "";

extern bool debug;
extern char pidfile_path[];
extern char sockfile_path[];


static enum mpd_fnscroller_result get_runtime_dir(void);
static enum mpd_fnscroller_result get_pidfile_path(void);
static enum mpd_fnscroller_result get_sockfile_path(void);


enum mpd_fnscroller_result runtime_paths_init(void)
{
    if (!get_runtime_dir())
    {
        ERR_("Unable to get runtime directory path")
        return RESULT_ERROR;
    }

    return get_pidfile_path() | get_sockfile_path();
};


static enum mpd_fnscroller_result get_runtime_dir(void)
{
    struct stat   stat_buffer;
    struct passwd *passwd_buffer;
    char          *xdg_runtime_dir = NULL;
    char          *username;

    if (strlen(runtime_dir_path))
    {
        return RESULT_SUCCESS;
    }

    xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
    memset(runtime_dir_path, '\0', PATH_STRING_SIZE);

    if (xdg_runtime_dir)
    {
        if (strlen(xdg_runtime_dir) > PATH_STRING_SIZE - (strlen(PROGNAME) +
                                      strlen(SOCKFILE_NAME) + 2))
        {
            ERR_("runtime directory path is too long")
            return RESULT_ERROR;
        }
        snprintf(runtime_dir_path, PATH_STRING_SIZE, "%s/" PROGNAME,
                 xdg_runtime_dir);
    }
    else
    {
        passwd_buffer = getpwuid(getuid());
        username = passwd_buffer ? passwd_buffer->pw_name : "unknown";
        snprintf(runtime_dir_path, PATH_STRING_SIZE,
                 TMP_DIR_PATH "/" TMP_RUNTIME_DIR_PREFIX "%s", username);
    }
    if (mkdir(runtime_dir_path, 0750) == -1)
    {
        if (errno == EEXIST)
        {
            stat(runtime_dir_path, &stat_buffer);
            if (!S_ISDIR(stat_buffer.st_mode))
            {
                ERR_("%s is a file", runtime_dir_path)
                return RESULT_ERROR;
            }
        }
        else
        {
            ERR_("error creating runtime directory %s", runtime_dir_path);
            return RESULT_ERROR;
        }
    }

    return RESULT_SUCCESS;
};

static enum mpd_fnscroller_result get_pidfile_path(void)
{
    if (snprintf(pidfile_path, PATH_STRING_SIZE, "%s/" PIDFILE_NAME,
             runtime_dir_path) < 0)
    {
        ERR_("Could not fill pidfile_path buffer")
        return RESULT_ERROR;
    }

    return RESULT_SUCCESS;
};

static enum mpd_fnscroller_result get_sockfile_path(void)
{
    if (snprintf(sockfile_path, PATH_STRING_SIZE, "%s/" SOCKFILE_NAME,
             runtime_dir_path) < 0)
    {
        ERR_("Could not fill sockfile_path buffer")
        return RESULT_ERROR;
    }

    return RESULT_SUCCESS;
};
