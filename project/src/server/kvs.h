/**
 * @file kvs.h
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
#ifndef KEY_VALUE_STORE_H
#define KEY_VALUE_STORE_H

#define TABLE_SIZE 26
#include "constants.h"
#include <stddef.h>
#include <pthread.h>
#include "../common/subs_lists.h"

typedef struct KeyNode {
    char *key;
    char *value;
    struct KeyNode *next;
    struct KeyInt *fd;
} KeyNode;

typedef struct HashTable {
    KeyNode *table[TABLE_SIZE];
    pthread_rwlock_t locks[TABLE_SIZE];
} HashTable;

/// Creates a new event hash table.
/// @return Newly created hash table, NULL on failure
struct HashTable *create_hash_table();

/**
 * @brief Write locks the access to the given keys of the hash table
 * @param ht Hash table where the keys will be locked
 * @param keys Array of keys
 * @param size Number of keys
 */
void write_lock_keys(HashTable* ht, char keys[][MAX_STRING_SIZE], int size);

/**
 * @brief Read locks the access to the given keys of the hash table
 * @param ht Hash table where the keys will be locked
 * @param keys Array of keys
 * @param size Number of keys
 */
void read_lock_keys(HashTable* ht, char keys[][MAX_STRING_SIZE], int size);

/**
 * @brief Unlocks the access to the given keys of the hash table
 * @param ht Hash table where the keys will be unlocked
 * @param keys Array of keys
 * @param size Number of keys
 */
void unlock_keys(HashTable* ht, char keys[][MAX_STRING_SIZE], int size);

/**
 * @brief Read locks all the keys of the hash table
 * @param ht Hash table where the keys will be locked
 */
void read_lock_all_keys(HashTable* ht);

/**
 * @brief Unlocks all the keys of the hash table
 * @param ht Hash table where the keys will be unlocked
 */
void unlock_all_keys(HashTable* ht);

/// Appends a new key value pair to the hash table.
/// @param ht Hash table to be modified.
/// @param key Key of the pair to be written.
/// @param value Value of the pair to be written.
/// @return 0 if the node was appended successfully, 1 otherwise.
int write_pair(HashTable *ht, const char *key, const char *value);

/// Gets the value of given key.
/// @param ht Hash table where the key is.
/// @param key Key of the pair to be read.
/// @return 0 if the value was read successfully, 1 otherwise.
char* read_pair(HashTable *ht, const char *key);

/// Deletes a key value pair from the hash table.
/// @param ht Hash table to be modified.
/// @param key Key of the pair to be deleted.
/// @return 0 if the node was deleted successfully, 1 otherwise.
int delete_pair(HashTable *ht, const char *key);

/// Frees the hashtable.
/// @param ht Hash table to be deleted.
void free_table(HashTable *ht);

/**
 * @brief Adds a file descriptor to the list of file descriptors to which
 * the value of the given key must be written when it is updated.
 * 
 * @param ht Hash table to be modified.
 * @param key Key to which the file descriptor mut be associated.
 * @param notif_fd File descriptor.
 * @return 0 if the file descriptor was added successfully, 1 otherwise.
 */
int subscribe_pair(HashTable * ht, const char*key, int notif_fd);

/**
 * @brief Deletes a file descriptor from the list of file descriptors to which
 * the value of the given key must be written when it is updated.
 * 
 * @param ht Hash table to be modified.
 * @param key Key from which the file descriptor mut be disassociated.
 * @param notif_fd File descriptor.
 * @return 0 if the file descriptor was deleted successfully, 1 otherwise.
 */
int unsubscribe_pair(HashTable* ht, const char*key, int notif_fd);

/**
 * @brief Deletes all the file descriptors previously associated with 
 * any key in the hash table, so that from now on, their values will no longer 
 * be written to upon update.
 * 
 * @param ht Hash table to be modified.
 */
void clear_subscriptions(HashTable* ht);

/**
 * @brief Deletes all the subscriptions of a file descriptor from the
 * given hash table.
 * 
 * @param ht Hash table to be modified.
 * @param notif_fd File descriptor.
 */
void clear_fifo_subscriptions(HashTable* ht, int notif_fd);

/** @brief Notifies the clients that have subscribed to a specific key.
 *
 * @param ht Hash table where the key is in.
 * @param key The key.
 * @param value The value of the key.
 * @return 0 if the clients were notified, 1 otherwise.
 */ 
int kvs_notify(HashTable* ht, const char *key, const char *value);

#endif  // KVS_H
