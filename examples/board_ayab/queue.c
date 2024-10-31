#include <stdio.h>
#include "sim_avr.h"
#include "queue.h"

int queue_push(event_queue_t *queue, enum event_type type, int value) {
    int index_write_next = (queue->index_write + 1) % ARRAY_SIZE(queue->items);

    if (index_write_next == queue->index_read) {
        fprintf(stderr, "WARNING: Event queue full !\n");
        return 0;
    }
    queue->items[queue->index_write].type=type;
    queue->items[queue->index_write].value=value;

    queue->index_write = index_write_next;
    return 1;
}

int queue_pop(event_queue_t *queue, enum event_type *type, int *value) {

    if (queue->index_read == queue->index_write) { //queue empty
        return 0;
    }
    *type = queue->items[queue->index_read].type;
    *value = queue->items[queue->index_read].value;

    queue->index_read = (queue->index_read + 1) % ARRAY_SIZE(queue->items);
    return 1;
}


