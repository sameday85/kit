#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <curl/curl.h>

typedef struct tagContentBlock {
    char *content;
    size_t size;
    struct tagContentBlock *nextBlock;
} ContentBlock;

//
//https://curl.haxx.se/libcurl/c/example.html
//
static size_t write_string(void *ptr_in, size_t size, size_t nmemb, void *stream) {
    size_t received = size * nmemb;
    ContentBlock *ptr = (ContentBlock*)stream;
    while (ptr->content) {
        ptr = ptr->nextBlock;
    }
    ptr->content = malloc (received);
    if (ptr->content) {
        ptr->size = received;
        memcpy (ptr->content, ptr_in, received);
    }    
    //prepare the next block
    ptr->nextBlock = malloc (sizeof(ContentBlock));
    if (ptr->nextBlock)
        memset (ptr->nextBlock, 0, sizeof (ContentBlock));
  
    return received;
}

//Download a given URL into a buffer
char *do_get(char *url) {
    char *content = NULL;
    
    ContentBlock *header = malloc(sizeof(ContentBlock));
    memset (header, 0, sizeof (ContentBlock));
    
    curl_global_init(CURL_GLOBAL_ALL);
    CURL *curl_handle = curl_easy_init();

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Mozilla/5.0 (iPad; CPU OS 6_0_1 like Mac OS X) AppleWebKit/536.26 (KHTML, like Gecko) Mobile/10A523");
    curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
    
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_string);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, header);

    CURLcode code = curl_easy_perform(curl_handle);
    if (CURLE_OK == code) {
        /* check the size */
        size_t content_length = 0;
        //The content-length field in the response headers may be zero
        if (header) {
            ContentBlock *ptr = header;
            while (ptr->size > 0) {
                content_length += ptr->size;
                ptr = ptr->nextBlock;
            }
        }        
        if (content_length > 0) {
            content = malloc (content_length + 12);
            char *dst = content;
            ContentBlock *ptr = header;
            while (ptr->size > 0) {
                memcpy (dst, ptr->content, ptr->size);
                dst += ptr->size;
                ptr = ptr->nextBlock;
            }
            content[content_length] = '\0';
        }
        //long http_code = 0;
        //curl_easy_getinfo (curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
    }

    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    
    while (header) {
        ContentBlock *ptr = header;
        header = header->nextBlock;
        if (ptr->content)
            free (ptr->content);
        free (ptr);
    }
    
    return content;
}

