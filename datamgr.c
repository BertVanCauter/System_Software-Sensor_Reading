/**
 * \author Bert Van Cauter
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "./lib/dplist.h"
#include "config.h"	
#include <assert.h>
#include "datamgr.h"
#include "sbuffer.h"
//#include <stdbool.h>

#ifndef RUN_AVG_LENGTH
#define RUN_AVG_LENGTH 5
#endif

#ifndef SET_MAX_TEMP
#error SET_MAX_TEMP not set
#endif

#ifndef SET_MIN_TEMP
#error SET_MIN_TEMP not set
#endif

#define FILES_NOT_FOUND 1
#define SENSOR_ID_NOT_FOUND 2

#ifdef DEBUG
#define DEBUG_PRINTF(...) 									                                        \
        do {											                                            \
            fprintf(stderr,"\nIn %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	    \
            fprintf(stderr,__VA_ARGS__);								                            \
            fflush(stderr);                                                                         \
                } while(0)
#else
#define DEBUG_PRINTF(...) (void)0
#endif

#define ERROR_HANDLER(condition, err_code)                         \
    do {                                                                \
            if ((condition)) DEBUG_PRINTF(#condition " failed\n");      \
            assert(!(condition));                                       \
        } while(0)

typedef struct {
	sensor_id_t sensorid;
	room_id_t roomid;
	sensor_value_t runningavg[RUN_AVG_LENGTH];
	sensor_ts_t timestamp; 
} element_t;


bool datamgr_is_sensor_in_list(element_t *element);

static void * element_copy(void *element)
{
	return element; 
}

static void element_free(void **element)
{
	free(*element);
	*element = NULL; 
}

static int element_compare(void *X, void *Y)
{
	return 0;
}

static dplist_t *list; // global variables because alle the functions in this file will use the list, but static because ONLY this file will need it. 
void datamgr_parse_sensor_files(FILE *fp_sensor_map, sbuffer_t *buffer)
{
	ERROR_HANDLER(fp_sensor_map==NULL, FILES_NOT_FOUND);
	//ERROR_HANDLER(fp_sensor_data==NULL, FILES_NOT_FOUND);
	if(fp_sensor_map==NULL)
	{
		printf("Files are not opened correctly\n");
	}
	else
	{
		list = dpl_create(element_copy, element_free, element_compare);
		while(!feof(fp_sensor_map))
		{
			element_t *element= malloc(sizeof(element_t));
			sensor_id_t sensorID;
			room_id_t roomID; 
			fscanf(fp_sensor_map, "%hu",&roomID);
			fscanf(fp_sensor_map, "%hu",&sensorID);
			element->roomid = (uint16_t)roomID;
			element->sensorid = (uint16_t)sensorID; 
			for(int i=0; i<RUN_AVG_LENGTH; i++)
			{	
				sensor_value_t initial = 0; 
				(element->runningavg)[i]= initial;
			}
			element->timestamp=time(NULL);
			if(!datamgr_is_sensor_in_list(element))
			{
				list = dpl_insert_at_index(list, element,0, false);
			}
			else
			{
				free(element);
			}
		}

		sensor_data_t *data = malloc(sizeof(sensor_data_t)); // outside of while loop, so that you don't need to malloc every time.
		bool flag = true;
		while(flag==true)
		{
			int i = sbuffer_remove(buffer, data, 1);
			if(i == SBUFFER_SUCCESS)
			{	
				sensor_id_t sensorID = data->id;
				sensor_value_t temp = data->value;
				sensor_ts_t ts = data->ts;
				printf("value read by datamanager:\n");
				printf("sensorID = %d, sensorValue = %f, sensorTs = %ld\n", data->id, data->value, data->ts);
				fflush(stdout);
				int list_size = dpl_size(list);
				bool inListFlag = false; 
				for(int i=0; i<list_size; i++)
				{
					element_t *dummy_element = (element_t*)dpl_get_element_at_index(list,i);
					if(dummy_element->sensorid == sensorID)
					{
						for(int index = 0; index<RUN_AVG_LENGTH-1; index++)
						{
							dummy_element->runningavg[index] = dummy_element->runningavg[index+1];
						}
						dummy_element->runningavg[RUN_AVG_LENGTH-1]=temp; 
						dummy_element->timestamp = ts;
						inListFlag = true;

						sensor_value_t avg = datamgr_get_avg(dummy_element->sensorid);
						if(avg < SET_MIN_TEMP && avg != 0)
						{ 	/////HERE THE LOG MESSAGE WILL BE SEND -> temp too low message!!!!!///////
							int log = TOO_COLD;
						    sensor_id_t dummyid = dummy_element->sensorid;
						    sensor_value_t dummyvalue = avg;
						    sensor_ts_t dummyts = dummy_element->timestamp;
						    int fd = open("logFIFO",O_WRONLY);
						    write(fd,&dummyid,sizeof(sensor_id_t));
						    write(fd,&dummyvalue , sizeof(sensor_value_t));
						    write(fd,&dummyts , sizeof(sensor_ts_t));
						    write(fd,&log, sizeof(int));
						    close(fd);
							//////////////////////////////////////////////////////////////////////////
							fprintf(stderr, "Avg temp: %g is too low in room %d @%s",avg,dummy_element->roomid,asctime(gmtime(&(dummy_element->timestamp))));
						}
						else if(avg > SET_MAX_TEMP && avg != 0)
						{ 	/////HERE THE LOG MESSAGE WILL BE SEND -> temp too high message!!!!!!//////
							int log = TOO_WARM;
						    sensor_id_t dummyid = dummy_element->sensorid;
						    sensor_value_t dummyvalue = avg;
						    sensor_ts_t dummyts = dummy_element->timestamp;
						    int fd = open("logFIFO",O_WRONLY);
						    write(fd,&dummyid,sizeof(sensor_id_t));
						    write(fd,&dummyvalue , sizeof(sensor_value_t));
						    write(fd,&dummyts , sizeof(sensor_ts_t));
						    write(fd,&log, sizeof(int));
						    close(fd);
							//////////////////////////////////////////////////////////////////////////
							fprintf(stderr, "Avg temp: %g is too high in room %d @%s",avg,dummy_element->roomid,asctime(gmtime(&(dummy_element->timestamp))));
						}
					}
				}
				if(inListFlag==false)
				{ 	///////////HERE THE LOG MESSAGE WILL BE SEND -> sensor node not in list!!!!!////////////
					int log = INVALID_SENSOR;
				    sensor_id_t dummyid = sensorID;
				    sensor_value_t dummyvalue = 0;
				    sensor_ts_t dummyts = ts;
				    int fd = open("logFIFO",O_WRONLY);
				    write(fd,&dummyid,sizeof(sensor_id_t));
				    write(fd,&dummyvalue , sizeof(sensor_value_t));
				    write(fd,&dummyts , sizeof(sensor_ts_t));
				    write(fd,&log, sizeof(int));
				    close(fd);
                    //////////////////////////////////////////////////////////////////////////////////////
					fprintf(stderr,"There is no sensor with ID = %d, so this data is not added to the list\n",sensorID);
				}
			}
			else if(i == SBUFFER_NO_DATA)
			{
				printf("no data datamgr! \n");
				if(get_buffer_stop(buffer)==true)
				{	
					printf("no data datamgr and flag is set to false! \n");
					flag = false;
				}
			}
			else if(i == SBUFFER_FAILURE)
			{
				flag = false;
			}
		}
		free(data);// outside of while loop, so that you don't need to malloc every time.
	}
}

void datamgr_free()
{
	dpl_free(&list, true);
}

uint16_t datamgr_get_room_id(sensor_id_t sensor_id)
{

	int listsize = dpl_size(list);
	int i = 0;
	while(i<listsize)
	{
		element_t *dummy_element = (element_t*)dpl_get_element_at_index(list,i);
		if(dummy_element->sensorid == sensor_id)
		{
			return dummy_element->roomid;
		}
		else
		{
			i = i+1; 
		}
	}
	ERROR_HANDLER(true,SENSOR_ID_NOT_FOUND);
	return 0; //use ERROR handler to handle the not finding of sensorID 
}

sensor_value_t datamgr_get_avg(sensor_id_t sensor_id)
{
	int listsize = dpl_size(list);
	int i = 0;
	while(i<listsize)
	{
		element_t *dummy_element = (element_t*)dpl_get_element_at_index(list,i);
		if(dummy_element->sensorid == sensor_id)
		{
			if(dummy_element->runningavg[0]==0)
			{
				return 0; 
			}
			else
			{
				double totalTemp = 0; 
				for(int index = 0; index < RUN_AVG_LENGTH; index++)
				{
					totalTemp = totalTemp + dummy_element->runningavg[index];
				}
				double avg;
				avg = totalTemp/RUN_AVG_LENGTH;
				return avg;
			}	 
		}
		else
		{
			i = i+1; 
		}
	}
	ERROR_HANDLER(1,SENSOR_ID_NOT_FOUND);
	return 0; //use ERROR handler to handle the not finding of sensorID
}

time_t datamgr_get_last_modified(sensor_id_t sensor_id)
{
	int listsize = dpl_size(list);
	int i = 0;
	while(i<listsize)
	{
		element_t *dummy_element = (element_t*)dpl_get_element_at_index(list,i);
		if(dummy_element->sensorid == sensor_id)
		{
			return dummy_element->timestamp;
		}
		else
		{
			i = i+1; 
		}
	}
	ERROR_HANDLER(1,SENSOR_ID_NOT_FOUND);
	return time(NULL);
}

int datamgr_get_total_sensors()
{
	return dpl_size(list);
}

bool datamgr_is_sensor_in_list(element_t *element)
{
	int listsize = dpl_size(list);
	int i = 0;
	while(i<listsize)
	{
		element_t *dummy_element = (element_t*)dpl_get_element_at_index(list,i);
		if(dummy_element->sensorid == element->sensorid)
		{
			return true;
		}
		else
		{
			i = i+1; 
		}
	}
	return false;
}