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
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <stdbool.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdio.h>
#include <pthread.h>
#include <wchar.h>
#include <mpd/client.h>

#include "mpd-fnscroller.h"
#include "server.h"




extern bool daemonize_service;
extern bool debug;
extern char pidfile_path[];
extern char sockfile_path[];

volatile static struct mpd_fnscroller_server *mpd_fnscroller_server = NULL;
volatile static unsigned int                 filename_part_buf_offset = 0;
volatile static enum server_status           status = STATUS_COUNT;
static pthread_mutex_t                       lock;


static void server_shutdown_handler(int sig);

static void daemonize(int *pidfile_fd);
static enum mpd_fnscroller_result
serve_thread_start(struct mpd_fnscroller_server *server);
static void *client_serve(void *arg);
static enum mpd_fnscroller_result
filename_part_send(struct mpd_fnscroller_server *server, int sock,
                   unsigned int bufsize);
static enum mpd_fnscroller_result
mpd_event_handler_loop(struct mpd_fnscroller_server *server);
static enum mpd_fnscroller_result
mpd_fn_string_get(struct mpd_fnscroller_server *server,
                  struct mpd_connection *connection);

static void server_cleanup(void);


enum mpd_fnscroller_result server_init(struct mpd_fnscroller_server *server)
{
    mpd_fnscroller_server = server;

    status = STATUS_INITIALIZING;
    server->mpd_timeout = MPD_DEFAULT_TIMEOUT;
    if (!mpd_server_defaults_get(server->mpd_host, &server->mpd_port))
    {
        ERR_("Unable to get default mpd server hostname and port")
        return RESULT_ERROR;
    }

    server->current_string_size = 0;

    filename_part_buf_offset = 0;

    memset(server->fn_string, '\0', FILENAME_STRING_SIZE);
    memset(server->fn_wcstring, '\0', sizeof(wchar_t) *
           FILENAME_WCHAR_STRING_SIZE);

    server->pidfile_fd = 0;

    server->sock_listener = 0;

    signal(SIGUSR1, server_shutdown_handler);

    return RESULT_SUCCESS;
};

enum mpd_fnscroller_result mpd_server_defaults_get(char *host,
                                                   unsigned int *port)
{
    char *mpd_env_variable_host = getenv(MPD_ENV_VARIABLE_HOST);
    char *mpd_env_variable_port = getenv(MPD_ENV_VARIABLE_PORT);
    char *invalid_numchar = NULL;

    memset(host, '\0', HOSTNAME_STRING_SIZE);
    if (mpd_env_variable_host)
    {
        if (strlen(mpd_env_variable_host) > HOSTNAME_STRING_SIZE - 1)
        {
            ERR_(MPD_ENV_VARIABLE_HOST " variable value too is long")
            return RESULT_ERROR;
        }
        else
        {
            strncpy(host, mpd_env_variable_host, HOSTNAME_STRING_SIZE - 1);
        }
    }
    else
    {
        strncpy(host, MPD_DEFAULT_HOST, HOSTNAME_STRING_SIZE - 1);
    }

    if (mpd_env_variable_port)
    {
        *port = strtol(mpd_env_variable_port, &invalid_numchar, DEC);
        if (*invalid_numchar)
        {
            ERR_("Invalid " MPD_ENV_VARIABLE_PORT "variable value")
            return RESULT_ERROR;
        }
    }
    else
    {
        *port = MPD_DEFAULT_PORT;
    }

    return RESULT_SUCCESS;
};

static void server_shutdown_handler(int sig)
{
    TRACE_()
    DEBUG_("SIGNAL: %d; current server status: %d", sig, status)

    if (sig == SIGUSR1)
    {
        pthread_mutex_lock(&lock);
        status = STATUS_SHUTDOWN;
        pthread_mutex_unlock(&lock);

        server_cleanup();
        exit(EXIT_SUCCESS);
    }
    else
    {
        ERR_("Unhandled signal %d", sig)

        pthread_mutex_lock(&lock);
        status = STATUS_SHUTDOWN;
        pthread_mutex_unlock(&lock);

        server_cleanup();
        exit(EXIT_FAILURE);
    }

    return;
};


enum mpd_fnscroller_result server_run(struct mpd_fnscroller_server *server)
{
    enum mpd_fnscroller_result result;

    TRACE_()

    syslog(LOG_INFO, PROGNAME " server is started");

    status = STATUS_OK;
    if (daemonize_service)
    {
        daemonize(&server->pidfile_fd);
    }

    if (!serve_thread_start(server))
    {
        TRACE_()
        DEBUG_("Server status: %d", status)
        server_cleanup();
        return RESULT_ERROR;
    }

    result = mpd_event_handler_loop(server);

    DEBUG_("Server status: %d; event handler loop retval: %d", status,
           result)
    server_cleanup();
    return result;
};


static void daemonize(int *pidfd)
{
    struct stat stat_buffer;
    char        pid_str[PID_STRING_SIZE];
    int         fd = 0;
    pid_t       pid;

    TRACE_()

    memset(pid_str, '\0', PID_STRING_SIZE);
    pid = fork();
    if (pid == -1)
    {
        ERR_("Could not daemonize process")
        closelog();
        exit(EXIT_FAILURE);
    }
    else
    {
        if (pid)
        {
            exit(EXIT_SUCCESS);
        }
    }

    if (setsid() == -1)
    {
        ERR_("Could not daemonize process")
        closelog();
        exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, SIG_IGN);

    pid = fork();
    if (pid == -1)
    {
        ERR_("Could not daemonize process")
        closelog();
        exit(EXIT_FAILURE);
    }
    else
    {
        if (pid)
        {
            exit(EXIT_SUCCESS);
        }
    }

    chdir(getenv("HOME"));

    for (fd = sysconf(_SC_OPEN_MAX); fd > 0; --fd)
    {
        close(fd);
    }
    stdin = fopen("/dev/null", "r");
    stdout = fopen("/dev/null", "w+");
    stderr = fopen("/dev/null", "w+");

    if (stat(pidfile_path, &stat_buffer) == 0)
    {
        ERR_("pidfile %s already exists", pidfile_path)
        exit(EXIT_FAILURE);
    }

    *pidfd = creat(pidfile_path, 0660);
    if (*pidfd == -1)
    {
        ERR_("Could not daemonize process: error creating pidfile")
        closelog();
        exit(EXIT_FAILURE);
    }

    snprintf(pid_str, PID_STRING_SIZE, "%d", getpid());
    if (lockf(*pidfd, F_TLOCK, 0) == -1)
    {
        ERR_("Could not daemonize process: issue locking pidfile")
        closelog();
        close(*pidfd);
        unlink(pidfile_path);
        exit(EXIT_FAILURE);
    }
    write(*pidfd, pid_str, strlen(pid_str));

    return;
};

static enum mpd_fnscroller_result
serve_thread_start(struct mpd_fnscroller_server *server)
{
    TRACE_()

    if (pthread_create(&server->serve_thread_id, NULL, client_serve,
                       server))
    {
        ERR_("Issue creating server thread")
        return RESULT_ERROR;
    }
    if (pthread_detach(server->serve_thread_id))
    {
        ERR_("Issue detaching server thread")
        pthread_cancel(server->serve_thread_id);
        return RESULT_ERROR;
    }

    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        ERR_("Issue initializing mutex")
        pthread_cancel(server->serve_thread_id);
        return RESULT_ERROR;
    }

    return RESULT_SUCCESS;
};

static void *client_serve(void *arg)
{
    struct mpd_fnscroller_server *server = arg;
    struct sockaddr_un           server_sockaddr;
    struct sockaddr_un           connected_client;
    int                          sock_connection = 0;
    socklen_t                    socklen;
    unsigned int                 client_msg;
    char                         client_wcbufsize = 0;

    TRACE_()

    unlink(sockfile_path);
    server->sock_listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server->sock_listener == -1)
    {
        ERR_("Issue creating server side socket")

        pthread_mutex_lock(&lock);
        status = STATUS_SERVE_THREAD_ISSUE;
        pthread_mutex_unlock(&lock);

        pthread_exit(NULL);
    }

    server_sockaddr.sun_family = AF_UNIX;
    strncpy(server_sockaddr.sun_path, sockfile_path, SUN_PATH_STRING_SIZE);
    if (bind(server->sock_listener, (struct sockaddr *)&server_sockaddr,
             sizeof(server_sockaddr)) < 0)
    {
        ERR_("Issue binding server side socket")

        pthread_mutex_lock(&lock);
        status = STATUS_SERVE_THREAD_ISSUE;
        pthread_mutex_unlock(&lock);

        pthread_exit(NULL);
    }

    while(status == STATUS_OK)
    {
        if (listen(server->sock_listener, 1))
        {
            ERR_("Issue listening sock_listener")

            pthread_mutex_lock(&lock);
            status = STATUS_SERVE_THREAD_ISSUE;
            pthread_mutex_unlock(&lock);

            pthread_exit(NULL);
        }
        sock_connection = accept(server->sock_listener,
                                 (struct sockaddr *)&connected_client,
                                 &socklen);
        if (sock_connection == -1)
        {
            ERR_("Issue accepting incoming connection")

            pthread_mutex_lock(&lock);
            status = STATUS_SERVE_THREAD_ISSUE;
            pthread_mutex_unlock(&lock);

            pthread_exit(NULL);
        }

        if (recv(sock_connection, &client_msg, sizeof(unsigned int),
                 0) > sizeof(unsigned int))
        {
            ERR_("Issue recieving client message")

            pthread_mutex_lock(&lock);
            status = STATUS_SERVE_THREAD_ISSUE;
            pthread_mutex_unlock(&lock);

            close(sock_connection);
            pthread_exit(NULL);
        }
        if (client_msg != client_wcbufsize)
        {
            client_wcbufsize = client_msg;

            pthread_mutex_lock(&lock);
            server->fn_wcstring_offset = 0;
            filename_part_buf_offset = 0;
            pthread_mutex_unlock(&lock);
        }

        if (!filename_part_send(server, sock_connection, client_wcbufsize))
        {
            ERR_("Issue sending current filename string part")

            pthread_mutex_lock(&lock);
            status = STATUS_SERVE_THREAD_ISSUE;
            pthread_mutex_unlock(&lock);

            close(sock_connection);
            pthread_exit(NULL);
        }
        close(sock_connection);
    }

    pthread_exit(NULL);
};

static enum mpd_fnscroller_result
filename_part_send(struct mpd_fnscroller_server *server, int sock,
                   unsigned int wcbufsize)
{
    wchar_t filename_part_buf[FILENAME_WCHAR_STRING_SIZE];
    wchar_t wc_delimeter[DELIMETER_STR_SIZE];
    ssize_t bytes_sent;

    TRACE_()

    mbstowcs(wc_delimeter, DELIMETER_DEFAULT_STRING, DELIMETER_STR_SIZE);
    memset(filename_part_buf, '\0', sizeof(wchar_t) *
           FILENAME_WCHAR_STRING_SIZE);

    pthread_mutex_lock(&lock);
// Song filename is less than buffer to send
    if (wcslen(server->fn_wcstring) < wcbufsize)
    {
        TRACE_()

        wcsncpy(filename_part_buf, server->fn_wcstring, wcbufsize - 1);
    }
    else
    {
// Sending everything before delimeter
        if (server->fn_wcstring_offset <=
            wcslen(server->fn_wcstring) - (wcbufsize - 1))
        {
            TRACE_()

            wcsncpy(filename_part_buf,
                    server->fn_wcstring + server->fn_wcstring_offset,
                    wcbufsize - 1);

            ++server->fn_wcstring_offset;
            DEBUG_("filename_part_buf: %ls; fn_wcstring_offset: %d",
                    filename_part_buf, server->fn_wcstring_offset)
        }
// Sending [filename ending] [delimeter] [filename beginning]
        else
        {
            if (wcslen(server->fn_wcstring + server->fn_wcstring_offset))
            {
                TRACE_()

                wcsncpy(filename_part_buf,
                        server->fn_wcstring + server->fn_wcstring_offset,
                        wcbufsize - 1);
                wcsncat(filename_part_buf, wc_delimeter,
                        (wcbufsize - 1) - wcslen(filename_part_buf));
                if (server->fn_wcstring_offset - wcslen(wc_delimeter) >
                    wcslen(wc_delimeter))
                {
                    wcsncat(filename_part_buf, server->fn_wcstring,
                            (wcbufsize - 1) - wcslen(filename_part_buf));
                }
            }
// Sending [delimeter] [filename beginning]
            else
            {
                TRACE_()

                wcsncpy(filename_part_buf,
                        wc_delimeter + filename_part_buf_offset + 1 -
                        (wcbufsize - 1), wcbufsize - 1);
                wcsncat(filename_part_buf, server->fn_wcstring,
                        (wcbufsize - 1) - wcslen(filename_part_buf));
            }

            ++filename_part_buf_offset;
            ++server->fn_wcstring_offset;
            if (filename_part_buf_offset >=
                (wcbufsize - 1) + wcslen(wc_delimeter))
            {
                TRACE_()

                filename_part_buf_offset = 0;
                server->fn_wcstring_offset = 0;
            }
        }
    }
    pthread_mutex_unlock(&lock);

    DEBUG_("filename_part_buf: %ls; wcbufsize: %d; fn_wcstring_offset: %d; filename_part_buf_offset: %d",
           filename_part_buf, wcbufsize, server->fn_wcstring_offset,
           filename_part_buf_offset)
    bytes_sent = send(sock, filename_part_buf, wcbufsize * sizeof(wchar_t), 0);
    if (bytes_sent == -1)
    {
        ERR_("Could not send entire filename; bytes_sent: %ld", bytes_sent)
        return RESULT_ERROR;
    }

    return RESULT_SUCCESS;
};

static enum mpd_fnscroller_result
mpd_event_handler_loop(struct mpd_fnscroller_server *server)
{
    struct  mpd_connection *mpd_connection;
    ssize_t req_wcstring_size;

    TRACE_()

    mpd_connection = mpd_connection_new(server->mpd_host, server->mpd_port,
                                        server->mpd_timeout * 1000);
    if (mpd_connection_get_error(mpd_connection) != MPD_ERROR_SUCCESS)
    {
        ERR_("Could not establish connection with mpd")

        pthread_mutex_lock(&lock);
        status = STATUS_MPD_EVENT_HANDLER_ISSUE;
        pthread_mutex_unlock(&lock);

        mpd_connection_free(mpd_connection);
        return RESULT_ERROR;
    }

    if (!mpd_fn_string_get(server, mpd_connection))
    {
        ERR_("Could not get fn_string for the first time")

        pthread_mutex_lock(&lock);
        status = STATUS_MPD_EVENT_HANDLER_ISSUE;
        pthread_mutex_unlock(&lock);

        mpd_connection_free(mpd_connection);
        return RESULT_ERROR;
    }

    req_wcstring_size = mbstowcs(NULL, server->fn_string, 0) + 1;
    if (req_wcstring_size > FILENAME_WCHAR_STRING_SIZE)
    {
        ERR_("req_wcstring_size: %ld; FILENAME_WCHAR_STRING_SIZE: %d",
             req_wcstring_size, FILENAME_WCHAR_STRING_SIZE)
        mpd_connection_free(mpd_connection);
        return RESULT_ERROR;
    }

    mbstowcs(server->fn_wcstring, server->fn_string,
             FILENAME_WCHAR_STRING_SIZE);
    DEBUG_("fn_string: %s; fn_wcstring: %ls", server->fn_string,
           server->fn_wcstring)

    DEBUG_("Entering event handler loop")
    while(status == STATUS_OK && mpd_run_idle_mask(mpd_connection,
                                                   MPD_IDLE_PLAYER))
    {
        if (!mpd_fn_string_get(server, mpd_connection))
        {
            pthread_mutex_lock(&lock);
            status = STATUS_MPD_EVENT_HANDLER_ISSUE;
            pthread_mutex_unlock(&lock);

            mpd_connection_free(mpd_connection);
        }

        req_wcstring_size = mbstowcs(NULL, server->fn_string, 0) + 1;
        if (req_wcstring_size > FILENAME_WCHAR_STRING_SIZE)
        {
            ERR_("req_wcstring_size: %ld; FILENAME_WCHAR_STRING_SIZE: %d",
                 req_wcstring_size, FILENAME_WCHAR_STRING_SIZE)
            mpd_connection_free(mpd_connection);
            return RESULT_ERROR;
        }

        mbstowcs(server->fn_wcstring, server->fn_string,
                 FILENAME_WCHAR_STRING_SIZE);
        DEBUG_("fn_string: %s; fn_wcstring: %ls", server->fn_string, server->fn_wcstring)
    }

    if (status == STATUS_SHUTDOWN)
    {
        TRACE_()
        syslog(LOG_WARNING, "Server shutdown");
        mpd_connection_free(mpd_connection);
        return RESULT_SUCCESS;
    }
    else
    {
        ERR_("Server status is not ok")
        mpd_connection_free(mpd_connection);
        return RESULT_ERROR;
    }
};

static enum mpd_fnscroller_result
mpd_fn_string_get(struct mpd_fnscroller_server *server,
                  struct mpd_connection *connection)
{
    struct mpd_song *mpd_song;
    enum mpd_state  mpd_state;
    const char      *mpd_song_uri;

    pthread_mutex_lock(&lock);
    memset(server->fn_string, '\0', FILENAME_STRING_SIZE);
    memset(server->fn_wcstring, '\0',
           sizeof(wchar_t) * FILENAME_WCHAR_STRING_SIZE);
    pthread_mutex_unlock(&lock);

    TRACE_()

    mpd_command_list_begin(connection, true);
    mpd_send_status(connection);
    mpd_send_current_song(connection);
    mpd_command_list_end(connection);

    mpd_state = mpd_status_get_state(mpd_recv_status(connection));
    DEBUG_("mpd_state: %d", mpd_state)
    switch(mpd_state)
    {
        case MPD_STATE_PAUSE:

        case MPD_STATE_PLAY:
            mpd_response_next(connection);
            mpd_song = mpd_recv_song(connection);
            mpd_song_uri = mpd_song_get_uri(mpd_song);

            pthread_mutex_lock(&lock);
            snprintf(server->fn_string, FILENAME_STRING_SIZE, "%s",
                     basename((char *)mpd_song_uri));
            pthread_mutex_unlock(&lock);

            mpd_song_free(mpd_song);
            mpd_response_finish(connection);

            pthread_mutex_lock(&lock);
            server->fn_wcstring_offset = 0;
            filename_part_buf_offset = 0;
            pthread_mutex_unlock(&lock);

            return RESULT_SUCCESS;

        case MPD_STATE_STOP:
            mpd_response_next(connection);
            mpd_response_finish(connection);

            pthread_mutex_lock(&lock);
            server->fn_wcstring_offset = 0;
            filename_part_buf_offset = 0;
            snprintf(server->fn_string, FILENAME_STRING_SIZE, "STOP");
            pthread_mutex_unlock(&lock);

            return RESULT_SUCCESS;

        default:
            ERR_("MPD_STATE_UNKNOWN")
            return RESULT_ERROR;
    }
};


static void server_cleanup(void)
{
    TRACE_()

    pthread_mutex_destroy(&lock);
    pthread_cancel(mpd_fnscroller_server->serve_thread_id);

    if (daemonize_service)
    {
        lockf(mpd_fnscroller_server->pidfile_fd, F_ULOCK, 0);
        close(mpd_fnscroller_server->pidfile_fd);
        unlink(pidfile_path);
    }

    close(mpd_fnscroller_server->sock_listener);
    unlink(sockfile_path);

    return;
};
