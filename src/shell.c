/* shell.c
 * Demonstration of multiplexing I/O with a background thread also printing.
 */

#include "../include/mq/thread.h"
#include "../include/mq/client.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <termios.h>
#include <unistd.h>

/* Constants */

#define BACKSPACE   127

/* Functions
 * https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html
 */

void toggle_raw_mode() {
    static struct termios OriginalTermios = {0};
    static bool enabled = false;

    if (enabled) {
    	tcsetattr(STDIN_FILENO, TCSAFLUSH, &OriginalTermios);
    } else {
	tcgetattr(STDIN_FILENO, &OriginalTermios);

	atexit(toggle_raw_mode);

	struct termios raw = OriginalTermios;
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    	enabled = true;
    }
}

// Prints a usage message to the user
void user_help() {
    printf("Welcome to the chat application! Here is how you use it:\n");
    printf("\nTo subscribe: subscribe <topic>");
    printf("\nTo unsubscrbe: unsubscribe <topic>");
    printf("\nTo publish: publish <message>");
    printf("\nTo set a new topic for publishing: new <topic>");
    printf("\nTo quit, type quit or exit");
    printf("\nYour default topic is home and your messages will publish to your last subscribed topic unless otherwise updated by new.");
    printf("\nTopics cannot have spaces in them.\n\n");

    return;
}

/* Threads */

void *background_thread(void *arg) {
    MessageQueue *mq = (MessageQueue *)arg;

    // While the message queue should not shutdown, retrieves and prints messages to user
    while (!mq_shutdown(mq)) {
        char *body = mq_retrieve(mq);
        if (body) {
    	    printf("\r%s\n", body);
            free(body);
        }
    }

    return NULL;
}

void *foreground_thread(void *arg) {
    MessageQueue *mq = (MessageQueue *)arg;
    char   input_buffer[BUFSIZ] = "";
    size_t input_index = 0;
    char   default_topic[] = "home";    // default topic
    char   topic[BUFSIZ];
    strcpy(topic, default_topic);

    // Subscribes to the default topic home
    mq_subscribe(mq, topic);

    // While the message queue should not shutdown, interacts with user
    while (!mq_shutdown(mq)) {
	char input_char = 0;
    	read(STDIN_FILENO, &input_char, 1);

    	if (input_char == '\n') {   // Process commands
            // If user says quit or exit, stop message queue
    	    if (strcmp(input_buffer, "quit") == 0 || strcmp(input_buffer, "exit") == 0) {
                puts("Stopping chat application.");
                mq_stop(mq);
	    } 

            // If user says subscribe, subscribe topic to message queue
            else if (strcmp(strtok(input_buffer, " "), "subscribe") == 0) { 
                strcpy(topic, strtok(NULL, " "));
               
                mq_subscribe(mq, topic);
                printf("%s has subscribed to topic %s\n", mq->name, topic);
            }
            
            // If user says unsubscribe, unsubscribe topic from message queue
            else if (strcmp(strtok(input_buffer, " "), "unsubscribe") == 0) {
                char *start = &input_buffer[12];
                char *end = &input_buffer[input_index];
                char *unsub_topic = (char *)calloc(1, end - start + 1);
                memcpy(unsub_topic, start, end - start); 

                mq_unsubscribe(mq, unsub_topic);
                printf("%s has unsubscribed from topic %s\n", mq->name, unsub_topic);

                // If the unsubscribed topic is the current topic, switch to default topic
                if (strcmp(unsub_topic, topic) == 0) {
                    strcpy(topic, default_topic);
                    printf("\t\t\t\t\t\t\t\t\t\t%s has switched to the topic %s\n", mq->name, topic);
                }

                free(unsub_topic);
            }

            // If user says publish, publish current topic to message queue
            else if (strcmp(strtok(input_buffer, " "), "publish") == 0) {
                char *start = &input_buffer[8];
                char *end = &input_buffer[input_index];
                char *message = (char *)calloc(1, end - start + 1);
                memcpy(message, start, end - start);
             
                mq_publish(mq, topic, message);
                printf("%s has published a message to topic %s\n", mq->name, topic);

                free(message);
            }

            // If user says new, change current topic
            else if (strcmp(strtok(input_buffer, " "), "new") == 0) {
                char *start = &input_buffer[4];
                char *end = &input_buffer[input_index];
                char *new_topic = (char *)calloc(1, end - start + 1);
                memcpy(new_topic, start, end - start); 
                strcpy(topic, new_topic);
                free(new_topic);       

                printf("%s has switched to the topic %s\n", mq->name, topic);
            }

            // If user input does not match above, remove line
            else {
		printf("\r%-80s\n", input_buffer);
	    }

    	    input_index = 0;
    	    input_buffer[0] = 0;
	} 

        // Removes last character is user enters backspace
        else if (input_char == BACKSPACE && input_index) {	// Backspace
    	    input_buffer[--input_index] = 0;
	} 

        // Adds new character to input
        else if (!iscntrl(input_char) && input_index < BUFSIZ) {
	    input_buffer[input_index++] = input_char;
	    input_buffer[input_index]   = 0;
	}

	printf("\r%-80s", "");			// Erase line (hack!)
	fflush(stdout);
    }

    return NULL;
}

/* Main Execution */

int main(int argc, char *argv[]) {
    // Default name, host, and port
    char *name = getenv("USER");
    char *host = "localhost";
    char *port = "9123";

    // Prints usage to user
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        printf("Usage: ./application [USERNAME] [HOST] [PORT]\n");
        return 0;
    }

    // User-specified name, host, and port
    if (argc > 1) { name = argv[1]; }
    if (argc > 2) { host = argv[2]; }
    if (argc > 3) { port = argv[3]; }

    // If no name, queue is shell
    if (!name)    { name = "shell";  }

    toggle_raw_mode();

    user_help();

    printf("\nYour messages:\t\t\t\t\t\t\t\t\tChat terminal:\n");
 
    // Creates and starts message queue 
    MessageQueue *mq = mq_create(name, host, port);
    mq_start(mq);

    // Background Thread 
    Thread background;
    thread_create(&background, NULL, background_thread, (void*)mq);

    // Foreground Thread
    Thread foreground;
    thread_create(&foreground, NULL, foreground_thread, (void*)mq);

    // Joins threads
    thread_join(foreground, NULL);
    thread_join(background, NULL);

    // Deletes message queue
    mq_delete(mq);

    return 0;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
