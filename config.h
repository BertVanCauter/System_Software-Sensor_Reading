/**
 * \author Bert Van Cauter
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdint.h>
#include <time.h>
//////USED FOR WORKING WITH FIFO/////////////
#include <errno.h> 
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define NEW_CONNECTION 1
#define CLOSED_CONNECTION 2
#define TOO_COLD 3
#define TOO_WARM 4
#define INVALID_SENSOR 5
#define SQL_CONNECTION_OK 6
#define TABLE_CREATED 7
#define SQL_CONNECTION_LOST 8
#define SQL_CONNECTION_FAILED 9

typedef uint16_t sensor_id_t;
typedef double sensor_value_t;
typedef time_t sensor_ts_t;         // UTC timestamp as returned by time() - notice that the size of time_t is different on 32/64 bit machine
typedef uint16_t room_id_t;

typedef struct {
    sensor_id_t id;
    sensor_value_t value;
    sensor_ts_t ts;
} sensor_data_t;

typedef struct{
	sensor_id_t id;
	sensor_value_t value;
	sensor_ts_t ts;
	int log_event;
} fifo_info_t;

#endif /* _CONFIG_H_ */
