/**
 * @file subs_lists.c
 * 
 * @author Pedro Vicente (ist1109852), Pedro Jerónimo (ist1110375)
 * 
 * @brief Functions in order to create, insert and delete
 *        linked lists of strings and integers
 * 
 */

#include <stdlib.h>
#include "subs_lists.h"
#include <string.h>

struct KeyChar* insert_KeyChar_List(KeyChar* head, char* node){
    KeyChar* new = (KeyChar*)malloc(sizeof(KeyChar));
    strcpy(new->key, node);
    new->next = head;
    
    return new;
}

struct KeyChar* delete_KeyChar_List(KeyChar* head, char* node){
    if(!head) return head; // List is empty.

    if(strcmp(head->key, node) == 0){
        KeyChar* temp = head->next;
        free(head);
        return temp;
    }

    KeyChar* aux = head;
    while(aux->next && strcmp(aux->next->key, node) != 0) aux = aux->next;

    if(aux->next){
        KeyChar* temp = aux->next->next;
        free(aux->next);
        aux->next = temp;
    }

    return head;
}

void delete_All_Char(KeyChar* head){
    KeyChar* current = head;
    KeyChar* nextNode;

    while(current != NULL){
        nextNode = current->next;
        free(current);
        current = nextNode;
    } 
}

struct KeyInt* insert_KeyInt_List(KeyInt* head, int node){
    KeyInt* new = (KeyInt*)malloc(sizeof(KeyInt));
    new->fd = node;
    new->next = head;

    return new;
}

struct KeyInt* delete_KeyInt_List(KeyInt* head, int node){
    if(!head) return head;

    KeyInt* tmp;
    if(head->fd == node){
        tmp = head->next;
        free(head);
        head = tmp;
    }
    else{
        KeyInt* aux;
        for(aux = head; aux->next; aux = aux->next)
            if(aux->next->fd == node){
                tmp = aux->next->next;
                free(aux->next);
                aux->next = tmp;
                break;
            }
    }

    return head;
}

void delete_All_Int(KeyInt* head){
    KeyInt* current = head;
    KeyInt* nextNode;

    while(current != NULL){
        nextNode = current->next;
        free(current);
        current = nextNode;
    } 
}