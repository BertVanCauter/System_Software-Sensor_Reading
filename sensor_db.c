#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include <sqlite3.h>
#include <string.h>
#include <time.h>
#include "sensor_db.h"
#include <stdbool.h>

#define REAL_TO_STRING(s) #s
#define TO_STRING(s) REAL_TO_STRING(s)    //force macro-expansion on s before stringify s

#ifndef DB_NAME
#define DB_NAME Sensor.db
#endif

#ifndef TABLE_NAME
#define TABLE_NAME SensorData
#endif

#define DBCONN sqlite3

typedef int (*callback_t)(void *, int, char **, char **);


DBCONN *init_connection(char clear_up_flag)
{
	DBCONN * db;	
	sqlite3_stmt *res;
	int rc = sqlite3_open(TO_STRING(DB_NAME), &db);
	if(rc != SQLITE_OK)
	{
		fprintf(stderr,"Failed:Cannot open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return NULL; 
	}
	else
	{	
		/////////HERE LOG MESSAGE->connection established////////////
		int log = SQL_CONNECTION_OK;
	    sensor_id_t dummyid = 0;
	    sensor_value_t dummyvalue = 0;
	    sensor_ts_t dummyts = 0;
	    int fd = open("logFIFO",O_WRONLY);
	    write(fd,&dummyid,sizeof(sensor_id_t));
	    write(fd,&dummyvalue , sizeof(sensor_value_t));
	    write(fd,&dummyts , sizeof(sensor_ts_t));
	    write(fd,&log, sizeof(int));
	    close(fd);
        /////////////////////////////////////////////////////////////
 		if(!clear_up_flag)
 		{
 			char *query = NULL;
 			asprintf(&query, "CREATE TABLE IF NOT EXISTS %s (id INTEGER PRIMARY KEY AUTOINCREMENT,sensor_id INT,sensor_value DECIMAL(4,2),timestamp TIMESTAMP)", TO_STRING(TABLE_NAME));
 			rc = sqlite3_prepare_v2(db,query, -1, &res,0);
 			if(rc!=SQLITE_OK)
 			{
 				fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(db));
 				sqlite3_close(db);
 				free(query);
 				return NULL;
 			}
 			else
 			{
 				rc = sqlite3_step(res);
 				free(query);
 			}
 			sqlite3_finalize(res);
 		}
 		else
 		{
	 		char *query = NULL;
	 		char *query2 = NULL; 
	 		asprintf(&query, "DROP TABLE IF EXISTS %s;", TO_STRING(TABLE_NAME));
	 		asprintf(&query2, "CREATE TABLE %s (id INTEGER PRIMARY KEY AUTOINCREMENT,sensor_id INT,sensor_value DECIMAL(4,2),timestamp TIMESTAMP);", TO_STRING(TABLE_NAME));
	 		rc = sqlite3_prepare_v2(db,query, -1, &res,0);
	 		if(rc!=SQLITE_OK)
	 		{
	 			fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(db));
	 			sqlite3_close(db);
	 			free(query);
	 			free(query2);
	 			return NULL;
	 		}
	 		else
	 		{
	 			rc = sqlite3_step(res); 
	 		}
	 		sqlite3_finalize(res);
	 		rc = sqlite3_prepare_v2(db,query2, -1, &res,0);
	 		if(rc!=SQLITE_OK)
	 		{
	 			fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(db));
	 			sqlite3_close(db);
	 			free(query);
	 			free(query2);
	 			return NULL;
	 		}
	 		else
	 		{
	 			rc = sqlite3_step(res); 
	 		}
	 		sqlite3_finalize(res);
	 		free(query);
	 		free(query2);
	 	}
	 	///////HERE LOG MESSAGE WITH TABLE NAME -> new table generated/////////////
        log = TABLE_CREATED;
        fd = open("logFIFO",O_WRONLY);
        write(fd,&dummyid,sizeof(sensor_id_t));
        write(fd,&dummyvalue , sizeof(sensor_value_t));
        write(fd,&dummyts , sizeof(sensor_ts_t));
        write(fd,&log, sizeof(int));
        close(fd);
        /////////////////////////////////////////////////////////////////////////////
	 	return db; 
	}
}

void disconnect(DBCONN *conn)
{
	sqlite3_close(conn);
	////////////////////HERE LOG MESSAGE->connection lost//////////////////
	int log = SQL_CONNECTION_LOST;
	sensor_id_t dummyid = 0;
	sensor_value_t dummyvalue = 0;
	sensor_ts_t dummyts = 0;
	int fd = open("logFIFO",O_WRONLY);
	write(fd,&dummyid,sizeof(sensor_id_t));
	write(fd,&dummyvalue , sizeof(sensor_value_t));
	write(fd,&dummyts , sizeof(sensor_ts_t));
	write(fd,&log, sizeof(int));
	close(fd);
    ///////////////////////////////////////////////////////////////////////
}

int insert_sensor(DBCONN *conn, sensor_id_t id, sensor_value_t value, sensor_ts_t ts)
{
	sqlite3_stmt *res;
	char *query = NULL;
	char sensorID[4];
	sprintf(sensorID, "%d", id);
	char sensorVALUE[100];
	snprintf(sensorVALUE,100, "%f",value);
	//char sensorTS[100]; //This commentent code is if you want to put the time in readable format in the database.
	//strftime(sensorTS, 100, "%Y-%m-%d %H:%M:%S", localtime(&ts));
	asprintf(&query, "INSERT INTO %s(sensor_id,sensor_value,timestamp) VALUES (%s,%s,%ld);", TO_STRING(TABLE_NAME), sensorID, sensorVALUE,ts /*sensorTS*/);
	int rc = sqlite3_prepare_v2(conn, query, -1, &res, 0); 
	if(rc != SQLITE_OK)
	{
		fprintf((stderr), "Failed to insert data %s\n", sqlite3_errmsg(conn));
		free(query);
		return -1;
	}
	else
	{
		rc = sqlite3_step(res);
		sqlite3_finalize(res);
		free(query);
		return 0;
	}
}

int insert_sensor_from_file(DBCONN *conn, FILE *sensor_data)
{
	if(sensor_data==NULL)
	{
		fprintf(stderr,"Files are not opened correctly\n");
		return -1;
	}
	else
	{
		sqlite3_stmt *res;
		while(!feof(sensor_data))
		{
			uint16_t id;
			double value; 
			time_t ts; 
			fread(&id, sizeof(uint16_t), 1, sensor_data);
			fread(&value, sizeof(double), 1, sensor_data);
			fread(&ts, sizeof(time_t), 1, sensor_data);
			char *query = NULL;
			char sensorID[4];
			sprintf(sensorID, "%d", id);
			char sensorVALUE[100];
			snprintf(sensorVALUE,100, "%f",value);
			// char sensorTS[100]; //This commentent code is if you want to put the time in readable format in the database.
			// strftime(sensorTS, 100, "%Y-%m-%d %H:%M:%S", localtime(&ts));
			asprintf(&query, "INSERT INTO %s(sensor_id,sensor_value,timestamp) VALUES (%s,%s,%ld);", TO_STRING(TABLE_NAME), sensorID, sensorVALUE, ts/*sensorTS*/);
			int rc = sqlite3_prepare_v2(conn, query, -1, &res, 0); 
			if(rc != SQLITE_OK)
			{
				fprintf((stderr), "Failed to insert data %s\n", sqlite3_errmsg(conn));
				free(query);
				return -1;
			}
			else
			{
				rc = sqlite3_step(res);
				sqlite3_finalize(res);
				free(query);
			}
		}
		return 0;
	}
}

int find_sensor_all(DBCONN *conn, callback_t f)
{
	char *query = NULL;
	char *err_msg = NULL;
	asprintf(&query, "SELECT * FROM %s;", TO_STRING(TABLE_NAME));
	int rc = sqlite3_exec(conn, query, f, 0, &err_msg);
	if(rc != SQLITE_OK)
	{
		fprintf((stderr), "Failed retrieve data: %s\n", sqlite3_errmsg(conn));
		sqlite3_free(err_msg);
		free(query);
		return -1;
	}
	else
	{
		sqlite3_free(err_msg);
		free(query);
		return 0;
	}
}

int find_sensor_by_value(DBCONN *conn, sensor_value_t value, callback_t f)
{	
	char *query = NULL;
	char *err_msg = NULL;
	char sensorVALUE[100];
	snprintf(sensorVALUE,100, "%f",value);
	asprintf(&query, "SELECT * FROM %s WHERE sensor_value = %s;", TO_STRING(TABLE_NAME), sensorVALUE);
	int rc = sqlite3_exec(conn, query, f, 0, &err_msg);
	if(rc != SQLITE_OK)
	{
		sqlite3_free(err_msg);
		fprintf((stderr), "Failed retrieve data: %s\n", sqlite3_errmsg(conn));
		free(query);
		return -1;
	}
	else
	{
		sqlite3_free(err_msg);
		free(query);
		return 0;
	}
}

int find_sensor_exceed_value(DBCONN *conn, sensor_value_t value, callback_t f)
{
	char *query = NULL;
	char *err_msg = NULL;
	char sensorVALUE[100];
	snprintf(sensorVALUE,100, "%f",value);
	asprintf(&query, "SELECT * FROM %s WHERE sensor_value > %s;", TO_STRING(TABLE_NAME), sensorVALUE);
	int rc = sqlite3_exec(conn, query, f, 0, &err_msg);
	if(rc != SQLITE_OK)
	{
		sqlite3_free(err_msg);
		fprintf((stderr), "Failed retrieve data: %s\n", sqlite3_errmsg(conn));
		free(query);
		return -1;
	}
	else
	{
		sqlite3_free(err_msg);
		free(query);
		return 0;
	}
}	

int find_sensor_by_timestamp(DBCONN *conn, sensor_ts_t ts, callback_t f)
{
	char *query = NULL;
	char *err_msg = NULL;
	// char sensorTS[100];
	// strftime(sensorTS, 100, "%Y-%m-%d %H:%M:%S", localtime(&ts));
	asprintf(&query, "SELECT * FROM %s WHERE timestamp = %ld;", TO_STRING(TABLE_NAME), ts/*sensorTS*/);
	int rc = sqlite3_exec(conn, query, f, 0, &err_msg);
	if(rc != SQLITE_OK)
	{
		sqlite3_free(err_msg);
		fprintf((stderr), "Failed retrieve data: %s\n", sqlite3_errmsg(conn));
		free(query);
		return -1;
	}
	else
	{
		sqlite3_free(err_msg);
		free(query);
		return 0;
	}
}

int find_sensor_after_timestamp(DBCONN *conn, sensor_ts_t ts, callback_t f)
{
	char *query = NULL;
	char *err_msg = NULL;
	// char sensorTS[100];
	// strftime(sensorTS, 100, "%Y-%m-%d %H:%M:%S", localtime(&ts));
	asprintf(&query, "SELECT * FROM %s WHERE timestamp > %ld;", TO_STRING(TABLE_NAME), ts/*sensorTS*/);
	int rc = sqlite3_exec(conn, query, f, 0, &err_msg);
	if(rc != SQLITE_OK)
	{
		sqlite3_free(err_msg);
		fprintf((stderr), "Failed retrieve data: %s\n", sqlite3_errmsg(conn));
		free(query);
		return -1;
	}
	else
	{
		sqlite3_free(err_msg);
		free(query);
		return 0;
	}
}





