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
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdio.h>

#include "mpd-fnscroller.h"
#include "client.h"




extern bool debug;
extern char pidfile_path[];
extern char sockfile_path[];


enum mpd_fnscroller_result client_init(struct mpd_fnscroller_client *client)
{
    client->server_sockaddr.sun_family = AF_UNIX;
    client->sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client->sock == -1)
    {
        ERR_("Could not create socket")
        return RESULT_ERROR;
    }

    client->buffer = NULL;
    client->bufsize = DEFAULT_OUTPUT_STRING_SIZE;

    return RESULT_SUCCESS;
};


enum mpd_fnscroller_result client_run(struct mpd_fnscroller_client *client)
{
    ssize_t send_recv_bytes;

    TRACE_()

    client->buffer = (wchar_t *)malloc(sizeof(wchar_t) * client->bufsize);

    strcpy(client->server_sockaddr.sun_path, sockfile_path);
    if (connect(client->sock, (struct sockaddr *)&client->server_sockaddr,
                sizeof(client->server_sockaddr)) == -1)
    {
        ERR_("Issue connecting with server")
        return RESULT_ERROR;
    }

    send_recv_bytes = send(client->sock, &client->bufsize, sizeof(unsigned int),
                           0);
    if (send_recv_bytes == -1)
    {
        ERR_("Could not send buffer size to server")
        close(client->sock);
        free(client->buffer);
        return RESULT_ERROR;
    }
    send_recv_bytes = recv(client->sock, client->buffer, sizeof(wchar_t) *
                           client->bufsize, 0);
    if (send_recv_bytes == -1)
    {
        ERR_("Could not receive buffer from server")
        close(client->sock);
        free(client->buffer);
        return RESULT_ERROR;
    }

    printf("%ls", client->buffer);

    close(client->sock);
    free(client->buffer);
    return RESULT_SUCCESS;
};
