/**
 * \author Bert Van Cauter
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "dplist.h"

/*
 * definition of error codes
 * */
#define DPLIST_NO_ERROR 0
#define DPLIST_MEMORY_ERROR 1 // error due to mem alloc failure
#define DPLIST_INVALID_ERROR 2 //error due to a list operation applied on a NULL list 

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


#define DPLIST_ERR_HANDLER(condition, err_code)                         \
    do {                                                                \
            if ((condition)) DEBUG_PRINTF(#condition " failed\n");      \
            assert(!(condition));                                       \
        } while(0)


/*
 * The real definition of struct list / struct node
 */

struct dplist_node {
    dplist_node_t *prev, *next;
    void *element;
};

struct dplist {
    dplist_node_t *head;

    void *(*element_copy)(void *src_element);

    void (*element_free)(void **element);

    int (*element_compare)(void *x, void *y);
};


dplist_t *dpl_create(// callback functions
        void *(*element_copy)(void *src_element),
        void (*element_free)(void **element),
        int (*element_compare)(void *x, void *y)
) {
    dplist_t *list;
    list = malloc(sizeof(struct dplist));
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_MEMORY_ERROR);
    list->head = NULL;
    list->element_copy = element_copy;
    list->element_free = element_free;
    list->element_compare = element_compare;
    return list;
}

void dpl_free(dplist_t **list, bool free_element) {
    int i = dpl_size(*list)-1;
    while(i>=0)
    {
        *list = dpl_remove_at_index(*list,i,free_element);
        i = i - 1; 
    }
    free(*list);
    *list = NULL;
    //TODO: add your code here
}

dplist_t *dpl_insert_at_index(dplist_t *list, void *element, int index, bool insert_copy) {
    dplist_node_t *ref_at_index, *list_node;
    if (list == NULL) return NULL;

    list_node = malloc(sizeof(dplist_node_t));
    DPLIST_ERR_HANDLER(list_node == NULL, DPLIST_MEMORY_ERROR);
    if(insert_copy==true)//Here we check if insert_copy is true? 
    {
        //void *copy = list->element_copy(element); //make a copy
        //list_node->element = copy; //put the copy of the element in the list node
        list_node->element = list->element_copy(element);
    }
    else
    {
        list_node->element = element;
    }
    // pointer drawing breakpoint
    if (list->head == NULL) { // covers case 1 means list is empy
        list_node->prev = NULL;
        list_node->next = NULL;
        list->head = list_node;
        // pointer drawing breakpoint
    } 
    else if (index <= 0) { // covers case 2 at index -1 
        list_node->prev = NULL;
        list_node->next = list->head;
        list->head->prev = list_node;
        list->head = list_node;
        // pointer drawing breakpoint
    } 
    else {
        ref_at_index = dpl_get_reference_at_index(list, index);
        assert(ref_at_index != NULL);
        // pointer drawing breakpoint
        if (index < dpl_size(list)) { // covers case 4
            list_node->prev = ref_at_index->prev;
            list_node->next = ref_at_index;
            ref_at_index->prev->next = list_node;
            ref_at_index->prev = list_node;
            // pointer drawing breakpoint
        } else { // covers case 3
            assert(ref_at_index->next == NULL);
            list_node->next = NULL;
            list_node->prev = ref_at_index;
            ref_at_index->next = list_node;
            // pointer drawing breakpoint
        }
    }
    return list;
    //TODO: add your code here//DONE
}

dplist_t *dpl_remove_at_index(dplist_t *list, int index, bool free_element) {
    if(list == NULL) return NULL;
    else if(list->head == NULL) return list;
    else
    {
        dplist_node_t *dummy_node = dpl_get_reference_at_index(list,index);
        if(dummy_node->prev == NULL && dummy_node->next == NULL) //in this case there is only one element is the list
        {
            if(free_element) list->element_free(&(dummy_node->element));
            list->head = NULL; 
            free(dummy_node);
        }
        else if(dummy_node->prev == NULL && dummy_node->next != NULL) //in this case we want to delete the first element but their are more elements
        {
            if(free_element) list->element_free(&(dummy_node->element));
            list->head = dummy_node->next;
            dummy_node->next->prev = NULL;
            free(dummy_node);
        }
        else if(dummy_node->prev != NULL && dummy_node->next != NULL) //In this case we are in the middle of the list
        {
            if(free_element) list->element_free(&(dummy_node->element));
            dummy_node->prev->next = dummy_node->next;
            dummy_node->next->prev = dummy_node->prev;
            free(dummy_node);
        }
        else if(dummy_node->prev != NULL && dummy_node->next == NULL)//In this case we are at the end of the list with multiple elements and want to remove the last element
        {
            if(free_element) list->element_free(&(dummy_node->element));
            dummy_node->prev->next = NULL;
            free(dummy_node);
        }
        return list;
    }
    //TODO: add your code here//DONE
}

int dpl_size(dplist_t *list) {
    if(list == NULL) return -1;
    if(list->head == NULL) return 0;
    int size = 0;
    dplist_node_t *node = list->head;
    while(node)
    {
        node = node->next;
        size++;
    }
    return size;
    //TODO: add your code here//DONE
}

void *dpl_get_element_at_index(dplist_t *list, int index) {
    if(list == NULL) return NULL;
    dplist_node_t * dummy_node = NULL;
    dummy_node = dpl_get_reference_at_index(list, index); //get a pointer to the node at index. (returns NULL for empty list and NULLlist) 
    if(dummy_node == NULL) return NULL; 
    else
    {
        return dummy_node->element;
    }
    //TODO: add your code here//DONE
}

int dpl_get_index_of_element(dplist_t *list, void *element) {
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);
    if(list == NULL) return -1;
    else
    {
        dplist_node_t * dummy_node = NULL;
        dummy_node = list->head;
        int list_size = dpl_size(list);
        if(dummy_node == NULL) return -1; //list is empty, so element is not found
        for(int i = 0; i < list_size; i++) // going trough the list looking for element 
        {
            if(list->element_compare(dummy_node->element, element)==0)//Here we use the callback function to compare two elements
            {                                                   //because we don't know what these elements are. 
                return i;
            }
            else
            {
                dummy_node = dummy_node->next;
            }
        }
        return -1; //if the element is not found, -1 is returned 
    }
    //TODO: add your code here//DONE
}

dplist_node_t *dpl_get_reference_at_index(dplist_t *list, int index) {
    int count;
    dplist_node_t *dummy = NULL;
    if(list == NULL) return NULL;
    //if (list->head == NULL) return NULL;
    for (dummy = list->head, count = 0; dummy->next != NULL; dummy = dummy->next, count++) {
        if (count >= index) return dummy;
    }
    return dummy;
    //TODO: add your code here//DONE
}

void *dpl_get_element_at_reference(dplist_t *list, dplist_node_t *reference) {
    if(list == NULL || list->head == NULL || reference == NULL) return NULL;
    else
    {
        int list_size = dpl_size(list);
        dplist_node_t * dummy_node;
        dummy_node = list->head;
        for(int i = 0; i<list_size; i++)
        {
            if(dummy_node == reference)
            {
                return dummy_node->element;
            }
            else
            {
                dummy_node = dummy_node->next;
            }
        }
        return NULL;
    }
    //TODO: add your code here//DONE
}

///////////////////////////// EXTRA FUNCTIONS FOR EX4 /////////////////////////////////////////////
dplist_node_t *dpl_get_first_reference(dplist_t *list)
{
    if(list==NULL || list->head==NULL) return NULL;
    else
    {
        return list->head; 
    }
}

dplist_node_t *dpl_get_last_reference(dplist_t *list)
{
    if(list==NULL || list->head==NULL) return NULL;
    else
    {
        dplist_node_t *dummy_node = NULL; 
        dummy_node = dpl_get_reference_at_index(list, (dpl_size(list))+1);
        return dummy_node;
    }
}

dplist_node_t *dpl_get_next_reference(dplist_t *list, dplist_node_t *reference)
{
    if(list==NULL || list->head==NULL || reference==NULL) return NULL;
    else
    {
        dplist_node_t *dummy_node=list->head;
        while(dummy_node!=NULL)
        {
            if(dummy_node==reference)
            {
                return dummy_node->next; 
            }
            else
            {
                dummy_node=dummy_node->next; 
            }
        }
        return NULL; 
    }
}

dplist_node_t *dpl_get_previous_reference(dplist_t *list, dplist_node_t *reference)
{
    if(list==NULL || list->head==NULL || reference==NULL) return NULL;
    else
    {
        dplist_node_t *dummy_node=list->head;
        while(dummy_node!=NULL)
        {
            if(dummy_node==reference)
            {
                return dummy_node->prev; 
            }
            else
            {
                dummy_node=dummy_node->next; 
            }
        }
        return NULL; 
    }
}

dplist_node_t *dpl_get_reference_at_element(dplist_t *list, void *element)
{
    if(list==NULL || list->head==NULL) return NULL;
    else
    {
        dplist_node_t *dummy_node = list->head; 
        while(dummy_node!=NULL)
        {
            if((list->element_compare(dummy_node->element, element))==0)
            {
                return dummy_node;
            }
            else
            {
                dummy_node=dummy_node->next; 
            }
        }
        return NULL; 
    }
}

int dpl_get_index_of_reference(dplist_t *list, dplist_node_t *reference)
{
    if(list==NULL || list->head==NULL || reference==NULL) return -1; 
    else
    {
        int index = 0;
        dplist_node_t *dummy_node = list->head;
        while(dummy_node!=NULL)
        {
            if(reference == dummy_node)
            {
                return index;
            }
            else
            {
                index = index +1; 
                dummy_node = dummy_node->next;
            }
        }
        return -1; 
    }
}

dplist_t *dpl_insert_at_reference(dplist_t *list, void *element, dplist_node_t *reference, bool insert_copy)
{
    if(list==NULL || reference==NULL) return NULL; 
    else
    {
        int index; 
        index = dpl_get_index_of_reference(list,reference);
        if(index == -1)
        {
            return list; 
        }
        else
        {
            list = dpl_insert_at_index(list, element, index, insert_copy);
            return list; 
        }
    }
}

dplist_t *dpl_insert_sorted(dplist_t *list, void *element, bool insert_copy)
{
    return list;
}

dplist_t *dpl_sort_list(dplist_t *list)
{
    // if(list==NULL) return NULL;
    // else if(list->head==NULL) return list; 
    // else
    // {
    //     int list_size = dpl_size(list);
    //     dplist_node_t *dummy_node = NULL;// will point t the X element of comparison
    //     dplist_node_t *temp_node = NULL; // will point to the Y element of comparison
    //     for(int i=0;i<list_size;i++)
    //     {
    //         dummy_node = dpl_get_reference_at_index(list, i); //
    //         int counter = 0;
    //         for(int counter=0;counter<list_size;counter++)
    //         {
    //             temp_node = dpl_get_reference_at_index(list,counter);
    //             if(list->element_compare(dummy_node->element, temp_node->element) == -1)
    //             {

    //             }
    //         }

    //     }
    // }
    return list; 
    
}

dplist_t *dpl_remove_at_reference(dplist_t *list, dplist_node_t *reference, bool free_element)
{
    if(reference==NULL || list==NULL) return NULL;
    else
    {
        int index = dpl_get_index_of_reference(list,reference);
        if(index == -1) return list; 
        else
        {
            list = dpl_remove_at_index(list,index, free_element);
            return list;
        }
    } 
}

dplist_t *dpl_remove_element(dplist_t *list, void *element, bool free_element)
{
    if(list==NULL) return NULL; 
    else
    {
        int index = dpl_get_index_of_element(list, element);
        if(index == -1) return list;
        else
        {
            list = dpl_remove_at_index(list,index, free_element);
            return list; 
        }
    }
}

