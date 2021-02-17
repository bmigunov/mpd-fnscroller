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


#ifndef MPD_FNSCROLLER_H
#define MPD_FNSCROLLER_H


#include <sys/param.h>
#ifdef __linux__
#include <linux/limits.h>
#endif /* __linux__ */
#include <syslog.h>


#define PROGNAME                     "mpd-fnscroller"
#define MPD_FNSCROLLER_VERSION_MAJOR 1
#define MPD_FNSCROLLER_VERSION_MINOR 0
#define MPD_FNSCROLLER_VERSION_PATCH 0

#define MPD_FNSCROLLER_HELP_STR       "Get filename of the song currently "    \
                                      "played by mpd. Meant to be used with"   \
                                      "i3blocks\n" MPD_FNSCROLLER_USAGE_STR " "\
                                      "   -h Show this message\n    -d Enable "\
                                      "debug\n    -s Launch in server mode\n  "\
                                      "  -n Do not daemonize server\n    -c "  \
                                      "Launch in client mode and get current " \
                                      "piece of the filename\n    -t Set "     \
                                      "MPD server connection timeout (for the "\
                                      "mpd-fnscroller server routine)\n    -q "\
                                      "Shutdown server instance\n    -v Show " \
                                      "program version\n"
#define MPD_FNSCROLLER_USAGE_STR      "Usage:\n" PROGNAME" [-h] [-d] [-s "     \
                                      "<host>:<port> | "                       \
                                      MPD_FNSCROLLER_DEFAULT_OPTARG " [-n] "   \
                                      "[-t <timeout> | "                       \
                                      MPD_FNSCROLLER_DEFAULT_OPTARG "] [-c "   \
                                      "<strlen> | "                            \
                                      MPD_FNSCROLLER_DEFAULT_OPTARG "] [-q] [" \
                                      "-v]\n"
#define MPD_FNSCROLLER_DEFAULT_OPTARG "default"

#define MPD_ENV_VARIABLE_HOST "MPD_HOST"
#define MPD_ENV_VARIABLE_PORT "MPD_PORT"

#define DEFAULT_OUTPUT_STRING_SIZE 25
#ifdef __linux__
#define PATH_STRING_SIZE           PATH_MAX
#define FILENAME_STRING_SIZE       NAME_MAX + 1
#else
#define PATH_STRING_SIZE           256
#define FILENAME_STRING_SIZE       PATH_STRING_SIZE - 1
#endif /* __linux__ */
#ifdef __SIZEOF_WCHAR_T__
#define FILENAME_WCHAR_STRING_SIZE FILENAME_STRING_SIZE / __SIZEOF_WCHAR_T__
#else
#define FILENAME_WCHAR_STRING_SIZE FILENAME_STRING_SIZE / 4
#endif /* __SIZEOF_WCHAR_T__ */
#ifdef MAXHOSTNAMELEN
#define HOSTNAME_STRING_SIZE       MAXHOSTNAMELEN + 1
#else
#define HOSTNAME_STRING_SIZE       256
#endif /* MAXHOSTNAMELEN */
#define SUN_PATH_STRING_SIZE       92

#define DEC 10


#define DEBUG_(fmt, ...)                       \
    if (debug)                                 \
    {                                          \
        syslog(LOG_DEBUG, fmt, ##__VA_ARGS__); \
    }

#define ERR_(fmt, ...)                                             \
    syslog(LOG_DEBUG, "%s:%s():%d", __FILE__, __func__, __LINE__); \
    syslog(LOG_ERR, fmt, ##__VA_ARGS__);

#define TRACE_() DEBUG_("%s:%s():%d", __FILE__, __func__, __LINE__)




enum mpd_fnscroller_mode
{
    SERVER_MODE = 0,
    CLIENT_MODE,
    MODE_COUNT
};

enum mpd_fnscroller_result
{
    RESULT_ERROR = 0,
    RESULT_SUCCESS,
    RESULT_COUNT
};


#endif /* MPD_FNSCROLLER_H */
