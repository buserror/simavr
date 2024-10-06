
#ifndef __AYAB_MACHINE__
#define __AYAB_MACHINE__

#define HALL_VALUE_NORTH 700
#define HALL_VALUE_IDLE  400
#define HALL_VALUE_SOUTH 100

enum side {LEFT, RIGHT};
enum machine_type {KH910, KH930, KH270};
enum carriage_type {KNIT, LACE, GARTNER, KNIT270};
enum belt_phase_type {REGULAR, SHIFTED};

typedef struct {
    enum carriage_type type;
    int position;
} carriage_t;

typedef struct {
    enum machine_type type;
    enum side start_side;
    int num_needles;
    int num_solenoids;
    carriage_t carriage;
    int belt_phase;
    int hall_left, hall_right;
} machine_t;

#endif
