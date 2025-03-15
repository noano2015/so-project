/**
 * @file api.c
 * 
 * @author Pedro Vicente (ist1109852), Pedro Jerónimo (ist1110375)
 * 
 * @brief An api that establishs a connection between the client and 
 * the server, processing requests and their respective responses
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "api.h"
#include "../common/constants.h"
#include "../common/protocol.h"
#include "../common/io.h"
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <bits/types/sigset_t.h>
#include <signal.h>
#include <bits/sigaction.h>
#include <pthread.h>

#define NOT_EXISTENT -1

// The file descriptores of the client's pipes
int REQ_FD = NOT_EXISTENT, RESP_FD = NOT_EXISTENT, NOTIF_FD = NOT_EXISTENT;

// The client's pipes paths
char REQ_PATH[MAX_PIPE_PATH_LENGTH] = {'\0'},
     RESP_PATH[MAX_PIPE_PATH_LENGTH] = {'\0'},
     NOTIF_PATH[MAX_PIPE_PATH_LENGTH] = {'\0'};

int close_and_unlink(){
  // Closes pipes and unlinks pipes files
  if(REQ_FD != NOT_EXISTENT)close(REQ_FD);
  if(RESP_FD != NOT_EXISTENT)close(RESP_FD);
  if(NOTIF_FD != NOT_EXISTENT )close(NOTIF_FD);

  if(unlink(REQ_PATH) != 0 && errno != ENOENT){
    fprintf(stderr, "Unlink(%s) failed.\n", REQ_PATH);
    return 1;
  }
  if(unlink(RESP_PATH) != 0 && errno != ENOENT){
    fprintf(stderr, "Unlink(%s) failed.\n", RESP_PATH);
    return 1;
  }
  if(unlink(NOTIF_PATH) != 0 && errno != ENOENT){
    fprintf(stderr, "Unlink(%s) failed.\n", NOTIF_PATH);
    return 1;
  }

  return 0;
}

int kvs_connect(char const* req_pipe_path, char const* resp_pipe_path,
                char const* server_pipe_path, char const* notif_pipe_path,
                int* notif_pipe){
  // Blocks SIGPIPE
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGPIPE);
  if(pthread_sigmask(SIG_BLOCK, &sigset, NULL) != 0){
    fprintf(stderr,"[API] Failed to mask SIGPIPE.\n");
    pthread_exit(NULL);
  }
  
  // Copies every pipe path in order to be accessible for other functions of the api
  strcpy(REQ_PATH, req_pipe_path);
  strcpy(RESP_PATH, resp_pipe_path);
  strcpy(NOTIF_PATH, notif_pipe_path);

  if(close_and_unlink()){
    fprintf(stderr, "[API] Falied to unlink and close the client's pipes.\n");
    return 1;
  }

  // Creates the request pipe
  if(mkfifo(req_pipe_path, 0640) != 0){
    fprintf(stderr, "[API] Failed to create request pipe.\n");
    return 1;
  }

  // Creates the response pipe
  if(mkfifo(resp_pipe_path, 0640) != 0){
    fprintf(stderr, "[API] Failed to create response pipe.\n");
    close_and_unlink();
    return 1;
  }

  // Creates the notifications pipe
  if(mkfifo(notif_pipe_path, 0640) != 0){
    close_and_unlink();
    fprintf(stderr, "[API] Failed to create notifications pipe.\n");
    return 1;
  }

  // Opens server pipe
  int server_fd;
  if((server_fd = open(server_pipe_path, O_WRONLY)) < 0){
    fprintf(stderr, "[API] Failed to open the server pipe.\n");
    close_and_unlink();
    return 1;
  }

  // Creates the connection request to be sent to the server
  // content of the message:
  // OP_CODE = 1
  // request pipe path
  // response pipe path
  // notifications pipe path
  char message[3*MAX_PIPE_PATH_LENGTH + 1] = {'\0'};
  if(sprintf(message, "%d", OP_CODE_CONNECT) < 0 
     || snprintf(message + 1, MAX_PIPE_PATH_LENGTH, "%s", REQ_PATH) < 0
     || snprintf(message + 1 + MAX_PIPE_PATH_LENGTH, MAX_PIPE_PATH_LENGTH, "%s", RESP_PATH) < 0
     || snprintf(message + 1 + 2*MAX_PIPE_PATH_LENGTH, MAX_PIPE_PATH_LENGTH, "%s", NOTIF_PATH) < 0){
      close_and_unlink();
      fprintf(stderr, "[API] Failed to create connection request.\n");
      return 1;
     }

  // Writes the connection request to the server pipe
  if(write_all(server_fd, message, 3*MAX_PIPE_PATH_LENGTH + 1) == -1){
    fprintf(stderr, "[API] Failed to write the connection request to the server pipe.\n");
    close_and_unlink();
    return 1;
  }

  close(server_fd);

  // Opens the client pipes
  RESP_FD = open(resp_pipe_path, O_RDONLY);
  REQ_FD = open(req_pipe_path, O_WRONLY);
  NOTIF_FD = open(notif_pipe_path, O_RDONLY);
  *notif_pipe = NOTIF_FD;

  // Reads the result of the connection from the response pipe
  char result[2] = {'\0'};
  if(read_all(RESP_FD, result, 2, NULL) <= 0){
    fprintf(stderr, "[API] Failed to read the connection result from the response pipe.\n");
    close_and_unlink();
    return 1;
  }

  // Prints the result of the operation
  char response[45] = {'\0'};
  if(snprintf(response, 45,
     "Server returned %c for operation: connect.\n", result[1]) < 0){
    fprintf(stderr,"[API] Error in creating the message obtained form the server.\n");
    return 1;
  }
  write_all(1, response, 45);

  if(result[1] - '0' == 1)
    close_and_unlink();
  
  return result[1] - '0';
}
 
int kvs_disconnect(int force_closing){
  char message[2] = {'\0'};

  // In case that the connection to the server is lost unpredicably
  // closes and unlinks the client pipes. Otherwise, sends a 
  // disconnection request to the server
  if(!force_closing){
    // Creates the disconnection request to be sent to the server
    if(snprintf(message, 2, "%d", OP_CODE_DISCONNECT) < 0){
      fprintf(stderr, "[API] Failed to create disconnection request.\n");
      return 1;
    }

    // Writes the disconnection request to the server pipe
    if(write_all(REQ_FD, message, 1) == -1){
      fprintf(stderr, "[API] Failed to write the disconnection request to the request pipe.\n");
      if(errno == EPIPE){
        fprintf(stderr, "[API] Server connection lost.\n");
        return 2;
      }
      return 1;
    }

    // Reads the result of the disconnection from the response pipe
    int io_result;
    if((io_result = read_all(RESP_FD, message, 2, NULL)) <= 0){
      fprintf(stderr, "[API] Failed to read the disconnection result from the response pipe.\n");
      if(io_result == 0)
        fprintf(stderr, "[API] Server connection lost.\n");
      return 2;
    }

    // Prints the result of the operation
    char response[48] = {'\0'};
    if(snprintf(response, 48,
       "Server returned %c for operation: disconnect.\n", message[1]) < 0){
      fprintf(stderr,"[API] Error in creating the message obtained form the server.\n");
      return 1;
    }
    write_all(1, response, 48);

    if(message[1] - '0' == 1) return 1;
  }
  
  // Closes pipes and unlinks pipes files
  close_and_unlink();
 
  return 0;
}

int kvs_subscribe(const char* key){
  // Creates the subscription request to be sent to the server
  char request[MAX_STRING_SIZE + 2] = {'\0'};
  if(snprintf(request, MAX_STRING_SIZE + 2, "%d%s", OP_CODE_SUBSCRIBE, key) < 0){
    fprintf(stderr, "[API] Failed to create subscription request.\n");
    return 1;
  }

  // Writes subscription request to the request pipe
  if(write_all(REQ_FD, request, MAX_STRING_SIZE + 2) == -1){
    fprintf(stderr, "[API] Failed to write the subscription request to the request pipe.\n");
    if(errno == EPIPE){
      fprintf(stderr, "[API] Server connection lost.\n");
      return 2;
    }
    return 1;
  }

  // Reads the result of the subscription from the response pipe
  int io_result;
  char result[2] = {'\0'};
  if((io_result = read_all(RESP_FD, result, 2, NULL)) <= 0){
    fprintf(stderr, "[API] Failed to read the subscription result from the response pipe.\n");
    if(io_result == 0)
      fprintf(stderr, "[API] Server connection lost.\n");
    return 2;
  }
  
  // Prints the result of the operation
  char response[47] = {'\0'};
  if(snprintf(response, 47,
     "Server returned %c for operation: subscribe.\n", result[1]) < 0){
    fprintf(stderr,"[API] Error in creating the message obtained form the server.\n");
    return 1;
  }
  write_all(1, response, 47);

  return !(result[1] - '0');
}

int kvs_unsubscribe(const char* key){
  // Creates the unsubscription request to be sent to the server
  char request[MAX_STRING_SIZE + 2] = {'\0'};
  if(snprintf(request, MAX_STRING_SIZE + 2, "%d%s",
              OP_CODE_UNSUBSCRIBE, key) < 0){
    fprintf(stderr, "[API] Failed to create unsubscription request.\n");
    return 1;
  }

  // Writes unsubscription request to the request pipe
  if(write_all(REQ_FD, request, MAX_STRING_SIZE + 2) == -1){
    fprintf(stderr, "[API] Failed to write the unsubscription request to the request pipe.\n");
    if(errno == EPIPE){
      fprintf(stderr, "[API] Server connection lost.\n");
      return 2;
    }
    return 1;
  }

  // Reads the result of the unsubscription from the response pipe
  int io_result;
  char result[2] = {'\0'};
  if((io_result = read_all(RESP_FD, result, 2, NULL)) <= 0){
    fprintf(stderr, "[API] Failed to read the unsubscription result from the validation pipe.\n");
    if(io_result == 0)
      fprintf(stderr, "[API] Server connection lost.\n");
    return 2;
  }

  // Prints the result of the operation
  char response[49] = {'\0'};
  if(snprintf(response, 49,
     "Server returned %c for operation: unsubscribe.\n", result[1]) < 0){
    fprintf(stderr,"[API] Error in creating the message obtained form the server.\n");
    return 1;
  }
  write_all(1, response, 49);

  return result[1] - '0';
}


