#include <stdlib.h>
#include <stdio.h>

#include "blobarray.h"


struct blob_t * getLastBlobblob(struct blob_t * first) {
    if (first == NULL) return first;
    while (first->next != NULL) first = first->next;
    return first;
}

void appendBlob(struct blob_t ** first, void * content ) {
    struct blob_t * newblob = malloc(sizeof(struct blob_t));
    newblob->next = NULL;
    newblob->content = content;
    if (*first == NULL) {
        *first = newblob;
    } else getLastBlobblob(*first)->next = newblob;
}

void * getBlob(struct blob_t * first, int i) {
    struct blob_t * iterator = first;
    first->current = 0;
    while (iterator != NULL) {
        if (i == first->current) return iterator->content; 
        first->current++;
        iterator = iterator->next;
    }
    //not found..
    return NULL;
}
 
void * getFirstBlob(struct blob_t * first) {
    if (first == NULL) return NULL;
    first->current = 0;
    return first->content;
}
    
void * getLastBlob(struct blob_t * first) {
    if (first == NULL) return NULL;
    first->current = 0;
    struct blob_t * iterator = first;
    while (iterator->next != NULL) {
        iterator = iterator->next;
        first->current ++;
    }
    return iterator->content;
}
    
void  * getNextBlob(struct blob_t * first) {
    if (first != NULL) {
        first->current++;
        return getBlob(first, first->current);
    } else return NULL;
} 

void removeBlobArray(struct blob_t * first) {
    struct blob_t * iterator = first;
    struct blob_t * blob;
    while (iterator != NULL) {
        blob = iterator;
        iterator = iterator ->next;
        free(blob->content);
        free(blob);
    }
}

void prepBlobIteration(struct blob_t * first) {
    if (first != NULL) first->current = -1;
}

