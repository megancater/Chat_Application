/* client.c: Message Queue Client */

#include "mq/client.h"
#include "mq/logging.h"
#include "mq/socket.h"
#include "mq/string.h"

/* Internal Constants */

#define SENTINEL "SHUTDOWN"

/* Internal Prototypes */

void * mq_pusher(void *);
void * mq_puller(void *);

/* External Functions */

/**
 * Create Message Queue withs specified name, host, and port.
 * @param   name        Name of client's queue.
 * @param   host        Address of server.
 * @param   port        Port of server.
 * @return  Newly allocated Message Queue structure.
 */
MessageQueue * mq_create(const char *name, const char *host, const char *port) {
    // Allocates message queue
    MessageQueue *m = calloc(1, sizeof(MessageQueue));

    if (!m) {
        return NULL;
    }

    // If name, host, and port are within appropriate length, copy them to the queue
    if (strlen(name) < NI_MAXHOST) strncpy(m->name, name, strlen(name));
    if (strlen(host) < NI_MAXHOST) strncpy(m->host, host, strlen(host));
    if (strlen(port) < NI_MAXSERV) strncpy(m->port, port, strlen(port));
    
    // Creates incoming and outgoing queues 
    m->outgoing = queue_create();
    m->incoming = queue_create();

    m->shutdown = false;

    // Initializes lock
    mutex_init(&m->lock, NULL);     

    return m;
}

/**
 * Delete Message Queue structure (and internal resources).
 * @param   mq      Message Queue structure.
 */
void mq_delete(MessageQueue *mq) {
    if (mq) {
        // Deletes queues
        queue_delete(mq->outgoing);
        queue_delete(mq->incoming);

        // Frees message queue
        free(mq); 
    }
}

/**
 * Publish one message to topic (by placing new Request in outgoing queue).
 * @param   mq      Message Queue structure.
 * @param   topic   Topic to publish to.
 * @param   body    Message body to publish.
 */
void mq_publish(MessageQueue *mq, const char *topic, const char *body) {
    char *method = "PUT";
    char uri[BUFSIZ];

    // Creates message /topic/$TOPIC
    sprintf(uri, "/topic/%s", topic); 

    // Pushes new request to outgoing queue
    queue_push(mq->outgoing, request_create(method, uri, body));
}

/**
 * Retrieve one message (by taking Request from incoming queue).
 * @param   mq      Message Queue structure.
 * @return  Newly allocated message body (must be freed).
 */
char * mq_retrieve(MessageQueue *mq) {
    // Grabs message from incoming queue
    Request *r = queue_pop(mq->incoming);

    // Grabs the body of the request
    char *temp = strdup(r->body);
  
    request_delete(r);

    // If the message is the sentinel, return NULL
    if (strcmp(temp, SENTINEL) == 0) {
        free(temp);
        return NULL;
    } 

    return temp;
}

/**
 * Subscribe to specified topic.
 * @param   mq      Message Queue structure.
 * @param   topic   Topic string to subscribe to.
 **/
void mq_subscribe(MessageQueue *mq, const char *topic) {
    char *method = "PUT";
    char uri[BUFSIZ];

    // Creates message /subscription/$QUEUE/$TOPIC
    sprintf(uri, "/subscription/%s/%s", mq->name, topic);

    // Pushes new request to outgoing queue
    queue_push(mq->outgoing, request_create(method, uri, NULL));
}

/**
 * Unubscribe to specified topic.
 * @param   mq      Message Queue structure.
 * @param   topic   Topic string to unsubscribe from.
 **/
void mq_unsubscribe(MessageQueue *mq, const char *topic) {
    char *method = "DELETE";
    char uri[BUFSIZ];

    // Creates message /subscription/$QUEUE/$TOPIC
    sprintf(uri, "/subscription/%s/%s", mq->name, topic);

    // Pushes new request to outgoing queue
    queue_push(mq->outgoing, request_create(method, uri, NULL));
}

/**
 * Start running the background threads:
 *  1. First thread should continuously send requests from outgoing queue.
 *  2. Second thread should continuously receive reqeusts to incoming queue.
 * @param   mq      Message Queue structure.
 */
void mq_start(MessageQueue *mq) {
    // Subscribes sentinel to message queue
    mq_subscribe(mq, SENTINEL);

    // Starts running background threads
    thread_create(&mq->pusher, NULL, mq_pusher, (void*)mq);
    thread_create(&mq->puller, NULL, mq_puller, (void*)mq);
}

/**
 * Stop the message queue client by setting shutdown attribute and sending
 * sentinel messages
 * @param   mq      Message Queue structure.
 */
void mq_stop(MessageQueue *mq) {
    // Sets shutdown attribute
    mutex_lock(&mq->lock);
    mq->shutdown = true;
    mutex_unlock(&mq->lock);

    // Publishes sentinel to message queue
    mq_publish(mq, SENTINEL, SENTINEL);

    // Joins threads
    thread_join(mq->pusher, NULL);
    thread_join(mq->puller, NULL);
}

/**
 * Returns whether or not the message queue should be shutdown.
 * @param   mq      Message Queue structure.
 */
bool mq_shutdown(MessageQueue *mq) {
    mutex_lock(&mq->lock);
    bool shutdown = mq->shutdown;
    mutex_unlock(&mq->lock);
    return shutdown;
}

/* Internal Functions */

/**
 * Pusher thread takes messages from outgoing queue and sends them to server.
 **/
void * mq_pusher(void *arg) {
    MessageQueue *mq = (MessageQueue *)arg;
    
    while (!mq_shutdown(mq)) {
        // Connects to server
        FILE *fs = socket_connect(mq->host, mq->port);

        // If not connected, stop message queue
        if (!fs) {
            continue;
        }

        // Takes message from outgoing
        Request *r = queue_pop(mq->outgoing);

        if (!r) {
            fclose(fs);
            continue;
        }

        // Writes request and then frees it
        request_write(r, fs);
        request_delete(r);

        char buffer[BUFSIZ];

        // Reads the first line of response and exits if failed
        if (!fgets(buffer, BUFSIZ, fs)) {
            fclose(fs);
            continue;
        }

        // Checks if response succeeded
        char *status = strstr(buffer, "200 OK");

        fclose(fs);

        // Response failed
        if (!status) {
            continue;
        }
    }

    return NULL;
}

/**
 * Puller thread requests new messages from server and then puts them in
 * incoming queue.
 **/
void * mq_puller(void *arg) {
    MessageQueue *mq = (MessageQueue *)arg;

    while (!mq_shutdown(mq)) {
        // Connects to server
        FILE *fs = socket_connect(mq->host, mq->port);

        // If not connected, stop message queue
        if (!fs) {
            continue;
        }

        char *method = "GET";
        char uri[BUFSIZ];

        // Requests message from the server
        sprintf(uri, "/queue/%s", mq->name);
        Request *r = request_create(method, uri, NULL);
        request_write(r, fs); 

        char buffer[BUFSIZ];

        // Reads the first line of response and exits if failed
        if (!fgets(buffer, BUFSIZ, fs)) {
            fclose(fs);
            continue;
        }

        // Checks if response succeeded
        char *status = strstr(buffer, "200 OK");

        // Response failed
        if (!status) {
            fclose(fs);
            continue;
        }
    
        long unsigned int length = 0;

        // Gets length of body
        while (fgets(buffer, BUFSIZ, fs) && !streq(buffer, "\r\n")) {
            sscanf(buffer, "Content-Length: %lu\r\n", &length);
        }

        // Allocates body and reads it in
        r->body = calloc(1, length + 1);
        fread(r->body, 1, length, fs);

        // Pushes new message into incoming queue
        queue_push(mq->incoming, r);

        fclose(fs);
    }

    return NULL;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
