/**
 * \author Bert Van Cauter
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sbuffer.h"
// #include <poll.h>
// #include "config.h"
// #include "lib/tcpsock.h"
// #include <signal.h>
// #include "lib/dplist.h"

//#define PORT 5678 //niet meer nodig als we deze nemen van commandline.
//#define MAX_CONN 100  // state the max. number of connections the server will handle before exiting



void * element_copy(void *element);


void element_free(void **element);


int element_compare(void *X, void *Y);


void connmgr_listen(int server_port, sbuffer_t *buffer);


void connmgr_free();

void write_data_to_file(sensor_id_t id, sensor_value_t value, sensor_ts_t ts, FILE * fp);