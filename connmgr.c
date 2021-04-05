/**
 * \author Bert Van Cauter
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <poll.h>
#include "config.h"
#include "lib/tcpsock.h"
#include <signal.h>
#include "lib/dplist.h"
#include "sbuffer.h"
#include <stdbool.h>

//#define PORT 5678 //niet meer nodig als we deze nemen van commandline.
//#define MAX_CONN 100  // state the max. number of connections the server will handle before exiting

#ifndef TIMEOUT
#error No Timeout value set 
#endif

void write_data_to_file(sensor_id_t id, sensor_value_t value, sensor_ts_t ts, FILE * fp);

typedef struct{
    sensor_data_t data;
    tcpsock_t *client;
}list_element;

static void * element_copy(void *element) // I made these static, because other wise the different threads can see this function
{                                         // and by making them statis the same functions, can have different content. 
    return element; 
}

static void element_free(void **element) 
{
    list_element * dummy = (list_element*) *element;
    tcpsock_t * s = dummy->client;
    tcp_close(&s);
    free(s);
    free(*element);
    *element = NULL; 
}

static int element_compare(void *X, void *Y)
{
    return 0; /// never used function so redundant! 
}

void connmgr_listen(int server_port, sbuffer_t *buffer)
{

	dplist_t *list; 
	list = dpl_create(element_copy, element_free, element_compare);
    tcpsock_t *server; //hier doen we plus één omdat we op index 0 de socket descriptor van de server meegegeven. 
    sensor_data_t data;                     //client[] gaat dus een lijst van socket descriptors van de clients met als eerste element de socket descripter van de server. 
    int check=0;
    char timeout_flag = 1; 
    
    printf("Test server is started\n");
    if (tcp_passive_open(&server, server_port) != TCP_NO_ERROR) exit(EXIT_FAILURE);//start server, provide pointer where you can implement the socket
    int conn_counter = 0;
    struct pollfd *poll_fd;
    poll_fd = malloc(sizeof(struct pollfd));
    int a = 0;
    list_element * server_element = malloc(sizeof(*server_element));
    if(tcp_get_sd(server,&a) != TCP_NO_ERROR) exit(EXIT_FAILURE);
    poll_fd[0].fd = a; 
    poll_fd[0].events = POLLIN;
    server_element->data.id = 0;
    server_element->data.value = 0;
    server_element->data.ts = time(NULL);
    server_element->client = server;
    list = dpl_insert_at_index(list, server_element, 0, false);
    do 
    {
        conn_counter = dpl_size(list);
        check = poll(poll_fd,(conn_counter), TIMEOUT*1000);
        if(check>0)
        {
            for(int index = 0; index<(conn_counter); index++)
            {
                if(poll_fd[index].revents & POLLIN)
                {
                    int bytes, result;
                    if(index==0)
                    {
                        tcpsock_t *new_client;
                        if(tcp_wait_for_connection(server,&new_client) != TCP_NO_ERROR) exit(EXIT_FAILURE);
                        // read sensorID
                        bytes = sizeof(data.id);
                        result = tcp_receive(new_client, (void *) &data.id, &bytes);
                        // read temperature
                        bytes = sizeof(data.value);
                        result = tcp_receive(new_client, (void *) &data.value, &bytes);
                        // read timestamp
                        bytes = sizeof(data.ts);
                        result = tcp_receive(new_client, (void *) &data.ts, &bytes);
                        if ((result == TCP_NO_ERROR) && bytes) 
                        {
                            //write_data_to_file(data.id, data.value, data.ts, fp_bin);
                            printf("event on server with new node\n");
                            //printf("sensor id = %" PRIu16 " - temperature = %g - timestamp = %ld\n", data.id, data.value, (long int) data.ts);
                            fflush(stdout);
                            list_element *new_element = malloc(sizeof(list_element));
                            new_element->data.id = data.id;
                            new_element->data.value = data.value;
                            new_element->data.ts = data.ts;
                            new_element->client = new_client;
                            ////////////////////////////////////////////////////////////////////////////////
                            sbuffer_insert(buffer, &data);
                            ///////////////////////LOG EVENT HERE->NEW CONNECTION///////////////////////////
                            int log = NEW_CONNECTION;
                            int fd = open("logFIFO",O_WRONLY);
                            write(fd,&data.id,sizeof(sensor_id_t));
                            write(fd,&data.value , sizeof(sensor_value_t));
                            write(fd,&data.ts , sizeof(sensor_ts_t));
                            write(fd,&log, sizeof(int));
                            close(fd);
                            ////////////////////////////////////////////////////////////////////////////////
                            list = dpl_insert_at_index(list,new_element,(dpl_size(list)+1),false);
                            poll_fd = realloc(poll_fd, dpl_size(list)*sizeof(struct pollfd));
                            if(poll_fd == NULL) free(poll_fd); // this if failure happens! 
                            for(int i=0;i<dpl_size(list);i++)
                            {
                                list_element *dummy_element =(list_element*)dpl_get_element_at_index(list,i);
                                tcpsock_t * dummyclient = dummy_element->client; 
                                int b = 0;
                                if(tcp_get_sd(dummyclient,&b) != TCP_NO_ERROR) exit(EXIT_FAILURE);
                                poll_fd[i].fd = b;
                                poll_fd[i].events = POLLIN;
                            }
                            poll_fd[0].revents = 0;
                        }
                        else if(result != TCP_NO_ERROR)
                        {
                            printf("Error occured on connection to peer\n");
                            fflush(stdout);
                        }
                    }
                    else
                    {
                        list_element *existing_element = (list_element*)dpl_get_element_at_index(list,index);
                        tcpsock_t *existing_client = existing_element->client;
                        bytes = sizeof(data.id);
                        result = tcp_receive(existing_client, (void *) &data.id, &bytes);
                         // read temperature
                        bytes = sizeof(data.value);
                        result = tcp_receive(existing_client, (void *) &data.value, &bytes);
                        // read timestamp
                        bytes = sizeof(data.ts);
                        result = tcp_receive(existing_client, (void *) &data.ts, &bytes);
                        if ((result == TCP_NO_ERROR) && bytes) 
                        {
                            //write_data_to_file(data.id, data.value, data.ts, fp_bin);
                            printf("event on server with existing node\n");
                            //printf("sensor id = %" PRIu16 " - temperature = %g - timestamp = %ld\n", data.id, data.value, (long int) data.ts);
                            fflush(stdout);
                            existing_element->data.id = data.id;
                            existing_element->data.value = data.value;
                            existing_element->data.ts = data.ts;
                            /////////////////////////////////////////////////////////////
                            sbuffer_insert(buffer, &data);
                            /////////////////////////////////////////////////////////////
                        }
                        else if (result == TCP_CONNECTION_CLOSED)
                        {
                            ///////////HERE LOG MESSAGE->CLOSED CONNECTION///////////////
                            int log = CLOSED_CONNECTION;
                            int fd = open("logFIFO",O_WRONLY);
                            write(fd,&data.id,sizeof(sensor_id_t));
                            write(fd,&data.value , sizeof(sensor_value_t));
                            write(fd,&data.ts , sizeof(sensor_ts_t));
                            write(fd,&log, sizeof(int));
                            close(fd);
                            /////////////////////////////////////////////////////////////
                            printf("Peer has closed connection\n");
                            list = dpl_remove_at_index(list, index, true);
                            poll_fd = realloc(poll_fd, dpl_size(list)*sizeof(struct pollfd));
                            if(poll_fd == NULL) free(poll_fd);
                            for(int i=0;i<dpl_size(list);i++)
                            {
                                list_element *dummy_element =(list_element*)dpl_get_element_at_index(list,i);
                                tcpsock_t * dummyclient = dummy_element->client; 
                                int b = 0;
                                if(tcp_get_sd(dummyclient,&b) != TCP_NO_ERROR) exit(EXIT_FAILURE);
                                poll_fd[i].fd = b;
                                poll_fd[i].events = POLLIN;
                            }
                        }
                        else if(result != TCP_NO_ERROR)
                        {
                            printf("Error occured on connection to peer\n");
                        }
                    }
                }
            }
        }
        else
        {
            printf("Server TIMEOUT\n");
            timeout_flag = 0;
            free(poll_fd);
            fflush(stdout);
            break;
        }   
    } while (timeout_flag);
    printf("Server is shutting down\n");
    set_buffer_stop(buffer, true);
    dpl_free(&list, true);
}

void connmgr_free()
{

}

void write_data_to_file(sensor_id_t id, sensor_value_t value, sensor_ts_t ts, FILE * fp)
{
    fwrite(&id, sizeof(sensor_id_t), 1, fp);
    fwrite(&value, sizeof(sensor_value_t), 1, fp);
    fwrite(&ts, sizeof(sensor_ts_t), 1, fp);
}
