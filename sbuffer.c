/**
 * \author Bert Van Cauter
 */

#include <stdlib.h>
#include <stdio.h>
#include "sbuffer.h"
#include <pthread.h>
#include <stdbool.h>
#include <errno.h> 
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
//#include <semaphore.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //by making it statis this main can only be seen in this file, and not by the others file running on the program
static pthread_cond_t data_available = PTHREAD_COND_INITIALIZER;
//static pthread_cond_t data_available_for_one = PTHREAD_COND_INITIALIZER; 	
/**
 * basic node for the buffer, these nodes are linked together to create the buffer
 */
typedef struct sbuffer_node {
    struct sbuffer_node *next;  /**< a pointer to the next node*/
    sensor_data_t data;         /**< a structure containing the data */
    int threadID1;	/**will say by wich thread it is already read, -1 if not read by any thread*/ 
} sbuffer_node_t;
	
/**
 * a structure to keep track of the buffer
 */
struct sbuffer {
    sbuffer_node_t *head;       /**< a pointer to the first node in the buffer */
    sbuffer_node_t *tail;       /**< a pointer to the last node in the buffer */
    bool server_stop;
}; 

int sbuffer_init(sbuffer_t **buffer) { //one thread may execute this function at a time. 
    pthread_mutex_lock(&mutex);
    *buffer = malloc(sizeof(sbuffer_t));
    if (*buffer == NULL)
    {
        pthread_mutex_unlock(&mutex);
        return SBUFFER_FAILURE;
    } 
    (*buffer)->head = NULL;
    (*buffer)->tail = NULL;
    (*buffer)->server_stop = false;
    pthread_mutex_unlock(&mutex);
    return SBUFFER_SUCCESS;
}

int sbuffer_free(sbuffer_t **buffer) { //one thread execute this function at a time. 
    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&data_available);
    if ((buffer == NULL) || (*buffer == NULL)) 
    {
        return SBUFFER_FAILURE;
    }
    while ((*buffer)->head) 
    {
         sbuffer_node_t *dummy = (*buffer)->head;
        (*buffer)->head = (*buffer)->head->next;
        free(dummy);
    }
    free(*buffer);
    *buffer = NULL;
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    return SBUFFER_SUCCESS;
}

int sbuffer_remove(sbuffer_t *buffer, sensor_data_t *data, int pthread_id) { //this function needs to be called by two threads before really deleting the item withing the buffer.
    sbuffer_node_t *dummy;
    
    // struct timespec timeToWait;
    // struct timeval now;
    // gettimeofday(&now  ,NULL);
    // timeToWait.tv_sec = now.tv_sec+5;
    // timeToWait.tv_nsec = (now.tv_usec+1000UL*5000)*1000UL;

    pthread_mutex_lock(&mutex);
    if (buffer == NULL)
    {  
        pthread_mutex_unlock(&mutex);
    	return SBUFFER_FAILURE; // pointer to buffer is NULL
    }	
    else if (buffer->head == NULL)
    {
        printf("reached 1: id = %d\n", pthread_id);
        fflush(stdout);
        if(buffer->server_stop == false)
        {
            printf("reached 2: id = %d\n", pthread_id);
            fflush(stdout);
            pthread_cond_wait(&data_available, &mutex);
            printf("reached 3: id = %d\n", pthread_id);
            fflush(stdout);
        }
        pthread_mutex_unlock(&mutex);
    	return SBUFFER_NO_DATA; //pointer to first element is NULL, so list is empty! 
	}
	if(buffer->head->threadID1 != 0) // in this case there went one thread over the first node. 
	{// This means that one thread is already passed and now we need to look if the second thread is a new one.  
        
        if(buffer->head->threadID1 == pthread_id) //the thread is again called upon the first node. 
        {
            dummy = buffer->head; 
            while(dummy->threadID1 != 0) //while there is a node this is not yet read by one thread AND the next element cant be NULL! 
            {
                if(dummy->next != NULL)
                {
                    dummy = dummy->next;
                }
                else
                {
                    break;
                }
            }
            if(dummy->threadID1 == 0) // there is no data left to read for this thread! 
            {
                *data = dummy->data;
                dummy->threadID1 = pthread_id;
                pthread_mutex_unlock(&mutex);
                return SBUFFER_SUCCESS;
            } 
            else if(dummy->next == NULL) // There is no data left to read for this thread. 
            {
                if(get_buffer_stop(buffer)==false)
                {
                    pthread_cond_wait(&data_available, &mutex);
                }
                printf("reached 4: id = %d\n", pthread_id);
                fflush(stdout);
                pthread_mutex_unlock(&mutex);
                return SBUFFER_NO_DATA;    
            }        
        }
        else //second thread passes the first node so it can be deleted! 
        {
            *data = buffer->head->data;
            dummy = buffer->head;
            if (buffer->head == buffer->tail) // buffer has only one node
            {
                buffer->head = buffer->tail = NULL;
            } 
            else  // buffer has many nodes empty
            {
                buffer->head = buffer->head->next;
            }
            free(dummy);
            pthread_mutex_unlock(&mutex);
            return SBUFFER_SUCCESS;
        }   
	}
	else  
	{
		*data = buffer->head->data;
        dummy = buffer->head;
        dummy->threadID1 = pthread_id;
        pthread_mutex_unlock(&mutex);
        pthread_cond_signal(&data_available);
        return SBUFFER_SUCCESS;   
    }
    return SBUFFER_FAILURE;
}

int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data) { // one thread execute this function at a time. 
    
    if (buffer == NULL)
    {
        return SBUFFER_FAILURE;
    }
    pthread_mutex_lock(&mutex);
    sbuffer_node_t *dummy;
    dummy = malloc(sizeof(sbuffer_node_t));
    if (dummy == NULL) 
    {
        pthread_mutex_unlock(&mutex);
        return SBUFFER_FAILURE;
    }

    dummy->data = *data;
    dummy->next = NULL;
    dummy->threadID1 = 0; 
    if (buffer->tail == NULL) // buffer empty (buffer->head should also be NULL
    {
        buffer->head = buffer->tail = dummy;
    } 
    else // buffer not empty
    {
        buffer->tail->next = dummy;
        buffer->tail = buffer->tail->next;
    }
    pthread_cond_signal(&data_available);
    pthread_mutex_unlock(&mutex);
    return SBUFFER_SUCCESS;
}

void set_buffer_stop(sbuffer_t *buffer, bool flag)
{
    pthread_cond_signal(&data_available);
    buffer->server_stop = flag;
}

bool get_buffer_stop(sbuffer_t *buffer)
{
    return buffer->server_stop;
}
