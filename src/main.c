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
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <stdio.h>
#include <signal.h>
#include <locale.h>

#include "mpd-fnscroller.h"
#include "runtime.h"
#include "server.h"
#include "client.h"




bool daemonize_service = true;
bool debug = false;
char pidfile_path[PATH_STRING_SIZE];
char sockfile_path[SUN_PATH_STRING_SIZE];

struct mpd_fnscroller_master
{
    enum mpd_fnscroller_mode     mode;

    struct mpd_fnscroller_server server;
    struct mpd_fnscroller_client client;
};


static enum mpd_fnscroller_result
mpd_fnscroller_start(struct mpd_fnscroller_master *master, int argc,
                     char **argv);
static enum mpd_fnscroller_result
mpd_fnscroller_master_init(struct mpd_fnscroller_master *master, int argc,
                           char **argv);
static enum mpd_fnscroller_result
opt_string_parse(struct mpd_fnscroller_master *master, int argc, char **argv);


int main(int argc, char **argv)
{
    struct mpd_fnscroller_master mpd_fnscroller_master;

    openlog(PROGNAME, LOG_CONS | LOG_PID, LOG_DAEMON);

    if (!mpd_fnscroller_start(&mpd_fnscroller_master, argc, argv))
    {
        ERR_("Failed to start " PROGNAME)
        closelog();
        exit(EXIT_FAILURE);
    }

    closelog();
    exit(EXIT_SUCCESS);
};


static enum mpd_fnscroller_result
mpd_fnscroller_start(struct mpd_fnscroller_master *master, int argc,
                     char **argv)
{
    setlocale(LC_ALL, "");

    if ((runtime_paths_init() != RESULT_SUCCESS) ||
        (mpd_fnscroller_master_init(master, argc, argv) != RESULT_SUCCESS))
    {
        ERR_("Failed to initialize master structure or runtime paths")
        return RESULT_ERROR;
    }

    switch (master->mode)
    {
        case SERVER_MODE:
            return server_run(&master->server);

        case CLIENT_MODE:
            return client_run(&master->client);

        default:
            ERR_("Invalid mode")
            return RESULT_ERROR;
    }

    ERR_("Should not be there")
    return RESULT_ERROR;
};

static enum mpd_fnscroller_result
mpd_fnscroller_master_init(struct mpd_fnscroller_master *master, int argc,
                           char **argv)
{
    enum mpd_fnscroller_result result = RESULT_COUNT;

    master->mode = MODE_COUNT;
    result = server_init(&master->server) & client_init(&master->client) &
             opt_string_parse(master, argc, argv);

    return result;
};

static enum mpd_fnscroller_result
opt_string_parse(struct mpd_fnscroller_master *master, int argc, char **argv)
{
    struct mpd_fnscroller_server *server = &master->server;
    struct mpd_fnscroller_client *client = &master->client;
    int                          pidfile_fd = 0;
    int                          server_pid = 0;
    int                          opt = 0;
    ssize_t                      bytes_read = 0;
    char                         *invalid_numchar = NULL;
    char                         pid_str[PID_STRING_SIZE];

    memset(pid_str, '\0', PID_STRING_SIZE);

    while ((opt = getopt(argc, argv, "hds:nt:c:qv")) != -1)
    {
        switch (opt)
        {
            case 'h':
                printf(MPD_FNSCROLLER_HELP_STR);
                exit(EXIT_SUCCESS);

            case 'd':
                debug = true;
                syslog(LOG_NOTICE, "Debug is enabled");
                break;

            case 's':
                master->mode = SERVER_MODE;
                memset(server->mpd_host, '\0', HOSTNAME_STRING_SIZE);

                char *colon = strchr(optarg, ':');
                if (colon)
                {
                    unsigned int colon_pos = colon - optarg;

                    if ((colon_pos < HOSTNAME_STRING_SIZE - 2) &&
                        (colon_pos > 0))
                    {
                        strncpy(server->mpd_host, optarg, colon_pos);
                    }
                    else
                    {
                        ERR_("Wrong <host>:<port> argument")
                        return RESULT_ERROR;
                    }
                    server->mpd_port = strtol(colon + 1, &invalid_numchar, DEC);
                    if ((*invalid_numchar) || (!strlen(colon + 1)))
                    {
                        ERR_("Invalid port")
                        return RESULT_ERROR;
                    }
                }
                else
                {
                    if (strcmp(optarg, MPD_FNSCROLLER_DEFAULT_OPTARG) == 0)
                    {
                        if (!mpd_server_defaults_get(server->mpd_host,
                                                     &server->mpd_port))
                        {
                            ERR_("Unable to get default mpd host and port")
                            return RESULT_ERROR;
                        }
                    }
                    else
                    {
                        ERR_("Invalid -s optarg")
                        return RESULT_ERROR;
                    }
                }

                break;

            case 'n':
                daemonize_service = false;
                break;

            case 't':
                if (strcmp(optarg, MPD_FNSCROLLER_DEFAULT_OPTARG) == 0)
                {
                    server->mpd_timeout =  MPD_DEFAULT_TIMEOUT;
                }
                else
                {
                    server->mpd_timeout = strtol(optarg, &invalid_numchar, DEC);
                    if (*invalid_numchar)
                    {
                        ERR_("Invalid -t optarg")
                        return RESULT_ERROR;
                    }
                }

                break;

            case 'c':
                master->mode = CLIENT_MODE;

                if (strcmp(optarg, MPD_FNSCROLLER_DEFAULT_OPTARG) == 0)
                {
                    client->bufsize = DEFAULT_OUTPUT_STRING_SIZE;
                }
                else
                {
                    client->bufsize = strtol(optarg, &invalid_numchar, DEC) + 1;
                    if (*invalid_numchar)
                    {
                        ERR_("Invalid -c optarg")
                        return RESULT_ERROR;
                    }
                    if (client->bufsize > FILENAME_WCHAR_STRING_SIZE)
                    {
                        ERR_("Buffer size is too long")
                        return RESULT_ERROR;
                    }
                }

                break;

            case 'q':
                syslog(LOG_WARNING, "Sending normal shutdown signal to server");
                pidfile_fd = open(pidfile_path, O_RDONLY);
                if (pidfile_fd == -1)
                {
                    ERR_("Could not open pidfile: %s", pidfile_path)
                    return RESULT_ERROR;
                }
                bytes_read = read(pidfile_fd, pid_str, PID_STRING_SIZE);
                if (bytes_read == -1)
                {
                    ERR_("Could not read server PID")
                    close(pidfile_fd);
                    return RESULT_ERROR;
                }
                server_pid = strtol(pid_str, &invalid_numchar, DEC);
                if (*invalid_numchar)
                {
                    ERR_("Invalid pidfile data")
                    close(pidfile_fd);
                    return RESULT_ERROR;
                }
                if (kill(server_pid, SIGUSR1) == -1)
                {
                    ERR_("Could not send SIGUSR1 to server process")
                    close(pidfile_fd);
                    return RESULT_ERROR;
                }
                close(pidfile_fd);

                closelog();
                exit(EXIT_SUCCESS);

            case 'v':
                printf("%d.%d.%d\n", MPD_FNSCROLLER_VERSION_MAJOR,
                       MPD_FNSCROLLER_VERSION_MINOR, MPD_FNSCROLLER_VERSION_PATCH);
                closelog();
                exit(EXIT_SUCCESS);

            default:
                printf(MPD_FNSCROLLER_USAGE_STR);
                return RESULT_ERROR;
        }

        invalid_numchar = NULL;
    }

    return RESULT_SUCCESS;
};
