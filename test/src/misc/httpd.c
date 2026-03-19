/*****************************************************************************
 * httpd.c: test for vlc_httpd.h
 *****************************************************************************
 * Copyright (C) 2025 VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <vlc_httpd.h>

#include "../lib/libvlc_internal.h"
#include "./libvlc/test.h"

static void sigalrm_handler(int signum)
{
    (void)signum;
    fprintf(stderr, "Received SIGALRM! Ignoring...\n");
}

static int test_url_cb(vlc_httpd_callback_sys_t *sys,
                       vlc_httpd_message_t *answer,
                       const vlc_httpd_message_t *query)
{
    (void)sys;
    if (query->args != NULL)
        fprintf(
            stderr, "test_url_cb: Query parameters: %s\n", (char *)query->args);

    if (query->body != NULL)
        fprintf(stderr,
                "test_url_cb: Query body: %s\n",
                (char *)(query->body + query->body_offset));

    if (query->headers != NULL)
    {
        fprintf(stderr, "test_url_cb: Query headers\n");
        for (size_t i = 0; i < query->headerslen; i++)
        {
            fprintf(stderr,
                    "Name: %s, Value: %s\n",
                    query->headers[i].name,
                    query->headers[i].value);
        }
    }

    const char *response_content = "Hello World!";
    size_t response_content_size = strlen(response_content);

    answer->body = (uint8_t *)malloc(response_content_size + 1);
    if (answer->body == NULL)
        return VLC_ENOMEM;

    memcpy(answer->body, response_content, response_content_size);
    answer->body[response_content_size] = '\0';

    answer->bodylen = response_content_size;
    if (vlc_httpd_MsgAddHeader(answer, "Content-Type", "text/plain"))
    {
        free(answer->body);
        answer->body = NULL;
        return VLC_ENOMEM;
    }
    answer->status = 200;
    return VLC_SUCCESS;
}

static int test_file_cb(vlc_httpd_file_args *args)
{
    (void)args->sys;

    const char *html_content =
        "<html><head><title>File</title></head><body>File page</body></html>";

    size_t content_size = strlen(html_content);

    args->response->body = (uint8_t *)malloc(content_size + 1);
    if (args->response->body == NULL)
        return VLC_ENOMEM;

    memcpy(args->response->body, html_content, content_size);
    args->response->body[content_size] = '\0';
    args->response->bodylen = content_size;

    return VLC_SUCCESS;
}

/* Helper to run curl and capture HTTP status code */
static int check_curl_status(const char *cmd)
{
    FILE *fp = popen(cmd, "r");
    if (!fp)
    {
        perror("popen failed");
        return -1;
    }

    char buffer[16] = {0};
    if (fgets(buffer, sizeof(buffer), fp) == NULL)
    {
        pclose(fp);
        fprintf(stderr, "Failed to read HTTP status code\n");
        return -1;
    }

    pclose(fp);

    int status = atoi(buffer);
    fprintf(stderr, "HTTP status code: %d\n", status);
    return status;
}

int main(void)
{
    signal(SIGALRM, sigalrm_handler);
    test_init();

    const char *const args[] = {"-vvv"};

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(args), args);
    if (vlc == NULL)
    {
        fprintf(stderr, "Failed to create libvlc instance\n");
        return 1;
    }

    vlc_httpd_host_t *host = vlc_httpd_HostNew(&vlc->p_libvlc_int->obj);
    if (host == NULL)
    {
        fprintf(stderr, "Failed to create host\n");
        libvlc_release(vlc);
        return 1;
    }

    fprintf(stderr, "Host created and running\n");

    vlc_httpd_url_t *url = NULL;
    vlc_httpd_file_t *file = NULL;

    vlc_httpd_file_cbs file_cbs = {.fill = test_file_cb};
    vlc_httpd_credential_t crd = {.user = "testuser",
                                  .password = "testpassword"};

    url = vlc_httpd_UrlNew(host, "/test-url", &crd, false);
    if (url == NULL)
        goto cleanup;

    if (vlc_httpd_UrlCatch(url, VLC_HTTPD_MSG_POST, test_url_cb, NULL))
        goto cleanup;

    file = vlc_httpd_FileNew(
        host, "/file", VLC_HTTPD_MSG_GET, "text/html", &crd, &file_cbs, NULL);
    if (file == NULL)
        goto cleanup;

    /* Build and run GET request with Digest Auth */
    char get_cmd[512];
    snprintf(get_cmd,
             sizeof(get_cmd),
             "curl --digest -s -o /dev/null -w \"%%{http_code}\" "
             "-u %s:%s http://localhost:8080/file",
             crd.user,
             crd.password);

    fprintf(stderr, "\n== Performing GET /file request ==\n");
    int get_status = check_curl_status(get_cmd);
    if (get_status != 200)
    {
        fprintf(stderr, "GET /file failed. Status: %d\n", get_status);
        goto cleanup;
    }
    else
        fprintf(stderr, "GET /file succeeded\n");

    /* Build and run POST request with Digest Auth */
    char post_cmd[512];
    snprintf(post_cmd,
             sizeof(post_cmd),
             "curl --digest -s -o /dev/null -w \"%%{http_code}\" "
             "-u %s:%s -X POST 'http://localhost:8080/test-url?name=ayush' "
             "-H 'Content-Type: application/json' "
             "-d '{\"key\":\"value\"}'",
             crd.user,
             crd.password);

    fprintf(stderr, "\n== Performing POST /test-url request ==\n");
    int post_status = check_curl_status(post_cmd);
    if (post_status != 200)
    {
        fprintf(stderr, "POST /test-url failed! Status: %d\n", post_status);
        goto cleanup;
    }
    else
        fprintf(stderr, "POST /test-url succeeded\n");

    fprintf(stderr, "\nTests done. Cleaning up...\n");

    vlc_httpd_FileDelete(file);
    vlc_httpd_UrlDelete(url);
    vlc_httpd_HostDelete(host);
    libvlc_release(vlc);
    return 0;

cleanup:
    fprintf(stderr, "Error during HTTPD setup. Cleaning up...\n");
    if (host)
    {
        if (file)
            vlc_httpd_FileDelete(file);
        if (url)
            vlc_httpd_UrlDelete(url);
        vlc_httpd_HostDelete(host);
    }
    libvlc_release(vlc);
    return 1;
}
