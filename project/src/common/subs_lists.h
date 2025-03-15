/**
 * @file subs_lists.h
 * 
 * @author Pedro Vicente (ist1109852), Pedro Jerónimo (ist1110375)
 * 
 * @brief Functions in order to create, insert and delete
 *        linked lists of strings and integers
 * 
 */

#ifndef __SUBS_LISTS_H__
#define __SUBS_LISTS_H__
#include "../common/constants.h"

typedef struct KeyChar{
    char key[MAX_STRING_SIZE + 1];
    struct KeyChar *next;
} KeyChar;

typedef struct KeyInt{
    int fd;
    struct KeyInt* next;
} KeyInt;

/**
 * @brief Inserts the specified node into the given KeyChar linked list
 * and returns the new head of the linked list.
 * 
 * @param head Head of the linked list.
 * @param node Node which we want to insert.
 * @return New head of the list.
 */
struct KeyChar* insert_KeyChar_List(KeyChar* head, char* node);

/**
 * @brief Deletes the specified node of the given KeyChar linked list
 * and returns the new head of the linked list.
 * 
 * @param head Head of the linked list.
 * @param node Node which we want to delete.
 * @return New head of the list.
 */
struct KeyChar* delete_KeyChar_List(KeyChar* head, char* node);

/**
 * @brief Deletes all the elements of the given KeyChar linked list.
 * 
 * @param head -head of the linked list.
 */
void delete_All_Char(KeyChar* head);

/**
 * @brief Inserts the specified node into the given KeyInt linked list
 * and returns the new head of the linked list.
 * 
 * @param head Head of the linked list.
 * @param node Node which we want to insert.
 * @return New head of the list.
 */
struct KeyInt* insert_KeyInt_List(KeyInt* head, int node);

/**
 * @brief Deletes the specified node of the given KeyInt linked list
 * and returns the head of the linked list.
 * 
 * @param head Head of the linked list.
 * @param node Node which we want to delete.
 * @return New head of the list.
 */
struct KeyInt* delete_KeyInt_List(KeyInt* head, int node);

/**
 * @brief Deletes all the elements of the given KeyInt linked list.
 * 
 * @param head - head of the linked list.
 */
void delete_All_Int(KeyInt* head);

#endif 
