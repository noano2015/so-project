/**
 * @file heap.h
 * 
 * @author Pedro Vicente (ist1109852), Pedro Jerónimo (ist1110375)
 * 
 * @brief The heapsort algorithm to sort keys of an specified array
 * of keys(Strings).
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef __HEAP_H__
#define __HEAP_H__

#include "constants.h"

/**
 * Sorts the pairs by the value of the keys
 * if values is NULL, sorts only a generic vector of keys
 * @param keys Array of keys
 * @param values Array of values of the respective keys
 * @param n Number of keys/values
 */
void heap_sort(char keys[][MAX_STRING_SIZE],char values[][MAX_STRING_SIZE], int n);

#endif