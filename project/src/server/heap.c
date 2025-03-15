/**
 * @file heap.c
 * 
 * @author Pedro Vicente (ist1109852), Pedro Jerónimo (ist1110375)
 * 
 * @brief The heapsort algorithm to sort keys of an specified array
 * of keys(Strings).
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <string.h>
#include <stdio.h>
#include "constants.h"

void swap(char t1[MAX_STRING_SIZE], char t2[MAX_STRING_SIZE]){
    char temp[MAX_STRING_SIZE];
    strcpy(temp, t1);
    strcpy(t1, t2);
    strcpy(t2, temp);
}

void heapify_pairs(char keys[][MAX_STRING_SIZE],char values[][MAX_STRING_SIZE], int n, int i){
    int largest= i;
    int left = 2*i + 1;
    int right = 2*i + 2;

    if(left < n && strcmp(keys[left], keys[largest]) > 0)
        largest = left;

    if(right < n && strcmp(keys[right], keys[largest]) > 0)
        largest = right;
    
    if(largest != i){
        swap(keys[i], keys[largest]);
        if(values) swap(values[i], values[largest]);
        heapify_pairs(keys,values, n, largest);
    }
}

void heap_sort(char keys[][MAX_STRING_SIZE],char values[][MAX_STRING_SIZE], int n){
    for(int i = n / 2 - 1; i >= 0; i--)
        heapify_pairs(keys,values, n, i);

    for(int i = n - 1; i > 0; i--){
        swap(keys[0], keys[i]);
        if(values) swap(values[0], values[i]);
        heapify_pairs(keys,values, i, 0);
    }
}