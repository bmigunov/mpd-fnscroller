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


#ifndef SERVER_H
#define SERVER_H


#include <pthread.h>
#include <mpd/client.h>

#include "mpd-fnscroller.h"




#define MPD_DEFAULT_HOST    "localhost"
#define MPD_DEFAULT_PORT    6600
#define MPD_DEFAULT_TIMEOUT 30

#define DELIMETER_DEFAULT_STRING " | "
#define DELIMETER_STR_SIZE       4

#define PID_STRING_SIZE 6


enum server_status
{
    STATUS_OK,
    STATUS_INITIALIZING,
    STATUS_SERVE_THREAD_ISSUE,
    STATUS_MPD_EVENT_HANDLER_ISSUE,
    STATUS_SHUTDOWN,
    STATUS_COUNT
};

struct mpd_fnscroller_server
{
    char                  mpd_host[HOSTNAME_STRING_SIZE];
    unsigned int          mpd_port;
    unsigned int          mpd_timeout;

    volatile unsigned int current_string_size;
    char                  fn_string[FILENAME_STRING_SIZE];
    wchar_t               fn_wcstring[FILENAME_WCHAR_STRING_SIZE];
    volatile unsigned int fn_wcstring_offset;

    int                   pidfile_fd;

    pthread_t             serve_thread_id;
    int                   sock_listener;
};


enum mpd_fnscroller_result server_init(struct mpd_fnscroller_server *server);
enum mpd_fnscroller_result mpd_server_defaults_get(char *host,
                                                   unsigned int *port);

enum mpd_fnscroller_result server_run(struct mpd_fnscroller_server *server);


#endif /* SERVER_H */
