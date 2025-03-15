/**
 * @file main.c
 * 
 * @author Pedro Vicente (ist1109852), Pedro Jerónimo (ist1110375)
 * 
 * @brief The main file of the client. It receives two arguments, 
 * its id and the server's pipe path (in order to connect to the
 * server), in this specified order.
 * 
 * The client connects to the server sending the request, response 
 * and notifications pipes paths. After connected can subscribe and
 * unsubscribe to which keys we wants to of the server.
 * 
 * Client may also send a request to disconnect to the server.
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "parser.h"
#include "src/client/api.h"
#include "../common/constants.h"
#include "../common/io.h"
#include <bits/types/sigset_t.h>
#include <bits/sigaction.h>
#include "../common/subs_lists.h"

#define MAX_HELP_CLIENT_STRING 79

KeyChar* SUBS_LIST = NULL;
int AVAILABLE_SUBS = MAX_NUMBER_SUB;
int END = 0, SIGUSR1_RECEIVED = 0, THREAD_FORCED_CLOSE = 0;

void handle_signal(int sig){
  if(sig == SIGUSR1){
    SIGUSR1_RECEIVED = 1;
  }
}

void* receive_notifications(void* args){
  int notif_fd = *((int*) args), io_result;
  char notification[2*(MAX_STRING_SIZE + 1)];

  // Blocks SIGUSR1 in this thread
  sigset_t sigset1;
  sigemptyset(&sigset1);
  sigaddset(&sigset1, SIGUSR1);
  if(pthread_sigmask(SIG_BLOCK, &sigset1, NULL) != 0){
    fprintf(stderr,"[NOTIFICATIONS THREAD] Failed to mask SIGURSR1.\n");
    pthread_exit(NULL);
  }

  // Reads a notification from notifications pipe
  while(!END){
    // Reads the key that has been modified and its new value from the notifications pipe
    if((io_result = read_all(notif_fd, notification, 2*(MAX_STRING_SIZE + 1), NULL)) == 0
        && !END){
      fprintf(stderr, "[NOTIFICATIONS THREAD] Server connection lost.\n");

      // Stops main
      THREAD_FORCED_CLOSE = 1;
      END = 1;
      close(STDIN_FILENO);          // Prevents blocks on potential future get_next() calls of read()
      kill(getpid(), SIGUSR1);      // Unblocks main if blocked on one of get_next() calls of read()

      break;
    }
    else if(io_result == -1 && !END){
      fprintf(stderr, "[NOTIFICATIONS THREAD] Failed to read a notification from the notifications pipe.\n");
      continue;
    }
    else if(io_result == 1){
      char value[MAX_STRING_SIZE + 1] = {'\0'}, key[MAX_STRING_SIZE + 1] = {'\0'};
      strncpy(key, notification, MAX_STRING_SIZE + 1);
      strncpy(value, notification + MAX_STRING_SIZE + 1, MAX_STRING_SIZE + 1);

      // Writes the notification read
      char message[2*(MAX_STRING_SIZE + 1) + 5] = {'\0'};
      if(snprintf(message, 2*(MAX_STRING_SIZE+1) + 5, "(%s,%s)\n", key, value) < 0){
        fprintf(stderr, "[NOTIFICATIONS THREAD] Failed to create a notification.\n");
        continue;
      }
      write_all(1, message, strlen(message));

      if(strcmp(value, "DELETED") == 0){
        write_all(1, "[NOTIFICATIONS THREAD] Key has been removed from the subscripitons.\n", 69);
        SUBS_LIST = delete_KeyChar_List(SUBS_LIST, key);
        AVAILABLE_SUBS++;
      }
    }
  }

  pthread_exit(NULL);
}

int main(int argc, char* argv[]){
  // The program must have exaclty 3 arguments
  if(argc < 3){
    fprintf(stderr, "Usage: %s <client_unique_id> <register_pipe_path>\n", argv[0]);
    return 1;
  }

  char req_pipe_path[MAX_PIPE_PATH_LENGTH] = "/tmp/req";
  char resp_pipe_path[MAX_PIPE_PATH_LENGTH] = "/tmp/resp";
  char notif_pipe_path[MAX_PIPE_PATH_LENGTH] = "/tmp/notif";
  unsigned int delay_ms;
  size_t num;

  // Obtains the name of the client's pipes
  strncat(req_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
  strncat(resp_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
  strncat(notif_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));

  write_all(1, "Connecting to the KVS server...\n", 33);

  // Connects to the server
  int notif_fd;
  if(kvs_connect(req_pipe_path, resp_pipe_path, argv[2], notif_pipe_path, &notif_fd)){
    fprintf(stderr, "Failed to connect to the server.\n");
    return 1;
  }

  write_all(1, "Connected to the KVS server.\n", 30);

  // Prepares to handle SIGUSR1
  if(signal(SIGUSR1, handle_signal) == SIG_ERR){
    fprintf(stderr, "Failed to create a routine to handle SIGUSR1.\n");
    return 1;
  }

  // Creates the notifications thread
  pthread_t notif_thread;
  if(pthread_create(&notif_thread, NULL, receive_notifications, (void*) &notif_fd) < 0){
    fprintf(stderr, "Failed to create the notifications thread.\n");
    kvs_disconnect(1);
    return 1;
  }
  
  // Executes the commands requested by the client
  int result = 0, exists;
  char key[1][MAX_STRING_SIZE] = {'\0'};
  while(!END){
    exists = 0;
    SIGUSR1_RECEIVED = 0;
    switch(get_next(STDIN_FILENO)){
      case CMD_DISCONNECT:
        END = 1;
        if((result = kvs_disconnect(0))){
          fprintf(stderr, "Command disconnect failed.\n");

          if(result == 1){
            pthread_join(notif_thread, NULL);
            END = 0;
            if(pthread_create(&notif_thread, NULL, receive_notifications, (void*) &notif_fd) < 0){
              fprintf(stderr, "Failed to create the notifications thread\n");
              kvs_disconnect(1);
              END = 1;
            }
          }
        }
        else
          write_all(1, "Disconnected from server.\n", 27);
        
        break;

      case CMD_SUBSCRIBE:
        num = parse_list(STDIN_FILENO, key, 1, MAX_STRING_SIZE);
        if(num == 0){
          fprintf(stderr, "Invalid command. See HELP for usage.\n");
          continue;
        }

        // Verifies if the client had already subscribed to the key
        for(KeyChar* aux = SUBS_LIST; aux != NULL; aux = aux->next)
          if(strcmp(aux->key, key[0]) == 0){
            fprintf(stderr, "The subscription was already made.\n");
            exists = 1;
          }
        if(exists)
          continue;

        // Verifies if there is enough space for the subscription
        else if(AVAILABLE_SUBS == 0){
          fprintf(stderr, "Maximum number of subscriptions has been reached.\n");
          continue;
        }

        if((result = kvs_subscribe(key[0]))){
          fprintf(stderr, "Command subscribe failed.\n");
          if(result == 2)
            END = 1;
        }
        else{
          SUBS_LIST = insert_KeyChar_List(SUBS_LIST, key[0]);
          AVAILABLE_SUBS--;
        }

        break;

      case CMD_UNSUBSCRIBE:
        num = parse_list(STDIN_FILENO, key, 1, MAX_STRING_SIZE);
        if(num == 0){
          fprintf(stderr, "Invalid command. See HELP for usage.\n");
          continue;
        }

        // Verifies if there are subscriptions
        if(AVAILABLE_SUBS == MAX_NUMBER_SUB){
          fprintf(stderr, "No subscriptions done.\n");
          continue;
        }

        // Verifies if the client had subscribed that key
        for(KeyChar* aux = SUBS_LIST; aux != NULL; aux = aux->next)
          if(strcmp(aux->key, key[0]) == 0) exists = 1;
        if(!exists){
          fprintf(stderr, "The key is not subscribed.\n");
          continue;
        }

        if((result = kvs_unsubscribe(key[0]))){
          fprintf(stderr, "Command unsubscribe failed.\n");
          if(result == 2)
            END = 1;
        }
        else{
          SUBS_LIST = delete_KeyChar_List(SUBS_LIST, key[0]);
          AVAILABLE_SUBS++;
        }

        break;

      case CMD_DELAY:
        if(parse_delay(STDIN_FILENO, &delay_ms) == -1){
          fprintf(stderr, "Invalid command. See HELP for usage.\n");
          continue;
        }

        if(delay_ms > 0){
            write_all(1, "Waiting..\n", 11);
            delay(delay_ms);
        }

        break;

      case EOC:
        if(END || SIGUSR1_RECEIVED)
          break;

        write_all(1, "End of commands reached. Disconnecting from server..\n", 53);

        if((result = kvs_disconnect(0)))
          fprintf(stderr, "Disconnection failed.\n");
        else
          write_all(1, "Disconnected from server.\n", 27);
        
        END = 1;

        break;

      case CMD_INVALID:
        if(!(END || SIGUSR1_RECEIVED))
          fprintf(stderr, "Invalid command. See HELP for usage.\n");

        break;
      
      case CMD_EMPTY:
        break;

      case CMD_HELP:
        write_all(1, 
                "Available commands:\n"
                "  DISCONNECT\n"
                "  SUBSCRIBE [key]\n"
                "  UNSUBSCRIBE [key]\n"
                "  HELP\n",
                MAX_HELP_CLIENT_STRING
          );

          break;
    }
  }

  delete_All_Char(SUBS_LIST);

  // Waits for the end of the notifications thread
  pthread_join(notif_thread, NULL);

  if(result || THREAD_FORCED_CLOSE){
    kvs_disconnect(1);
    return 1;
  }
  else
    return 0;
}