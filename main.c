/*
Author: Bert Van Cauter, r0703645
*/	
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/wait.h>
#include "config.h"
#include "datamgr.h"
#include "sensor_db.h"
#include "sbuffer.h"
#include "connmgr.h"
#include <sqlite3.h>
#include <poll.h>
#include <stdbool.h>


int print_log_message(fifo_info_t *fifo_info, FILE *log_fdn, int seq_num); //protype of log printing function

typedef struct{ //This type of struct will contain data that will be passed to
	sbuffer_t *buffer;
	int port_number;
}thread_info_t;

void* tcpconnection_thread(void* value)
{
	thread_info_t *info = (thread_info_t*)value; 
	connmgr_listen(info->port_number, info->buffer);
	pthread_exit(NULL);
}

void* datamgr_thread(void* value)
{	
	thread_info_t *info = (thread_info_t*)value;
	FILE *room_sensor = fopen("./room_sensor.map","r");
	datamgr_parse_sensor_files(room_sensor, info->buffer);
	datamgr_free();
	fclose(room_sensor);	
	pthread_exit(NULL);
}

void* storagemgr_thread(void* value)
{
	///////////////////////ESTABLISHING CONNECTION/////////////////////
	int attempts = 1; 
	DBCONN * database_con = NULL; //Here we will make connection to database! 
	while(database_con == NULL && attempts <= 3)
	{
		database_con = init_connection(1);
		if(database_con == NULL) //If first connection failes, wait 5 seconds and try again.
		{
			sleep(5);
			attempts++;
		}
	}
	if(attempts > 3) //When you reach this point, and you have more than three attempts, the gateway must close. 
	{
	/////////HERE LOG MESSAGE-> unable to connect to database//////////
		fifo_info_t fifo_info;
	    fifo_info.id = 0;
	    fifo_info.value = 0;
	    fifo_info.ts = time(NULL);
	    fifo_info.log_event = SQL_CONNECTION_FAILED;
	    int fd = open("logFIFO",O_WRONLY);
	    write(fd, &fifo_info, sizeof(fifo_info_t));
	    close(fd);
		exit(EXIT_FAILURE);
	}
	//////////////READING IN DATA FROM BUFFER////////////////
	thread_info_t *info = (thread_info_t*)value;
	sbuffer_t *buffer = info->buffer;
	sensor_data_t *storagemgrdata = malloc(sizeof(*storagemgrdata)); //use size of data instead of type, so when type of variable is changed, you will still have the correct size.
	bool flag = true;
	while(flag == true)
	{
		printf("storagemanger stuck in while loop!\n");
		fflush(stdout);
		int i = sbuffer_remove(buffer, storagemgrdata, 2);
		if(i == SBUFFER_SUCCESS)
		{	
			int check = insert_sensor(database_con, storagemgrdata->id, storagemgrdata->value, storagemgrdata->ts);
			if(check == 0)
			{
				printf("value read by storagemanager:\n");
				printf("sensorID = %d, sensorValue = %f, sensorTs = %ld\n", storagemgrdata->id, storagemgrdata->value, storagemgrdata->ts);
				fflush(stdout);
			}
		}
		else if(i == SBUFFER_NO_DATA)
		{
			printf("no data storagemgr1! \n");
			fflush(stdout);
			if(get_buffer_stop(buffer) == true)
			{
				printf("no data storagemgr2! \n");
				fflush(stdout);
				break;

			}
		}
		else if(i == SBUFFER_FAILURE)
		{
			flag = false;
		}
	}	
	free(storagemgrdata);
	disconnect(database_con);
	pthread_exit(NULL);
}


int main(int argc, char *argv[]) //the port number will be expected when running the program, so use arg...
{ 
	//////////////////////ACCEPTING INPUT FROM USER//////////////////////// 
	int server_port;
    if(argc != 2)	// check if two argument are passed one running the program. 
    {
        printf("Did not receive correct port number!\n");
        printf("type: ./main xxxx\n");
        printf("Replace xxxx with number between 1024 and 65536\n");
        fflush(stdout);
        exit(EXIT_FAILURE);	 // exit with an appropriate exit message.
    }
    else
    {
        server_port = atoi(argv[1]);
    }
    ////////////////////MAKING THE FIFO IF NOT EXIST//////////////////////
    if(mkfifo("logFIFO", 0777)==-1)
    {
    	if(errno != EEXIST) // If the fifo already exist, great do nothing! 
    	{
    		printf("Error occurd on FIFO\n");
    		fflush(stdout);
    		return -1;
    	}
	}
	/////////////////////////MAKING TWO PROCESSES/////////////////////////
	int processID = fork();
	if(processID == -1)
	{
		printf("An error accord while making new process\n");
		fflush(stdout);
		return -1; // error occurd
	}
	
	if(processID == 0)	///////CHILD PROCESS/////// 
	{
		///////////////////READING OF THE FIFO FILE///////////////////////////
		struct pollfd *poll_fifo = malloc(sizeof(struct pollfd));
  		printf("gateway.log is opened\n"); 
  		int sequence_number = 0;
		int fd;
		bool flag = true;
		fifo_info_t *fifo_info = malloc(sizeof(fifo_info_t));
		while(flag == true)
		{	
			fflush(stdout);
			fd = open("logFIFO",O_RDONLY);
			if(fd == -1)
			{
				exit(EXIT_FAILURE); 
			}
			poll_fifo[0].fd = fd;
			poll_fifo[0].events = POLLIN;
			int check = poll(poll_fifo, 1, 5000);
			if(check>0)
			{
				printf("child process received activity on file descriptor!\n");
				fflush(stdout);
				if(poll_fifo[0].revents & POLLIN)
				{
					printf("child process received POLLIN!\n");
					fflush(stdout);
					FILE *log_fd = fopen("gateway.log","a+");
					if(log_fd==NULL) exit(EXIT_FAILURE);// error check!
					printf("logFIFO is opened\n");
					read(fd,&fifo_info->id, sizeof(sensor_id_t));
					read(fd,&fifo_info->value, sizeof(sensor_value_t));
					read(fd,&fifo_info->ts, sizeof(sensor_ts_t));
					read(fd,&fifo_info->log_event, sizeof(int));
					if(fifo_info->log_event==-1)
					{
						printf("reached log -1!\n");
						fflush(stdout);
						fclose(log_fd);
						close(fd);
						flag = false;
					}
					else
					{
						if(print_log_message(fifo_info,log_fd,sequence_number)==-1) exit(EXIT_FAILURE);
						printf("logFIFO is closed!\n");
						fclose(log_fd);
						sequence_number++;
						close(fd);
					}
				}	
			}
			else
			{
				printf("child process will close!\n");
				fflush(stdout);
				flag = false;
			}
		} 
		printf("FIFO ENDED!\n");
		fflush(stdout);
		free(poll_fifo);
		free(fifo_info);
	}
	
	else	///////MAIN PROCESS/////// 
	{
		///////////////////////////OPENING DIFFERENT THREADS/////////////////////////////
		sbuffer_t *buffer;
		pthread_t tcpconnect;
		pthread_t datamgr;
		pthread_t storagemgr;
		sbuffer_init(&buffer);
		thread_info_t *info = malloc(sizeof(*info));
		info->buffer = buffer;
		info->port_number = server_port; 
		pthread_create(&tcpconnect, NULL, tcpconnection_thread, info);
		pthread_create(&datamgr, NULL, datamgr_thread, info);
		pthread_create(&storagemgr, NULL, storagemgr_thread, info);
		/////////////BLOCK UNTIL ALL PTHREADS ARE DONE///////////////////////////////////
		pthread_join(tcpconnect, NULL);
		printf("connectionmgr thread is closed well!\n");
		fflush(stdout);
		
		pthread_join(datamgr, NULL);
		printf("datamgr thread is closed well!\n");
		fflush(stdout);
		
		set_buffer_stop(buffer,true);
		pthread_join(storagemgr, NULL);
		printf("storagemgr thread is closed well!\n");
		fflush(stdout);

		pthread_detach(tcpconnect);
		pthread_detach(datamgr);
		pthread_detach(storagemgr);
		/////////////SEND CLOSING MESSAGE TO FIFO TO CLOSE CHILD PROCESS////////////////
		sleep(1); // This will make shure that the child process has time enough to close properly
		int log = -1;
	    sensor_id_t dummyid = 0;
	    sensor_value_t dummyvalue = 0;
	    sensor_ts_t dummyts = time(NULL);
	    int fd = open("logFIFO",O_WRONLY);
	    write(fd,&dummyid,sizeof(sensor_id_t));
	    write(fd,&dummyvalue , sizeof(sensor_value_t));
	    write(fd,&dummyts , sizeof(sensor_ts_t));
	    write(fd,&log, sizeof(int));
	    close(fd);

		printf("destroy message is send to child process!\n");
		fflush(stdout);

		free(info);
		sbuffer_free(&buffer);
		//////////////WAIT UNTIL CHILD PROCESS IS CLOSED CORRECTLY/////////////////////
		wait(&processID);

		printf("everything went well!\n");
		fflush(stdout);
	}	
}
///////////////////////////////HANDELING THE LOG MESSAGES////////////////////////////
int print_log_message(fifo_info_t *fifo_info, FILE *log_fd, int seq_num)
{
	char sensorTS[100];
	strftime(sensorTS, 100, "%Y-%m-%d %H:%M:%S", localtime(&(fifo_info->ts)));
	if(fifo_info->log_event == NEW_CONNECTION)
	{
		if(fprintf(log_fd, "#LOGMESSAGE %d @%s A sensor node with %d has opened a new connection\n", seq_num, sensorTS, fifo_info->id)<0) return -1; 
		return 0;
	}
	else if(fifo_info->log_event == CLOSED_CONNECTION)
	{
		if(fprintf(log_fd, "#LOGMESSAGE %d @%s The sensor node with %d has closed the connection\n", seq_num, sensorTS,fifo_info->id)<0) return -1; 
		return 0;
	}
	else if(fifo_info->log_event == TOO_COLD)
	{
		if(fprintf(log_fd, "#LOGMESSAGE %d @%s The sensor node with %d reports it's too cold %g\n", seq_num, sensorTS, fifo_info->id, fifo_info->value)<0) return -1; 
		return 0;
	}
	else if(fifo_info->log_event == TOO_WARM)
	{
		if(fprintf(log_fd, "#LOGMESSAGE %d @%s The sensor node with %d reports it's too warm %g\n", seq_num, sensorTS, fifo_info->id, fifo_info->value)<0) return -1; 
		return 0;
	}
	else if(fifo_info->log_event == INVALID_SENSOR)
	{
		if(fprintf(log_fd, "#LOGMESSAGE %d @%s Received sensor data with invalid sensor node ID %d\n", seq_num, sensorTS, fifo_info->id)<0) return -1; 
		return 0;
	}
	else if(fifo_info->log_event == SQL_CONNECTION_OK)
	{
		if(fprintf(log_fd, "#LOGMESSAGE %d @%s Connection to SQL server established\n", seq_num, sensorTS)<0) return -1; 
		return 0;
	}
	else if(fifo_info->log_event == TABLE_CREATED)
	{
		if(fprintf(log_fd, "#LOGMESSAGE %d @%s New table SensorData created\n", seq_num, sensorTS)<0) return -1; 
		return 0;
	}	
	else if(fifo_info->log_event == SQL_CONNECTION_LOST)
	{
		if(fprintf(log_fd, "#LOGMESSAGE %d @%s Connection to SQL server lost\n", seq_num, sensorTS)<0) return -1; 
		return 0;
	}
	else if(fifo_info->log_event == SQL_CONNECTION_FAILED)
	{
		if(fprintf(log_fd, "#LOGMESSAGE %d @%s Unable to connect to SQL server\n", seq_num, sensorTS)<0) return -1; 
		return 0;
	}
 return -1;
}
