/**
 * @file operations.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "kvs.h"
#include "constants.h"
#include <pthread.h>
#include "heap.h"
#include "../common/io.h"

static struct HashTable* KVS_TABLE = NULL;
pthread_rwlock_t PERMISSION_LOCK;

/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms){
  return (struct timespec) {delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

int kvs_init(){
  if(KVS_TABLE != NULL){
    fprintf(stderr, "[OPERATIONS] KVS state has already been initialized.\n");
    return 1;
  }

  pthread_rwlock_init(&PERMISSION_LOCK, NULL);

  KVS_TABLE = create_hash_table();
  return KVS_TABLE == NULL;
}

int kvs_terminate(){
  if(KVS_TABLE == NULL){
    fprintf(stderr, "[OPERATIONS] KVS state must be initialized.\n");
    return 1;
  }

  pthread_rwlock_destroy(&PERMISSION_LOCK);
  free_table(KVS_TABLE);
  return 0;
}

void kvs_read_lock(){
  read_lock_all_keys(KVS_TABLE);
}

void kvs_unlock(){
  unlock_all_keys(KVS_TABLE);
}

int kvs_write(size_t num_pairs, char keys[][MAX_STRING_SIZE], char values[][MAX_STRING_SIZE]){
  if(KVS_TABLE == NULL){
    fprintf(stderr, "[OPERATIONS] KVS state must be initialized.\n");
    return 1;
  }

  // Sort the keys to avoid deadlocks
  heap_sort(keys, values, (int)num_pairs);

  // Avoid performing while other thread is executing the show command
  pthread_rwlock_rdlock(&PERMISSION_LOCK);

  // Write lock the given keys of the hash table
  write_lock_keys(KVS_TABLE, keys, (int)num_pairs);

  // Write all the given pairs
  for(size_t i = 0; i < num_pairs; i++){
    if(write_pair(KVS_TABLE, keys[i], values[i]) != 0){
      fprintf(stderr, "[OPERATIONS] Failed to write keypair (%s,%s).\n", keys[i], values[i]);
      unlock_keys(KVS_TABLE, keys, (int)num_pairs);
      pthread_rwlock_unlock(&PERMISSION_LOCK);
      return 1;
    }
  }

  
  size_t i = 0;
  while (i < (num_pairs-1)){
    if(strncmp(keys[i], keys[i+1], MAX_STRING_SIZE) == 0);
    else if(kvs_notify(KVS_TABLE, keys[i], values[i]) < 0){
      fprintf(stderr, "[OPERATIONS] Failed to notify the clients about key's modification.\n");
      unlock_keys(KVS_TABLE, keys, (int)num_pairs);
      pthread_rwlock_unlock(&PERMISSION_LOCK);
      return 1;
    }
    i++;
  }
  kvs_notify(KVS_TABLE, keys[num_pairs - 1], values[num_pairs - 1]);

  // Unlock the keys that were previously locked
  unlock_keys(KVS_TABLE, keys, (int)num_pairs);

  pthread_rwlock_unlock(&PERMISSION_LOCK);
  return 0;
}

int kvs_read(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd){
    if(KVS_TABLE == NULL){
      fprintf(stderr, "[OPERATIONS] KVS state must be initialized.\n");
      return 1;
    }

    char aux[MAX_WRITE_SIZE];

    // Sorts the keys to avoid deadlocks
    heap_sort(keys, NULL, (int)num_pairs);

    // Avoid performing while other thread is executing the show command
    pthread_rwlock_rdlock(&PERMISSION_LOCK);

    // Read lock the given keys of the hash table
    read_lock_keys(KVS_TABLE, keys, (int)num_pairs);

    // Write opening bracket
    if(write_all(fd, "[", 1) < 0){
      fprintf(stderr,"[OPERATIONS] Error writing opening bracket.\n");
      unlock_keys(KVS_TABLE, keys, (int)num_pairs);
      pthread_rwlock_unlock(&PERMISSION_LOCK);
      return 1;
    }

    // Read all the given pairs
    for(size_t i = 0; i < num_pairs; i++){
      char *result = read_pair(KVS_TABLE, keys[i]); // Read the value associated with the key

      if(result == NULL){
        snprintf(aux, sizeof(aux), "(%s,KVSERROR)", keys[i]); // Handle missing key
      }else{
        snprintf(aux, sizeof(aux), "(%s,%s)", keys[i], result); // Format the key-value pair
        free(result); // Free memory allocated by `read_pair`
      }

      // Write formatted string
      size_t len = strlen(aux);
      if(write_all(fd, aux, len) < 0) {
        fprintf(stderr,"[OPERATIONS] Error writing key-value pair.\n");
        unlock_keys(KVS_TABLE, keys, (int)num_pairs);
        pthread_rwlock_unlock(&PERMISSION_LOCK);
        return 1;
      }

      // Add comma between pairs except for the last one
      if(i < num_pairs - 1){
        if(write_all(fd, ",", 1) <-1){
          fprintf(stderr,"[OPERATIONS] Error writing comma separator.\n");
          unlock_keys(KVS_TABLE, keys, (int)num_pairs);
          pthread_rwlock_unlock(&PERMISSION_LOCK);
          return 1;
        }
      }
    }

    // Write closing bracket and newline
    if(write_all(fd, "]\n", 2) < 0){
      fprintf(stderr,"[OPERATIONS] Error writing closing bracket.\n");
      unlock_keys(KVS_TABLE, keys, (int)num_pairs);
      pthread_rwlock_unlock(&PERMISSION_LOCK);
      return 1;
    }

    // Unlock the keys that were previously locked
    unlock_keys(KVS_TABLE, keys, (int)num_pairs);

    pthread_rwlock_unlock(&PERMISSION_LOCK);
    return 0;
}


int kvs_delete(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd){
  if(KVS_TABLE == NULL){
    fprintf(stderr, "[OPERATIONS] KVS state must be initialized.\n");
    return 1;
  }
  int aux = 0;
  char aux_string[MAX_WRITE_SIZE];

  // Sorts the keys to avoid deadlocks
  heap_sort(keys, NULL, (int)num_pairs);

  // Avoid performing while other thread is executing the show command
  pthread_rwlock_rdlock(&PERMISSION_LOCK);

  // Write lock the given keys of the hash table
  write_lock_keys(KVS_TABLE, keys, (int)num_pairs);

  // Delete all the given pairs
  for(size_t i = 0; i < num_pairs; i++){
    if(delete_pair(KVS_TABLE, keys[i]) != 0){
      if(!aux){

        // Writes the first bracket into the file
        if(write_all(fd, "[", 1) < 0){
          fprintf(stderr, "[OPERATIONS] Failed to write the initial bracket to the file.\n");
          unlock_keys(KVS_TABLE, keys, (int)num_pairs);
          pthread_rwlock_unlock(&PERMISSION_LOCK);
          return 1;
        }
        aux = 1;
      }

      // Obtains the formated string and writes into the file
      sprintf(aux_string, "(%s,KVSMISSING)", keys[i]);
      size_t len = strlen(aux_string);
      if(write_all(fd, aux_string, len) < 0){
        fprintf(stderr, "[OPERATIONS] Failed to write the key to the file.\n");
        unlock_keys(KVS_TABLE, keys, (int)num_pairs);
        pthread_rwlock_unlock(&PERMISSION_LOCK);
        return 1;
      }
    }
  }

  // Writes the final bracket
  if(aux){
    if(write_all(fd, "]\n", 2)<0){
      fprintf(stderr,"[OPERATIONS] Failed to write the final bracket to the file.\n");
      pthread_rwlock_unlock(&PERMISSION_LOCK);
      return 1;
    } 
  }

  // Unlock the keys that were previously locked
  unlock_keys(KVS_TABLE, keys, (int)num_pairs);

  pthread_rwlock_unlock(&PERMISSION_LOCK);
  return 0;
}

void kvs_show(int fd){
  // Avoid performing while other thread is executing the read/write/delete
  // command to run properly
  pthread_rwlock_wrlock(&PERMISSION_LOCK);

  // Shows all the pairs present at the hash table
  for (int i = 0; i < TABLE_SIZE; i++) {
    KeyNode *keyNode = KVS_TABLE->table[i];
    while (keyNode != NULL) {
      char aux[MAX_WRITE_SIZE];

      // Obtains the formatted string to write into the file
      sprintf(aux, "(%s, %s)\n", keyNode->key, keyNode->value);

      // Writes into the file
      size_t len = strlen(aux);
      if(write_all(fd, aux, len)  < 0)
        fprintf(stderr, "[OPERATIONS] Failed to write a pair to the file.\n");
      keyNode = keyNode->next; // Move to the next node
    }
  }

  pthread_rwlock_unlock(&PERMISSION_LOCK);
}

int kvs_backup(char* name){
  // Open the backup file
  int fd_backup = open(name, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR | S_IWUSR);
  if(fd_backup < 0){
    fprintf(stderr, "[OPERATIONS] Failed to open the backup file.\n");
    return 1;
  }

  // Write on the backup file
  for (int i = 0; i < TABLE_SIZE; i++) {
    KeyNode *keyNode = KVS_TABLE->table[i];
    while (keyNode != NULL) {
      char aux[MAX_WRITE_SIZE];

      // Obtains the formatted string to write into the file
      if(sprintf(aux, "(%s, %s)\n", keyNode->key, keyNode->value) < 0){
        fprintf(stderr,"[OPERATIONS] Failed to convert a pair to a string.\n");
        return 1;
      }
      // Writes into the file
      size_t len = strlen(aux);
      if(write_all(fd_backup, aux, len) < 0){
        fprintf(stderr, "[OPERATIONS] Failed to write a pair to the bcakup.\n");
        return 1;
      }
      keyNode = keyNode->next; // Move to the next node
    }
  }

  close(fd_backup);
  return 0;
}

void kvs_wait(unsigned int delay_ms){
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}

int kvs_subscribe(int notif_fd, const char key[MAX_STRING_SIZE]){
  // Avoid performing while other thread is executing the show command
  pthread_rwlock_rdlock(&PERMISSION_LOCK);

  
  if(subscribe_pair(KVS_TABLE, key, notif_fd)){
    fprintf(stderr, "[OPERATIONS] Failed to subscribe the key.\n");
    return 1;
  }
  
  pthread_rwlock_unlock(&PERMISSION_LOCK);

  return 0;

}

int kvs_unsubscribe(int notif_fd, const char key[MAX_STRING_SIZE]){
  // Avoid performing while other thread is executing the show command
  pthread_rwlock_rdlock(&PERMISSION_LOCK);

  if(unsubscribe_pair(KVS_TABLE, key, notif_fd)){
    fprintf(stderr, "[OPERATIONS] Failed to unsubscribe the key.\n");
    return 1;
  }

  pthread_rwlock_unlock(&PERMISSION_LOCK);

  return 0;
}

void kvs_clear_subscriptions(){
  // Avoid performing while other thread is executing the show command
  pthread_rwlock_rdlock(&PERMISSION_LOCK);

  clear_subscriptions(KVS_TABLE);

  pthread_rwlock_unlock(&PERMISSION_LOCK);
}

int unsubscribe_fifo(int notif_fd){
  // Avoid performing while other thread is executing the show command
  pthread_rwlock_rdlock(&PERMISSION_LOCK);
  
  clear_fifo_subscriptions(KVS_TABLE, notif_fd);

  pthread_rwlock_unlock(&PERMISSION_LOCK);

  return 0;
}

