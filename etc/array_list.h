#ifndef ARRAYLIST
#define ARRAYLIST

//#include <stdio.h>

typedef struct al {
  int *data;
  int data_len;
  int len;
  int cur;
} array_list;

#define AL_START_SIZE 10



array_list *al_new();
void al_init(array_list *the_al);
void al_add(array_list *the_al, int data);
int al_remove(array_list *the_al, int data);
int al_ith(array_list *the_al, int index);
int al_has_next(array_list *the_al);
int al_next(array_list *the_al);
int al_current(array_list * the_al);
void al_free(array_list *the_al);
array_list *al_concatenate(array_list *first, array_list *second);

#endif
