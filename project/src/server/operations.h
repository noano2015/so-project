/**
 * @file operations.h
 * 
 * @author Pedro Vicente (ist1109852), Pedro Jerónimo (ist1110375)
 * 
 * @brief Contains functions to the main server read commands from 
 * diverse file descriptors and operate on the KVS Table like read,
 * write, delete as well as create and destroy it. 
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef KVS_OPERATIONS_H
#define KVS_OPERATIONS_H

#include <stddef.h>
#include "../common/subs_lists.h"

/// Initializes the KVS state.
/// @return 0 if the KVS state was initialized successfully, 1 otherwise.
int kvs_init();

/// Destroys the KVS state.
/// @return 0 if the KVS state was terminated successfully, 1 otherwise.
int kvs_terminate();

/// Writes a key value pair to the KVS. If key already exists it is updated.
/// @param num_pairs Number of pairs being written.
/// @param keys Array of keys' strings.
/// @param values Array of values' strings.
/// @return 0 if the pairs were written successfully, 1 otherwise.
int kvs_write(size_t num_pairs, char keys[][MAX_STRING_SIZE], char values[][MAX_STRING_SIZE]);

/// Reads values from the KVS.
/// @param num_pairs Number of pairs to read.
/// @param keys Array of keys' strings.
/// @param fd File descriptor to write the (successful) output
/// @return 0 if the key reading, 1 otherwise.
int kvs_read(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd);

/// Deletes key value pairs from the KVS.
/// @param num_pairs Number of pairs to read.
/// @param keys Array of keys' strings.
/// @param fd File descriptor to write the (successful) output.
/// @return 0 if the pairs were deleted successfully, 1 otherwise.
int kvs_delete(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd);

/// Writes the state of the KVS.
/// @param fd File descriptor to write the output.
void kvs_show(int fd);

/// Creates a backup of the KVS state and stores it in the correspondent
/// backup file.
/// @param name name of the backup. 
/// @return 0 if the backup was successful, 1 otherwise.
int kvs_backup(char* name);

/// @brief Read locks all the keys of the hash table.
void kvs_read_lock();

/// @brief Unlocks all the keys of the hash table.
void kvs_unlock();

/// Waits for a given amount of time.
/// @param delay_us Delay in milliseconds.
void kvs_wait(unsigned int delay_ms);

/// @brief Subscribes the specified key for the given client.
/// @param fd_notify_fifo Notifications pipe of the client.
/// @param key Key that the client wants to subscribe.
/// @return 1 if it exists, 0 otherwise.
int kvs_subscribe(int notif_fd, const char key[MAX_STRING_SIZE]);

/// @brief Unubscribes the specified key for a given client.
/// @param fd_notify_fifo Notifications pipe of the client.
/// @param key Key that the client wants to unsubscribe.
/// @return 0 if it exists, otherwise.
int kvs_unsubscribe(int notif_fd, const char key[MAX_STRING_SIZE]);

/// @brief Unsubscribes all clients from all keys.
void kvs_clear_subscriptions();

/// @brief Unsubscribes all the keys that a specific client had subscribed.
/// @param fd_notify_fifo Notifications pipe of the client.
/// @return 0 if it unsubscribed successfully, 1 otherwise.
int unsubscribe_fifo(int notif_fd);

#endif  // KVS_OPERATIONS_H
