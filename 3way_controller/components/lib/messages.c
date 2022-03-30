#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "esp_log.h"
#include "lwip/sockets.h"
#include "libconfig.h"
#include "libdecls.h"

/*
 * Messages serve both direct responses (sent by the console) and logging & error messages.
 * Messages are "sent" by being broadcast on the broadcast port.  Messages which are errors
 * are also queued so they can be reviewed on demand.
 * 
 * All messages are limited to MESSAGE_LEN in length.
 * 
 * To enable messages to be sent from multiple tasks, we use a queuing model and a separate
 * task (defined in network.c) that actually does the sending/processing.
 * 
 * We implement our own queuing mechanism (rather than using FreeRTOS xQueue for example)
 * because we want our queues to overflow gracefully by losing older messages rather
 * than blocking.  (That is, our queues are round-robin.)
 * 
 * There are two queues implemented exactly the same way:  the message queue, and
 * the error queue.  (What varies is when and how they are filled and emptied.)
 */

static const char *TAG = "messages";

static int err_count = 0;
static int new_errors = 0;
static int report_requested = 0;
static int queues_init = 0;

// See comments in implementation section below
struct rrqueue {
    char q1[ MESSAGE_QUEUE_SIZE * MESSAGE_LEN ];
    char q2[ MESSAGE_QUEUE_SIZE * MESSAGE_LEN ];
    char *fill; // points to q1 or q2, whichever we are currently filling
    int i; // index of next slot to write in fill queue
    int has_wrapped;  // true if queue has already round-robined.
};

static struct rrqueue message_queue;
static struct rrqueue error_queue;

// forward decl
void init_rrqueue(struct rrqueue *q);
void enqueue_rrqueue(struct rrqueue *q, const char *message);
char **fetch_rrqueue(struct rrqueue *q);


/*
 * Accept messages for queuing.
 * Severity is 0=informational, 1=warning, 2=error.
 */

void send_message(int severity, const char *message) {
    if (queues_init == 0) {
        init_rrqueue(&message_queue);
        init_rrqueue(&error_queue);
        queues_init = 1;
    }
    enqueue_rrqueue( &message_queue, message );
    if (severity > 0) {
        enqueue_rrqueue( &error_queue, message );
        err_count++;
        new_errors++;
    }
}

void send_messagef(int severity,const char *fmt, ...) {
    char buf[MESSAGE_LEN];

    va_list args;
    va_start(args, fmt);
    vsnprintf( buf, MESSAGE_LEN, fmt, args );
    va_end(args);
    
    send_message(severity, buf);
}


/*
 * Error management
 */

int current_error_count() {
    return err_count;
}

void report_errors() {
    // All we do here is set the "report requested" variable.
    // Process_message_queue takes care of it next time it runs.
    report_requested = 1;
}


/*
 * Queue processing.  This is where the real work happens.
 */

int send_a_message(int sock, void *sa, char *m) {
    if ( sendto(sock, m, strlen(m), 0, (struct sockaddr *)sa, sizeof(struct sockaddr_in)) < 0 ) {
        LOGE(TAG, "Error occurred during broadcast: errno %d", errno);
        return errno;
    }
    return 0;
}

int process_message_queue(int sock, void *sa) {
    if (queues_init == 0) {
        init_rrqueue(&message_queue);
        init_rrqueue(&error_queue);
        queues_init = 1;
    }

    int didoverflow = message_queue.has_wrapped;
    char **mlist = fetch_rrqueue(&message_queue);
    char **mp;
    int err = 0;

    // First send any queued messages
    if (mlist) {
        if (didoverflow) {
            // This won't get picked up until next time, but we want to do it
            // this way to it will itself get added to the error queue.
            LOGW(TAG, "Message queue overflowed.  Some messages were lost.");
        }
        mp = mlist;
        // To avoid possibly snowballing things, we stop sending messages if
        // sending causes an error.
        while( *mp && err == 0 ) {
            err = send_a_message(sock, sa, *mp);
            mp++;
        }
        free(mlist);
    }

    // If we're also supposed to send errors, do that too.
    // We don't worry about overflow here --- since we're only doing occassional, user-requested, 
    // reports, we expect overflow to happen.
    if (report_requested && err == 0) {
        mlist = fetch_rrqueue(&error_queue);
        int nerrs = new_errors;
        new_errors = 0;
        report_requested = 0;
        if (mlist) {
            char msgbuf[64];
            sprintf(msgbuf, "Error Report. %d errors since last report", nerrs);
            err = send_a_message(sock, sa, msgbuf);
            mp = mlist;
            while( *mp && err == 0) {
                err = send_a_message(sock, sa, *mp);
                mp++;
            }
            send_a_message(sock, sa, "End Error Report");        
            free(mlist);
        }
        else {
            send_a_message(sock, sa, "No new Error Messages to report.");
        }
    }
    return err;
}


/* 
 * RRqueue internal implementation
 *
 * Implementation Notes:
 * 
 * Each queue is actually implemented as two queues.  This is so that that one is always
 * available to be inserted into, while the other might be being processed.
 * 
 * We don't worry about mutex issues regarding adding to the queues because this
 * is an RT system which won't switch tasks until they block, and all the queue
 * manipulation is non-blocking.
 * 
 * It is important, however, that there is only a single queue consumer, so the 
 * activities of processing a queue, and swapping between them, are also safe.
 * 
 * If a queue would overflow before it would be processed, we start 
 * filling it round-robin (newer messages overwrite older ones).  Messages can
 * get lost that way.   If that happens in practice, increase the queue size or
 * decrease the broadcast loop interval.
 * 
 * The round-robin handling is also why we copy messages into a fixed block
 * of messages rather than keeping pointers to messages --- this way we don't
 * have to handle de-allocation and/or potential leaks.
 *
 * I chose to do my own pointer arithmetic because I find it easier to read than
 * trying to do the correct casts on 2-D arrays.  Hence the single block
 * of characters for each queue.  YMMV.
 * 
 * Finally, there is no logical requirement for the error queue and the message queue to
 * be the same size; it just makes the code simpler.
 */

void init_rrqueue(struct rrqueue *q) {
    q->fill = q->q1;
    q->i = 0;
    q->has_wrapped = 0;
}

void enqueue_rrqueue(struct rrqueue *q, const char *message) {
        // Round robin if necessary
        if (q->i == MESSAGE_QUEUE_SIZE) {
            q->i = 0;
            q->has_wrapped = 1;
        }
        int len = strlen(message);
        if (len >= MESSAGE_LEN) {
            len = MESSAGE_LEN - 1;
        }
        
        char *mptr = q->fill + (q->i)*MESSAGE_LEN;
        memcpy( mptr, message, len );
        *(mptr + len) = 0; // null terminate string
        
        q->i++;
}

/* 
 * Does two things: returns a set of messages available to process, and internally swaps the
 * queues so that any new input goes to the other queue.  Will return NULL if there are no
 * items to process, otherwise a NULL-terminated array of pointers to the individual messages,
 * in proper order (accounting for round-robining)
 */
char **fetch_rrqueue(struct rrqueue *q) {
    if ( q->i == 0 && q->has_wrapped == 0 ) {
        return NULL;
    }

    // Make our own copy of state we need
    char *buffer = q->fill;
    int top = q->i;
    int has_wrapped = q->has_wrapped;

    // Swap the queues
    q->fill = (q->fill == q->q1 ? q->q2 : q->q1);
    q->i = 0;
    q->has_wrapped = 0;

    // build a list of pointers to return
    int count = (has_wrapped ? MESSAGE_QUEUE_SIZE : top);    
    char **result = malloc((count+1) * sizeof(char *));
    int bp, rp = 0;

    // Fill result array.
    if (has_wrapped) {
        for(bp=top; bp < MESSAGE_QUEUE_SIZE; bp++) {
            result[rp++] = buffer + (bp*MESSAGE_LEN);
        }
    }
    for(bp=0; bp<top; bp++) {
        result[rp++] = buffer + (bp*MESSAGE_LEN);
    }
    result[rp] = NULL;
    return result;
}

