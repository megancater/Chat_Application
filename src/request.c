/* request.c: Request structure */

#include "mq/request.h"

#include <stdlib.h>
#include <string.h>

/**
 * Create Request structure.
 * @param   method      Request method string.
 * @param   uri         Request uri string.
 * @param   body        Request body string.
 * @return  Newly allocated Request structure.
 */
Request * request_create(const char *method, const char *uri, const char *body) {
    // Allocates request
    Request *r = calloc(1, sizeof(Request));

    // Checks if request was allocated correctly
    if (!r) {
        return NULL;
    }
    
    // Copies method, uri, and body to request
    if (method) {
        r->method = strdup(method);
    }
        
    if (uri) {
        r->uri = strdup(uri);
    }
    
    if (body) {
        r->body = strdup(body);
    }

    return r;
}

/**
 * Delete Request structure.
 * @param   r           Request structure.
 */
void request_delete(Request *r) {
    // Frees method, uri, body, and request struct
    if (r) {
        free(r->method);
        free(r->uri);
        free(r->body);

        free(r);
    }

    return;
}

/**
 * Write HTTP Request to stream:
 *  
 *  $METHOD $URI HTTP/1.0\r\n
 *  Content-Length: Length($BODY)\r\n
 *  \r\n
 *  $BODY
 *      
 * @param   r           Request structure.
 * @param   fs          Socket file stream.
 */
void request_write(Request *r, FILE *fs) {
    // Checks if request and file stream exist
    if (!fs || !r) {
        return;
    }

    // Writes HTTP request to stream
    if (r->body) { // Publishes
        fprintf(fs, "%s %s HTTP/1.0\r\nContent-Length: %zu\r\n\r\n%s", r->method, r->uri, strlen(r->body), r->body);
    }
    else { // No body specified
        fprintf(fs, "%s %s HTTP/1.0\r\n\r\n", r->method, r->uri);
    }

    return;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */ 
