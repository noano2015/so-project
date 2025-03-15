/**
 * @file kvs.c
 * 
 * @author Pedro Vicente (ist1109852), Pedro Jerónimo (ist1110375)
 * 
 * @brief Contais functions to inicialize and delete the
 * KVS table which will act like a server but also other 
 * functions to write, delete, read pairs from the table.
 * There other functions to clients be able to subscribe
 * and unsubscribe to certain keys.
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "kvs.h"
#include <stdio.h> 
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include "constants.h"
#include "../common/io.h"


// Hash function based on key initial.
// @param key Lowercase alphabetical string.
// @return hash.
// NOTE: This is not an ideal hash function, but is useful for test purposes of the project
int hash(const char *key){
    int firstLetter = tolower(key[0]);
    if (firstLetter >= 'a' && firstLetter <= 'z'){
        return firstLetter - 'a';
    }else if (firstLetter >= '0' && firstLetter <= '9'){
        return firstLetter - '0';
    }
    return -1; // Invalid index for non-alphabetic or number strings
}

void read_lock_keys(HashTable* ht, char keys[][MAX_STRING_SIZE], int size){
    int locked[TABLE_SIZE] = {0};
    for(int i = 0; i < size; i++){
        if(!locked[hash(keys[i])]){
            pthread_rwlock_rdlock(&ht->locks[hash(keys[i])]);
            locked[hash(keys[i])] = 1;
        }
    }
}

void write_lock_keys(HashTable* ht, char keys[][MAX_STRING_SIZE], int size){
    int locked[TABLE_SIZE] = {0};
    for(int i = 0; i < size; i++){
        if(!locked[hash(keys[i])]){
            pthread_rwlock_wrlock(&ht->locks[hash(keys[i])]);
            locked[hash(keys[i])] = 1;
        }
    }
}

void unlock_keys(HashTable* ht, char keys[][MAX_STRING_SIZE], int size){
    int unlocked[TABLE_SIZE] = {0};
    for(int i = 0; i < size; i++){
        if(!unlocked[hash(keys[i])]){
            pthread_rwlock_unlock(&ht->locks[hash(keys[i])]);
            unlocked[hash(keys[i])] = 1;
        }
    }
}

void read_lock_all_keys(HashTable* ht){
    for(int i = 0; i < TABLE_SIZE; i++)
        pthread_rwlock_rdlock(&(ht->locks[i]));
}

void write_lock_all_keys(HashTable* ht){
    for(int i = 0; i < TABLE_SIZE; i++)
        pthread_rwlock_wrlock(&(ht->locks[i]));
}

void unlock_all_keys(HashTable* ht){
    for(int i = 0; i < TABLE_SIZE; i++)
        pthread_rwlock_unlock(&(ht->locks[i]));
}

struct HashTable* create_hash_table() {
  HashTable *ht = malloc(sizeof(HashTable));
  if (!ht) return NULL;
  for (int i = 0; i < TABLE_SIZE; i++) {
      ht->table[i] = NULL;
      pthread_rwlock_init(&ht->locks[i], NULL);
  }
  return ht;
}

int write_pair(HashTable *ht, const char *key, const char *value) {
    int index = hash(key);
    KeyNode *keyNode = ht->table[index];

    // Search for the key node
    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            free(keyNode->value);
            keyNode->value = strdup(value);
                       
            return 0;
        }
        keyNode = keyNode->next; // Move to the next node
    }

    // Key not found, create a new key node
    keyNode = malloc(sizeof(KeyNode));
    keyNode->key = strdup(key); // Allocate memory for the key
    keyNode->value = strdup(value); // Allocate memory for the value
    keyNode->fd = NULL;
    keyNode->next = ht->table[index]; // Link to existing nodes
    ht->table[index] = keyNode; // Place new key node at the start of the list
    return 0;
}

char* read_pair(HashTable *ht, const char *key) {
    int index = hash(key);
    KeyNode *keyNode = ht->table[index];
    char* value;

    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            value = strdup(keyNode->value);
    
            return value; // Return copy of the value if found
        }
        keyNode = keyNode->next; // Move to the next node
    }
    return NULL; // Key not found
}

int delete_pair(HashTable *ht, const char *key) {
    int index = hash(key);
    KeyNode *keyNode = ht->table[index];
    KeyNode *prevNode = NULL;

    // Search for the key node
    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            // Key found; delete this node
            if (prevNode == NULL) {
                // Node to delete is the first node in the list
                ht->table[index] = keyNode->next; // Update the table to point to the next node
            } else {
                // Node to delete is not the first; bypass it
                prevNode->next = keyNode->next; // Link the previous node to the next node
            }

            char message[2*(MAX_STRING_SIZE + 1)] = {'\0'};
            strncpy(message, key, MAX_STRING_SIZE);
            strncpy(message + MAX_STRING_SIZE + 1, "DELETED", 8);

            for(KeyInt* aux = keyNode->fd; aux != NULL; aux = aux->next){
                if(write_all(aux->fd, message, 2*(MAX_STRING_SIZE + 1)) < 0)
                    fprintf(stderr, "[KVS] Failed to write to the notifications pipe.\n");
            }
            
            // Free the memory allocated for the key and value
            free(keyNode->key);
            free(keyNode->value);
            free(keyNode); // Free the key node itself
    
            return 0; // Exit the function
        }
        prevNode = keyNode; // Move prevNode to current node
        keyNode = keyNode->next; // Move to the next node
    }
    return 1;
}

int kvs_notify(HashTable* ht, const char *key, const char *value){
    int index = hash(key);
    KeyNode *keyNode = ht->table[index];

    char message[2*(MAX_STRING_SIZE + 1)] = {'\0'};
    strncpy(message, key, MAX_STRING_SIZE);
    strncpy(message + MAX_STRING_SIZE + 1, value, MAX_STRING_SIZE);

    // Search for the key node
    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            for(KeyInt* aux = keyNode->fd; aux != NULL; aux = aux->next)
                if(write_all(aux->fd, message, 2*(MAX_STRING_SIZE + 1)) < 0)
                    fprintf(stderr, "[KVS] Failed to write to the notififcations pipe.\n");

            return 0;
        }
        keyNode = keyNode->next; // Move to the next node
    }
    return 1;
}

int subscribe_pair(HashTable * ht, const char*key, int notif_fd){
    int index = hash(key);
    pthread_rwlock_wrlock(&ht->locks[index]);
    KeyNode *keyNode = ht->table[index];
    
    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            keyNode->fd = insert_KeyInt_List(keyNode->fd, notif_fd);
            pthread_rwlock_unlock(&ht->locks[index]);
            return 0; 
        }
        keyNode = keyNode->next; // Move to the next node
    }
    pthread_rwlock_unlock(&ht->locks[index]);

    return 1; // Key not found
}

int unsubscribe_pair(HashTable* ht, const char*key, int notif_fd){
    int index = hash(key);
    pthread_rwlock_wrlock(&ht->locks[index]);
    KeyNode *keyNode = ht->table[index];
    
    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            keyNode->fd = delete_KeyInt_List(keyNode->fd, notif_fd);
            pthread_rwlock_unlock(&ht->locks[index]);
            return 0; 
        }
        keyNode = keyNode->next; // Move to the next node
    }
    pthread_rwlock_unlock(&ht->locks[index]);
    return 1;
}

void clear_subscriptions(HashTable* ht){
    write_lock_all_keys(ht);

    for(int i = 0; i < TABLE_SIZE; i++){
        KeyNode *keyNode = ht->table[i];

        while(keyNode != NULL){
            delete_All_Int(keyNode->fd);
            keyNode->fd = NULL;
            keyNode = keyNode->next;
        }
    }

    unlock_all_keys(ht);
}

void clear_fifo_subscriptions(HashTable* ht, int notif_fd){
    write_lock_all_keys(ht);
    for(int i = 0; i < TABLE_SIZE; i++){
        KeyNode *keyNode = ht->table[i];

        while(keyNode != NULL){
            keyNode->fd = delete_KeyInt_List(keyNode->fd, notif_fd);
            keyNode = keyNode->next;
        }
    }
    unlock_all_keys(ht);
}

void free_table(HashTable *ht) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        KeyNode *keyNode = ht->table[i];
        pthread_rwlock_destroy(&ht->locks[i]);
        
        while (keyNode != NULL) {
            KeyNode *temp = keyNode;
            keyNode = keyNode->next;
            free(temp->key);
            free(temp->value);
            free(temp);
        }
    }
    free(ht);
}