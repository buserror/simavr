#ifndef __AYAB_QUEUE_H
#define __AYAB_QUEUE_H

// Event queue from graphic to avr threads
enum event_type {
    CARRIAGE_LEFT,
    CARRIAGE_RIGHT,
};

typedef struct {
    int index_read;  // updated by read thread
    int index_write; // updated by graphic thread
    struct {
        enum event_type  type;
        int         value;
    } items[8];
} event_queue_t;

extern event_queue_t event_queue;

int queue_push(event_queue_t *queue, enum event_type type, int value);
int queue_pop(event_queue_t *queue, enum event_type *type, int *value);

#endif
