//g++ rtsp_client.cpp -o rtsp_client -lcurl
//./rtsp_client rtsp://192.168.2.110/34567

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <thread>  // For std::this_thread::sleep_for
#include <chrono>  // For std::chrono::milliseconds, seconds, etc.

#include "rtsp_client.hpp"


// Helper function to send RTSP OPTIONS request
static CURLcode rtsp_options(CURL *curl, const char *uri) {
    CURLcode res = CURLE_OK;
    printf("\nRTSP: OPTIONS %s\n", uri);
    curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri);
    curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_OPTIONS);

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    curl_easy_setopt(curl, CURLOPT_USERNAME, "admin");
    curl_easy_setopt(curl, CURLOPT_PASSWORD, "PASSWORD");

    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST); // For Digest authentication
    // Or for Basic: curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    // Or for any supported methods: curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform(OPTIONS) failed: %d\n", res);
    }
    return res;
}

// Helper function to send RTSP DESCRIBE request and print SDP
static CURLcode rtsp_describe(CURL *curl, const char *uri)
{
    CURLcode res = CURLE_OK;
    printf("\nRTSP: DESCRIBE %s\n", uri);

    const char *sdp_filename = "cam00.sdp";
    FILE *sdp_fp = fopen(sdp_filename, "wb");
    printf("\nRTSP: DESCRIBE %s\n", uri);
    if(sdp_fp == NULL) {
        fprintf(stderr, "Could not open '%s' for writing\n", sdp_filename);
        sdp_fp = stdout;
    }
    else {
        printf("Writing SDP to '%s'\n", sdp_filename);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, sdp_fp);
    curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri);
    curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_DESCRIBE);
    res = curl_easy_perform(curl); // Output will be printed to stdout by default
    if (res != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform(DESCRIBE) failed: %d\n", res);
    }
    fclose(sdp_fp);
    return res;
}

// Helper function to send RTSP SETUP request
static CURLcode rtsp_setup(CURL *curl, const char *uri, const char *transport) {
    CURLcode res = CURLE_OK;
    printf("\nRTSP: SETUP %s\n", uri);
    printf(" TRANSPORT %s\n", transport);
    curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri);
    curl_easy_setopt(curl, CURLOPT_RTSP_TRANSPORT, transport);
    curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP);
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform(SETUP) failed: %d\n", res);
    }
    return res;
}

// Helper function to send RTSP PLAY request
static CURLcode rtsp_play(CURL *curl, const char *uri) {
    CURLcode res = CURLE_OK;
    printf("\nRTSP: PLAY %s\n", uri);
    curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri);
    curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY);
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform(PLAY) failed: %d\n", res);
    }
    return res;
}

// Helper function to send RTSP TEARDOWN request
static CURLcode rtsp_teardown(CURL *curl, const char *uri) {
    CURLcode res = CURLE_OK;
    printf("\nRTSP: TEARDOWN %s\n", uri);
    curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_TEARDOWN);
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform(TEARDOWN) failed: %d\n", res);
    }
    return res;
}

void rtsp_client(const std::string argv)
{
    const char *url = argv.c_str();
    const char *transport = "RTP/AVP;unicast;client_port=10001-10002"; // Example UDP transport

    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (!curl)
    {
        fprintf(stderr, "Error: Could not initialize libcurl\n");
        curl_global_cleanup();
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Enable verbose output for debugging

    do 
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        CURLcode res;
        // 1. Send OPTIONS request
        res = rtsp_options(curl, url);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "rtsp_options failed: %d\n", res);
            continue;
        }

        // 2. Send DESCRIBE request to get SDP
        res = rtsp_describe(curl, url);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "rtsp_describe failed: %d\n", res);
            continue;
        }

        // 3. Send SETUP request for a media track (e.g., video)
        // In a real client, you'd parse the SDP to get the media control URI.
        // For this example, we assume the base URL is sufficient for setup.
        res = rtsp_setup(curl, url, transport);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "rtsp_setup failed: %d\n", res);
            continue;
        }

        // 4. Send PLAY request
        res = rtsp_play(curl, url);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "rtsp_play failed: %d\n", res);
            continue;
        }


        // receive loop
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::seconds(10));//milliseconds(500)
            curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_GET_PARAMETER);
            //curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, CURL_RTSPREQ_RECEIVE);
            res = curl_easy_perform(curl);
            if(res != CURLE_OK)
            { // <== When the connection is broken, it still returns CURLE_OK!
                fprintf(stderr, "Error: call curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                break;
            }
        }

        // In a real application, you would now start receiving and processing RTP data.
        // For this example, we'll just wait for a moment.
        //printf("\nPlaying RTSP stream (simulated). Press Enter to stop...\n");
        //getchar();

        // 5. Send TEARDOWN request
        res = rtsp_teardown(curl, url);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "rtsp_teardown failed: %s\n", curl_easy_strerror(res));
            continue;
        }
    } while(false);

    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return;
}
