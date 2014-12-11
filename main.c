/*
 * Copyright (c) 2011, Jim Hollinger
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Jim Hollinger nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
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
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined (WIN32)
#  include <conio.h>  /* _getch() */
#else

#  include <termios.h>
#  include <unistd.h>

static int _getch(void) {
    struct termios oldt;
    struct termios newt;
    int ch;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

#endif

#include <curl/curl.h>
#include <stdbool.h>

#if 1
#define DEFAULT_TRANSPORT "RTP/AVP;unicast;client_port=1234-1235" /* UDP */
#else
#define DEFAULT_TRANSPORT "RTP/AVP/TCP;unicast;client_port=1234-1235"  /* TCP */
#endif

#define DEFAULT_RANGE "0.000-"

/* error handling macros */
#define my_curl_easy_setopt(A, B, C) \
  if ((ret = curl_easy_setopt((A), (B), (C))) != CURLE_OK) \
    fprintf(stderr, "curl_easy_setopt(%s, %s, %s) failed: %d\n", \
            #A, #B, #C, ret);

#define my_curl_easy_perform(A) \
  if ((ret = curl_easy_perform((A))) != CURLE_OK) \
    fprintf(stderr, "curl_easy_perform(%s) failed: %d\n", #A, ret);

/* send RTSP OPTIONS request */
static void rtsp_options(CURL *curl, const char *uri) {
    CURLcode ret = CURLE_OK;
    printf("\nRTSP: OPTIONS %s\n", uri);
    my_curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri);
    my_curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_OPTIONS);
    my_curl_easy_perform(curl);

    return;
}

/* send RTSP DESCRIBE request and write sdp response to a file */
static void rtsp_describe(CURL *curl, const char *uri,
        const char *sdp_filename) {
    CURLcode ret = CURLE_OK;
    FILE *sdp_fp = fopen(sdp_filename, "wt");

    printf("\nRTSP: DESCRIBE %s\n", uri);

    if (NULL == sdp_fp) {
        fprintf(stderr, "Could not open '%s' for writing\n", sdp_filename);
        sdp_fp = stdout;
    } else {
        printf("Writing SDP to '%s'\n", sdp_filename);
    }

    my_curl_easy_setopt(curl, CURLOPT_WRITEDATA, sdp_fp);
    my_curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_DESCRIBE);
    my_curl_easy_perform(curl);
    my_curl_easy_setopt(curl, CURLOPT_WRITEDATA, stdout);
    if (sdp_fp != stdout) {
        fclose(sdp_fp);
    }

    return;
}

/* send RTSP SETUP request */
static void rtsp_setup(CURL *curl, const char *uri, const char *transport) {
    CURLcode ret = CURLE_OK;

    printf("\nRTSP: SETUP %s\n", uri);
    printf("      TRANSPORT %s\n", transport);

    my_curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri);
    my_curl_easy_setopt(curl, CURLOPT_RTSP_TRANSPORT, transport);
    my_curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_SETUP);
    my_curl_easy_perform(curl);

    return;
}


/* send RTSP PLAY request */
static void rtsp_play(CURL *curl, const char *uri, const char *range) {
    CURLcode ret = CURLE_OK;

    printf("\nRTSP: PLAY %s\n", uri);

    my_curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri);
    if (range != NULL) {
        my_curl_easy_setopt(curl, CURLOPT_RANGE, range);
    }
    my_curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_PLAY);
    my_curl_easy_perform(curl);

    return;
}


/* send RTSP TEARDOWN request */
static void rtsp_teardown(CURL *curl, const char *uri) {
    CURLcode ret = CURLE_OK;

    printf("\nRTSP: TEARDOWN %s\n", uri);

    my_curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_TEARDOWN);
    my_curl_easy_perform(curl);

    return;
}


/* convert url into an sdp filename */
static void get_sdp_filename(const char *url, char *sdp_filename) {
    const char *s = strrchr(url, '/');
    strcpy(sdp_filename, "video.sdp");

    if (s != NULL) {
        ++s;
        if (s[0] != '\0') {
            sprintf(sdp_filename, "%s.sdp", s);
        }
    }

    return;
}


/* scan sdp file for media control attribute */
static void get_media_control_attribute(const char *sdp_filename,
        char *control) {
    int max_len = 256;
    char *s = malloc(max_len);
    FILE *sdp_fp = fopen(sdp_filename, "rt");

    control[0] = '\0';

    if (sdp_fp != NULL) {
        while (fgets(s, max_len - 2, sdp_fp) != NULL) {
            sscanf(s, " a = control: %s", control);
        }
        fclose(sdp_fp);
    }
    free(s);

    return;
}

static void print_usage(const char *base_name) {
    printf("Usage:   %s url [-t transport] [-r range]\n", base_name);
    printf("           [-norange] [-noteardown] [-nointeractive] [-v]\n");
    printf("         url of video server\n");
    printf("         transport (optional) specifier for media stream protocol\n");
    printf("           default transport: %s\n", DEFAULT_TRANSPORT);
    printf("         range (optional) specifier for playing media stream\n");
    printf("           default range: %s\n", DEFAULT_RANGE);
    printf("Example: %s rtsp://192.168.1.105/ch1/main\n\n", base_name);

    return;
}

/* main app */
int main(int argc, char *const argv[]) {

    const char *transport = DEFAULT_TRANSPORT;
    const char *range = DEFAULT_RANGE;
    bool teardown_flag = true;
    bool interactive_flag = true;
    long verbose = 0;
    int rc = EXIT_SUCCESS;
    char *base_name = NULL;
    CURLcode ret = CURLE_OK;
    int i = 0;

    /* check command line */
    base_name = strrchr(argv[0], '/');
    if (base_name == NULL) {
        // on windows
        base_name = strrchr(argv[0], '\\');
    }
    if (base_name == NULL) {
        base_name = argv[0];
    } else {
        ++base_name;
    }

    if (argc <= 1) {
        print_usage(base_name);
        rc = EXIT_FAILURE;
    } else {
        const char *url = argv[1];
        char *uri = malloc(strlen(url) + 32);
        char *sdp_filename = malloc(strlen(url) + 32);
        char *control = malloc(strlen(url) + 32);
        get_sdp_filename(url, sdp_filename);
        i = 2;
        --argc;
        while (argc > 1) {
            if (strcmp(argv[i], "-t") == 0) {
                --argc;
                ++i;
                if (argc > 1) {
                    transport = argv[i];
                    --argc;
                    ++i;
                }
                printf("Using transport: %s\n", transport);
            } else if (strcmp(argv[i], "-r") == 0) {
                --argc;
                ++i;
                if (argc > 1) {
                    range = argv[i];
                    --argc;
                    ++i;
                }
                printf("Using range: %s\n", range);
            } else if (strcmp(argv[i], "-norange") == 0) {
                --argc;
                ++i;
                range = NULL;
                printf("RTSP play range disabled\n");
            } else if (strcmp(argv[i], "-noteardown") == 0) {
                --argc;
                ++i;
                teardown_flag = false;
            } else if (strcmp(argv[i], "-nointeractive") == 0) {
                --argc;
                ++i;
                interactive_flag = false;
            } else if (strcmp(argv[i], "-v") == 0) {
                --argc;
                ++i;
                verbose = 1;
            } else if (strcmp(argv[i], "-h") == 0) {
                --argc;
                ++i;
                print_usage(base_name);
                rc = EXIT_FAILURE;
                break;
            } else {
                printf("Unrecognized option: %s\n\n", argv[i]);
                argc--;
                i++;
                print_usage(base_name);
                rc = EXIT_FAILURE;
                break;
            }
        }

        if (rc == EXIT_SUCCESS) {
            // initialize curl
            ret = curl_global_init(CURL_GLOBAL_ALL);
            if (ret == CURLE_OK) {
                curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
                CURL *curl = NULL;
                fprintf(stderr, "    cURL V%s loaded\n", data->version);

                /* initialize this curl session */
                curl = curl_easy_init();
                if (curl != NULL) {
                    my_curl_easy_setopt(curl, CURLOPT_VERBOSE, verbose);
                    my_curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
                    my_curl_easy_setopt(curl, CURLOPT_HEADERDATA, stdout);
                    my_curl_easy_setopt(curl, CURLOPT_URL, url);

                    /* request server options */
                    sprintf(uri, "%s", url);
                    rtsp_options(curl, uri);

                    /* request session description and write response to sdp file */
                    rtsp_describe(curl, uri, sdp_filename);

                    /* get media control attribute from sdp file */
                    get_media_control_attribute(sdp_filename, control);

                    /* setup media stream */
                    sprintf(uri, "%s/%s", url, control);
                    rtsp_setup(curl, uri, transport);

                    /* start playing media stream */
                    sprintf(uri, "%s/", url);
                    rtsp_play(curl, uri, range);
                    if (interactive_flag) {
                        printf("Playing video, press any key to stop ...");
                        _getch();
                        printf("\n");
                    } else {
                        printf("Playing video\n");
                    }

                    /* teardown session */
                    if (teardown_flag) {
                        rtsp_teardown(curl, uri);
                    }

                    /* cleanup */
                    curl_easy_cleanup(curl);
                    curl = NULL;
                } else {
                    fprintf(stderr, "curl_easy_init() failed\n");
                }
                curl_global_cleanup();
            } else {
                fprintf(stderr, "curl_global_init(%s) failed: %d\n",
                        "CURL_GLOBAL_ALL", ret);
            }

        }

        free(control);
        free(sdp_filename);
        free(uri);
    }

    return rc;
}
