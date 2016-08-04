#include "array_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void al_remove_ith(array_list *the_al, int index);

array_list *al_new() {
  array_list *the_list = malloc(sizeof(array_list));
  return the_list;
}

void al_init(array_list *the_al) {
  the_al->data = malloc(AL_START_SIZE * sizeof(int));
  the_al->cur = 0;
  the_al->len = 0;
  the_al->data_len = AL_START_SIZE;
}

void al_add(array_list *the_al, int data) {
  if(the_al->data_len <= the_al->len) {
    int *new_data = malloc(the_al->data_len * 2 * sizeof(int));
    memcpy(new_data, the_al->data, the_al->data_len * sizeof(int));
    free(the_al->data);
    the_al->data = new_data;
    the_al->data_len *= 2;
  }
  
  the_al->data[the_al->len] = data;
  the_al->len++;
}

int al_remove(array_list * the_al, int data) {
  int i;

  for(i = 0; i < the_al->len; i++) {
    if(the_al->data[i] == data) {
      al_remove_ith(the_al, i);
      return 1;
    }
  }
  return 0;
}

void al_remove_ith(array_list *the_al, int index) {
  the_al->len--;
  memcpy(the_al->data + index, the_al->data + index + 1, the_al->len - index * sizeof(int));
}

int al_ith(array_list *the_al, int index) {
  int found = the_al->data[index];
  return found;
}

int al_has_next(array_list *the_al) {
  int retv = the_al->cur < the_al->len;
  return retv;
}

int al_next(array_list *the_al) {
  int found = the_al->data[++(the_al->cur)];
  return found;
}

int al_current(array_list *the_al) {
  int found = the_al->data[the_al->cur];
  return found;
}

void al_free(array_list *the_al) {
  free(the_al->data);
  free(the_al);
}

array_list *al_concatenate(array_list *first, array_list *second) {
  array_list *retv = al_new();
  al_init(retv);
  retv->data_len = 2 * (first->data_len + second->data_len);
  retv->len = first->len + second->len;
  int *new_data = malloc(retv->data_len * sizeof(int));
  int to_copy = first->data_len * sizeof(int);
  /* printf("AKH: moving %d bytes from first->data (= %p) to new_data (= %p)\n",  */
  /* 	 to_copy,  */
  /* 	 first->data, new_data); */
  memcpy(new_data, first->data, to_copy);
  /* printf("AKH: moving %d bytes from second->data (= %p) to new_data + first->len (= %d) + 1 (= %p)\n",  */
  /* 	 to_copy,  */
  /* 	 second->data,  */
  /* 	 first->len, */
  /* 	 new_data + first->len + 1); */
  to_copy = second->data_len * sizeof(int);
  memcpy(new_data + first->len + 1, second->data,to_copy);
  free(retv->data);
  retv->data = new_data;
  return retv;
}
