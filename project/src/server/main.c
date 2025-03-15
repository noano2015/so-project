/**
 * @file main.c
 * 
 * @author Pedro Vicente (ist1109852), Pedro Jerónimo (ist1110375)
 * 
 * @brief The main file of the server part to operate the KVS Table.
 * Receives four arguments, the file folder where are the .job files,
 * the number of backups which are permited to do simultaneously, the
 * maximum number of threads that can be used and the name of the pipe
 * which the clients will connect to, in this specific order.
 * 
 * The server obtais the specified .job files, executes the commands
 * that are in those files and writes the .out files with the output 
 * of the commands (and .bck if it there is a BACKUP command).
 * 
 * The server also send notifications to the connected clients about
 * the updates/modifications of the keys which they subscribed to.
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "constants.h"
#include "parser.h"
#include "operations.h"
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include "../common/constants.h"
#include "../common/protocol.h"
#include "../common/io.h"
#include <errno.h>
#include <semaphore.h>
#include <bits/sigaction.h>
#include <bits/types/sigset_t.h>

// Locks to avoid file descriptors from being closed twice
pthread_mutex_t FDS_LOCKS[MAX_SESSION_COUNT];

// A Buffer that stores the pipes paths of the clients wishing to establish connection
char BUFFER_SESSION_PATHS[MAX_SESSION_COUNT][3*MAX_PIPE_PATH_LENGTH];

// A Buffer with the file descriptors of the connected clients pipes
// 0 - Request pipe
// 1 - Response pipe
// 2 - Notifications pipe
int BUFFER_SESSION_FDS[MAX_SESSION_COUNT][3];

unsigned int MAX_BACKUPS, ACTIVE_BACKUPS = 0, CLOSED = 0;
unsigned int SIGUSR1_RECEIVED = 0; // To verify if there is an signal routine in course

// backup_lock - To prevent the kvs table from being modified while doing a bakup
// dir_lock - To prevent two job threads from reading the same .job file
// session_lock - To prevent two session threads from connecting to the same client
pthread_mutex_t backup_lock, dir_lock, session_lock;

// To synchronize the manager thread and the session threads
sem_t read_sessions_sem, write_sessions_sem;
size_t read_i = 0;

// To unlink the the fifo if the server is closed by a signal
char fifo_name[MAX_PIPE_PATH_LENGTH] = {'\0'};

// Struct with the job info
typedef struct{
  char* dir_path;
  DIR* dir;
} jobInfo;

// CLOSE FIFOS OF CLIENTS //

void close_session_fifos(int session_id){
  pthread_mutex_lock(&(FDS_LOCKS[session_id]));

  if(BUFFER_SESSION_FDS[session_id][0] != -1){
    close(BUFFER_SESSION_FDS[session_id][0]);
    BUFFER_SESSION_FDS[session_id][0] = -1;
  }
  if(BUFFER_SESSION_FDS[session_id][1] != -1){
    close(BUFFER_SESSION_FDS[session_id][1]);
    BUFFER_SESSION_FDS[session_id][1] = -1;
  }
  if(BUFFER_SESSION_FDS[session_id][2] != -1){
    close(BUFFER_SESSION_FDS[session_id][2]);
    BUFFER_SESSION_FDS[session_id][2] = -1;
  }

  pthread_mutex_unlock(&(FDS_LOCKS[session_id]));
}

// FREES ALL THE LOCKS AND SEMAPHORES //

void destroy_and_clean(){
  kvs_terminate();
  pthread_mutex_destroy(&backup_lock);
  pthread_mutex_destroy(&session_lock);
  pthread_mutex_destroy(&dir_lock);
  for(int i = 0; i < MAX_SESSION_COUNT; i++)
    pthread_mutex_destroy(&FDS_LOCKS[i]);
  sem_destroy(&read_sessions_sem);
  sem_destroy(&write_sessions_sem);
}

// HANDLE SIGNALS //

void handle_signal(int sig){
  if(sig == SIGUSR1){
    // Indicates the execution of a SIGUSR1 routine
    SIGUSR1_RECEIVED = 1;

    if(signal(SIGUSR1, handle_signal) == SIG_ERR)
      fprintf(stderr, "[SIGNAL HANDLER] Failed to create a rotine to handle SIGUSR1.\n");
  }
}

// READ JOBS //

void* read_job(void* arg){
  // Blocks SIGUSR1 in this thread
  sigset_t sigset1;
  sigemptyset(&sigset1);
  sigaddset(&sigset1, SIGUSR1);
  if(pthread_sigmask(SIG_BLOCK, &sigset1, NULL) != 0){
    fprintf(stderr,"[JOB THREAD] Failed to mask SIGURSR1.\n");
    pthread_exit(NULL);
  }

  // Block SIGPIPE in this thread
  sigset_t sigset2;
  sigemptyset(&sigset2);
  sigaddset(&sigset2, SIGPIPE);
  if(pthread_sigmask(SIG_BLOCK, &sigset2, NULL) != 0){
    fprintf(stderr,"[JOB THREAD] Failed to mask SIGPIPE.\n");
    pthread_exit(NULL);
  }

  jobInfo* info = (jobInfo*) arg;
  unsigned int current_backup = 1;
  int input_file, output_file;
  struct dirent* entry;

  // Browse files
  pthread_mutex_lock(&dir_lock);
  while((entry = readdir(info->dir)) != NULL){
    pthread_mutex_unlock(&dir_lock);

    int done = 0;
    char input_path[MAX_JOB_FILE_NAME_SIZE];
    char output_path[MAX_JOB_FILE_NAME_SIZE];

    // Skip "." and ".."
    if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
      pthread_mutex_lock(&dir_lock);
      continue;
    }

    // Check if the file type is .job
    char extension[5];
    strncpy(extension, entry->d_name + (strlen(entry->d_name) - 4), (size_t) 4);
    if(strncmp(extension, ".job", 4)){
      fprintf(stderr, "[JOB THREAD] Wrong type of file.\n");
      pthread_mutex_lock(&dir_lock);
      continue;
    }

    // Get the input file path
    if(snprintf(input_path, MAX_JOB_FILE_NAME_SIZE, "%s/%s",
                info->dir_path, entry->d_name) >= MAX_JOB_FILE_NAME_SIZE){
      fprintf(stderr, "[JOB THREAD] Input path size exceeded.\n");
    }

    // Get the output file name
    char output_name[MAX_STRING_SIZE];
    strncpy(output_name, entry->d_name, strlen(entry->d_name) - 4);
    output_name[strlen(entry->d_name) - 4] = '\0';

    // Get the output file path
    if(snprintf(output_path, MAX_JOB_FILE_NAME_SIZE,"%s/%s.out",
                info->dir_path, output_name) >= MAX_JOB_FILE_NAME_SIZE){
      fprintf(stderr, "[JOB THREAD] Output path size exceeded.\n");
    }

    // Open the input file
    input_file = open(input_path, O_RDONLY);
    if(input_file < 0){
      fprintf(stderr, "[JOB THREAD] Error opening input file %s\n", input_path);
      pthread_mutex_lock(&dir_lock);
      continue;
    }

    // Open the output file
    output_file = open(output_path, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
    if(output_file < 0){
      fprintf(stderr, "[JOB THREAD] Error opening output file %s\n", output_path);
      close(input_file);
      pthread_mutex_lock(&dir_lock);
      continue;
    }

    // Execute file commands
    while(!done){
      char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
      char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
      unsigned int delay;
      size_t num_pairs;

      switch (get_next(input_file)){
        case CMD_WRITE:
          num_pairs = parse_write(input_file, keys, values, MAX_WRITE_SIZE, MAX_STRING_SIZE);
          
          if(num_pairs == 0){
            fprintf(stderr, "[JOB THREAD] Invalid command. See HELP for usage.\n");
            continue;
          }

          if(kvs_write(num_pairs, keys, values)){
            fprintf(stderr, "[JOB THREAD] Failed to write pair.\n");
          }

          break;

        case CMD_READ:
          num_pairs = parse_read_delete(input_file, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

          if(num_pairs == 0){
            fprintf(stderr, "[JOB THREAD] Invalid command. See HELP for usage.\n");
            continue;
          }

          if(kvs_read(num_pairs, keys, output_file)){
            fprintf(stderr, "[JOB THREAD] Failed to read pair.\n");
          }

          break;

        case CMD_DELETE:
          num_pairs = parse_read_delete(input_file, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

          if (num_pairs == 0){
            fprintf(stderr, "[JOB THREAD] Invalid command. See HELP for usage.\n");
            continue;
          }

          if(kvs_delete(num_pairs, keys, output_file)){
            fprintf(stderr, "[JOB THREAD] Failed to delete pair.\n");
          }
          
          break;

        case CMD_SHOW:
          kvs_show(output_file);

          break;

        case CMD_WAIT:
          if(parse_wait(input_file, &delay, NULL) == -1){
            fprintf(stderr, "[JOB THREAD] Invalid command. See HELP for usage.\n");
            continue;
          }

          if(delay >0) {
              write_all(output_file, "Waiting..\n", MAX_WAIT_STRING);
              kvs_wait(delay);
          }
          
          break;

        case CMD_BACKUP:
          ;// Get the backup path
          char backup_path[MAX_JOB_FILE_NAME_SIZE];
          char aux[MAX_JOB_FILE_NAME_SIZE];
          strncpy(aux, entry->d_name, strlen(entry->d_name) - 4);
          aux[strlen(entry->d_name) - 4] = '\0';
          if(snprintf(backup_path, MAX_JOB_FILE_NAME_SIZE, 
                      "%s/%s-%d.bck", info->dir_path, aux,
                      current_backup) >= MAX_JOB_FILE_NAME_SIZE){
            fprintf(stderr, "[JOB THREAD] Backup path size exceeded.\n");
            continue;
          }

          // Check if it is possible to do a backup at the moment
          pthread_mutex_lock(&backup_lock);
          if(ACTIVE_BACKUPS == MAX_BACKUPS){
            wait(NULL);
            ACTIVE_BACKUPS--;
          }

          // Make a non-blocking backup
          kvs_read_lock();
          int pid = fork();
          if(pid < 0){
            fprintf(stderr, "[JOB THREAD] Error in forking the process.\n");
          }else if(pid == 0){
            if(kvs_backup(backup_path))
              fprintf(stderr, "[JOB THREAD] Failed to perform backup.\n");

            close(input_file);
            close(output_file);
            closedir(info->dir);
            kvs_terminate();
            pthread_mutex_destroy(&backup_lock);
            pthread_mutex_destroy(&dir_lock);
            pthread_mutex_destroy(&session_lock);
            exit(0);
          }else{
            ACTIVE_BACKUPS++;
            pthread_mutex_unlock(&backup_lock);
            current_backup++;
            kvs_unlock();
          }

          break;

        case CMD_INVALID:
          fprintf(stderr, "[JOB THREAD] Invalid command. See HELP for usage.\n");
          
          break;

        case CMD_HELP:
          write_all(output_file, 
                "Available commands:\n"
                "  WRITE [(key,value)(key2,value2),...]\n"
                "  READ [key,key2,...]\n"
                "  DELETE [key,key2,...]\n"
                "  SHOW\n"
                "  WAIT <delay_ms>\n"
                "  BACKUP\n"
                "  HELP\n",
                MAX_HELP_STRING
          );

          break;
              
        case CMD_EMPTY:
          break;

        case EOC:
          close(input_file);
          close(output_file);
          done = 1;
          pthread_mutex_lock(&dir_lock);

          break;
      }

    }
    current_backup = 1;
  }

  pthread_mutex_unlock(&dir_lock);
  pthread_exit(NULL);
}


// READ SESSIONS //

void* read_session(void* args){
  // Blocks SIGUSR1 in this thread
  sigset_t sigset1;
  sigemptyset(&sigset1);
  sigaddset(&sigset1, SIGUSR1);
  if(pthread_sigmask(SIG_BLOCK, &sigset1, NULL) != 0){
    fprintf(stderr,"[SESSION THREAD] Failed to mask SIGURSR1 in a session thread.\n");
    pthread_exit(NULL);
  }

  // Block SIGPIPE in this thread
  sigset_t sigset2;
  sigemptyset(&sigset2);
  sigaddset(&sigset2, SIGPIPE);
  if(pthread_sigmask(SIG_BLOCK, &sigset2, NULL) != 0){
    fprintf(stderr,"[SESSION THREAD] Failed to mask SIGPIPE in a session thread.\n");
    pthread_exit(NULL);
  }

  int session_id = *((int*) args);
  char req_path[MAX_PIPE_PATH_LENGTH] = {'\0'}, resp_path[MAX_PIPE_PATH_LENGTH] = {'\0'}, 
       notif_path[MAX_PIPE_PATH_LENGTH] = {'\0'}, command[1] = {'\0'}, 
       key[MAX_STRING_SIZE + 1] = {'\0'}, response[2] = {'\0'};
  int done, req_fd, resp_fd, notif_fd, io_result;

  while(!CLOSED){

    // Reads a connection request
    sem_wait(&read_sessions_sem);
    pthread_mutex_lock(&session_lock);

    strncpy(req_path,
            BUFFER_SESSION_PATHS[read_i], MAX_PIPE_PATH_LENGTH);

    strncpy(resp_path,
            BUFFER_SESSION_PATHS[read_i] + MAX_PIPE_PATH_LENGTH, MAX_PIPE_PATH_LENGTH); 

    strncpy(notif_path,
            BUFFER_SESSION_PATHS[read_i] + 2*MAX_PIPE_PATH_LENGTH, MAX_PIPE_PATH_LENGTH);

    read_i = (read_i + 1) % MAX_SESSION_COUNT;

    pthread_mutex_unlock(&session_lock);
    sem_post(&write_sessions_sem);

    char id[MAX_STRING_SIZE - 8];
    strncpy(id, req_path + 8, MAX_STRING_SIZE - 8);

    response[0] = OP_CODE_CONNECT + '0';
    // Opens the response pipe from the client
    if((resp_fd = open(resp_path, O_WRONLY)) < 0){
      fprintf(stderr, "[SESSION THREAD] Failed to open the client %s response pipe.\n", id);
      continue;
    }

    // Opens the request pipe from the client
    if((req_fd = open(req_path, O_RDONLY)) < 0){
      fprintf(stderr, "[SESSION THREAD] Failed to open the client %s request pipe.\n", id);
      response[1] = '1';
      if(write_all(resp_fd, response, 2) == -1)
        fprintf(stderr, "[SESSION THREAD] Failed to write the connection result to the client %s response pipe.\n", id);
      close(resp_fd);
      continue;
    }
    
    // Opens the notifications pipe from the cliente
    if((notif_fd = open(notif_path, O_WRONLY)) < 0){
      fprintf(stderr, "[SESSION THREAD] Failed to open the client %s notifications pipe.\n", id);
      response[1] = '1';
      if(write_all(resp_fd, response, 2) == -1)
        fprintf(stderr, "[SESSION THREAD] Failed to write the connection result to the client %s response pipe.\n", id);
      close(resp_fd);
      close(req_fd);
      continue;
    }

    response[1] = '0';
    
    // Writes the connection result to the client response pipe
    if(write_all(resp_fd, response, 2) == -1){
      fprintf(stderr, "[SESSION THREAD] Failed to write the connection result to the client %s response pipe.\n", id);
      close(req_fd);
      close(resp_fd);
      close(notif_fd);
      continue;
    }

    // In order to stop the SIGUSR1 handler from closing the file descriptors
    // before they are updated
    pthread_mutex_lock(&(FDS_LOCKS[session_id]));

    BUFFER_SESSION_FDS[session_id][0] = req_fd;
    BUFFER_SESSION_FDS[session_id][1] = resp_fd;
    BUFFER_SESSION_FDS[session_id][2] = notif_fd;

    pthread_mutex_unlock(&(FDS_LOCKS[session_id]));

    char connection_message[29 + MAX_STRING_SIZE] = {'\0'};
    snprintf(connection_message, 29 + MAX_STRING_SIZE, "[SESSION THREAD] Connected client %s.\n",id);
    write_all(1, connection_message, strlen(connection_message));

    done = 0;
    while(!done){
      if((io_result = read_all(req_fd, command, 1, NULL)) == 0 
         || (io_result == -1 && errno == EBADF))
        break;
      else if(io_result == -1){
        fprintf(stderr, "[SESSION THREAD] Failed to read a session command of the client %s.\n", id);
        continue;
      }

      response[0] = command[0];
      switch(command[0] - '0'){
        case OP_CODE_DISCONNECT:
          done = 1;
              
          response[1] = '0';
          if(write_all(resp_fd, response, 2) == -1)
            fprintf(stderr, "[SESSION THREAD] Failed to write the disconnection response to the client %s response pipe.\n", id);

          break;

        case OP_CODE_SUBSCRIBE:
          if((io_result = read_all(req_fd, key, MAX_STRING_SIZE, NULL)) <= 0){
            fprintf(stderr, "[SESSION THREAD] Failed to read the key to be subscribed of client %s.\n", id);
            done = 1;
            continue;
          }

          if(kvs_subscribe(notif_fd, key))
            response[1] = '0';
          else{
            response[1] = '1';
          }

          if(write_all(resp_fd, response, 2) == -1){
            fprintf(stderr, "[SESSION THREAD] Failed to write the subscribing response to the client %s response pipe.\n", id);
            done = 1;
            continue;
          }

          break;

        case OP_CODE_UNSUBSCRIBE:
          if((io_result = read_all(req_fd, key, MAX_STRING_SIZE, NULL)) <= 0){
            fprintf(stderr, "[SESSION THREAD] Failed to read the key to be unsubscribed of the client %s.\n", id);
            done = 1;
            continue;
          }

          if(kvs_unsubscribe(notif_fd, key))
            response[1] = '1';
          else{
            response[1] = '0';
          }

          if(write_all(resp_fd, response, 2) == -1){
            fprintf(stderr, "[SESSION THREAD] Failed to write the unsubscribing response to the client %s response pipe.\n", id);
            done = 1;
            continue;
          }

          break;
      }
    }

    char disconnection_message[32 + MAX_STRING_SIZE] = {'\0'};
    snprintf(disconnection_message, 32 + MAX_STRING_SIZE, "[SESSION THREAD] Disconnected client %s.\n",id);
    write_all(1, disconnection_message, strlen(disconnection_message));

    unsubscribe_fifo(notif_fd);
    close_session_fifos(session_id);
  }

  pthread_exit(NULL);
}

int main(int argc, char**argv){
  // Check if the number of arguments is correct
  if(argc != 5){
    fprintf(stderr,"Invalid number of arguments.\n");
    return 1;
  }

  // Deletes the server pipe if it already exists
  if(unlink(argv[4]) != 0 && errno != ENOENT){
    fprintf(stderr, "Unlink(%s) failed.\n", argv[4]);
    return 1;
  }
  
  // Create and open server pipe
  if(mkfifo(argv[4], 0640) != 0){
    fprintf(stderr, "Failed to create server pipe.\n");
    return 1;
  }
  
  char *endptr1, *endptr2;
  MAX_BACKUPS = (unsigned int) strtoul(argv[2], &endptr1, 10);
  unsigned int max_jobs = (unsigned int) strtoul(argv[3], &endptr2, 10);

  // Check if the conversions were successful
  if(*endptr1 != '\0'){
    fprintf(stderr,"Conversion error, non-numeric characters found: %s.\n", endptr1);
    return 1;
  }
  if(*endptr2 != '\0'){
    fprintf(stderr,"Conversion error, non-numeric characters found: %s.\n", endptr2);
    return 1;
  }

  pthread_t jobs_ids[max_jobs];
  pthread_t sessions_ids[MAX_SESSION_COUNT];

  // Inicialize the kvs hashtable
  if(kvs_init()){
    fprintf(stderr, "Failed to initialize KVS.\n");
    return 1;
  }

  if(pthread_mutex_init(&backup_lock, NULL) < 0){
    fprintf(stderr, "Failed to initialize the backups lock.\n");
    kvs_terminate();
    return 1;
  }

  if(pthread_mutex_init(&dir_lock, NULL) < 0){
    fprintf(stderr, "Failed to initialize the directory lock.\n");
    kvs_terminate();
    pthread_mutex_destroy(&backup_lock);
    return 1;
  }

  if(pthread_mutex_init(&session_lock, NULL) < 0){
    fprintf(stderr, "Failed to initialize the session mutex.\n");
    kvs_terminate();
    pthread_mutex_destroy(&backup_lock);
    pthread_mutex_destroy(&dir_lock);
    return 1;
  }

  for(int i = 0; i < MAX_SESSION_COUNT; i++)
    if(pthread_mutex_init(&(FDS_LOCKS[i]), NULL) < 0){
      fprintf(stderr, "Failed to initialize the file descriptors lock.\n");
      kvs_terminate();
      pthread_mutex_destroy(&backup_lock);
      pthread_mutex_destroy(&dir_lock);
      pthread_mutex_destroy(&session_lock);
      return 1;
    }

  if(sem_init(&read_sessions_sem, 0, 0) <0){
    fprintf(stderr, "Failed to initialize the read semaphore.\n");
    kvs_terminate();
    pthread_mutex_destroy(&backup_lock);
    pthread_mutex_destroy(&dir_lock);
    pthread_mutex_destroy(&session_lock);
    for(int i = 0; i < MAX_SESSION_COUNT; i++)
      pthread_mutex_destroy(&(FDS_LOCKS[i]));
    return 1;
  }

  if(sem_init(&write_sessions_sem, 0, MAX_SESSION_COUNT) < 0){
    fprintf(stderr, "Failed to initialize the write semaphore.\n");
    kvs_terminate();
    pthread_mutex_destroy(&backup_lock);
    pthread_mutex_destroy(&dir_lock);
    pthread_mutex_destroy(&session_lock);
    for(int i = 0; i < MAX_SESSION_COUNT; i++)
      pthread_mutex_destroy(&(FDS_LOCKS[i]));
    sem_destroy(&read_sessions_sem);
    return 1;
  }

  // Open the given directory
  DIR* dir = opendir(argv[1]);

  // Checks if the directory was opened correctly
  if(!dir){
    fprintf(stderr, "Failed to open the directory.\n");
    destroy_and_clean();
    return 1;

  }else{
    jobInfo tinfo;
    tinfo.dir_path = argv[1];
    tinfo.dir = dir;

    // Create jobs threads
    for(int i = 0; i < (int) max_jobs; i++){
      if(pthread_create(&(jobs_ids[i]), NULL, read_job, (void*) &(tinfo)) < 0){
        fprintf(stderr, "Failed to create a job thread.\n");
        destroy_and_clean();
        closedir(dir);
        return 1;
      }
    }

    // Resets all the fds of the sessions threads in order to stop the server
    for(int i = 0; i < MAX_SESSION_COUNT; i++){
      BUFFER_SESSION_FDS[i][0] = -1;
      BUFFER_SESSION_FDS[i][1] = -1;
      BUFFER_SESSION_FDS[i][2] = -1;
    }
    
    // Create sessions threads
    int session_ids[MAX_SESSION_COUNT];
    for(int i = 0; i < MAX_SESSION_COUNT; i++){
      session_ids[i] = i;
      if(pthread_create(&(sessions_ids[i]), NULL, read_session, &(session_ids[i])) < 0){
        fprintf(stderr, "Failed to create a session thread.\n");
        destroy_and_clean();
        closedir(dir);
        return 1;
      }
    }

    // Prepare to handle SIGUSR1
    if(signal(SIGUSR1, handle_signal) == SIG_ERR){
      fprintf(stderr, "Failed to create a routine to handle SIGUSR1.\n");
      destroy_and_clean();
      closedir(dir);
      return 1;
    }

    // Read connection requests
    size_t write_i = 0;
    int server_fd, io_result;
    char connection_request[1 + 3*MAX_PIPE_PATH_LENGTH];

    // Open server pipe
    while((server_fd = open(argv[4], O_RDONLY)) < 0){
      // Try again if open was interrupted by a signal
      if(errno == EINTR)
        continue;
      fprintf(stderr, "Failed to open the server pipe.\n");
      destroy_and_clean();
      closedir(dir);
      return 1;
    }
    
    while(!CLOSED){
      if(SIGUSR1_RECEIVED){
        SIGUSR1_RECEIVED = 0;

        write_all(1, "[HOST] SIGUSR1 received.\n", 26);

        // Verifies that all request, response and notification fifos are closed
        for(int i = 0; i < MAX_SESSION_COUNT; i++){
          close_session_fifos(i);
        }
        
        // Clears all subscriptions
        kvs_clear_subscriptions();
      }

      // Read connection requests from the server pipe
      sem_wait(&write_sessions_sem);
      if((io_result = read_all(server_fd, connection_request, 1 + 3*MAX_PIPE_PATH_LENGTH, NULL)) == 1){
        if(connection_request[0] != '1'){
          fprintf(stderr, "[HOST] Invalid command.\n"); 
          break;
        }

        strncpy(BUFFER_SESSION_PATHS[write_i], connection_request + 1, MAX_PIPE_PATH_LENGTH);
        strncpy(BUFFER_SESSION_PATHS[write_i] + MAX_PIPE_PATH_LENGTH, connection_request + 1 + MAX_PIPE_PATH_LENGTH, MAX_PIPE_PATH_LENGTH);
        strncpy(BUFFER_SESSION_PATHS[write_i] + 2*MAX_PIPE_PATH_LENGTH, connection_request + 1 + 2*MAX_PIPE_PATH_LENGTH, MAX_PIPE_PATH_LENGTH);
        write_i = (write_i + 1) % MAX_SESSION_COUNT;
        sem_post(&read_sessions_sem);
        continue;
      }
      if(io_result < 0){
        fprintf(stderr, "[HOST] Failed to read a connection request.\n");
      }
      sem_post(&write_sessions_sem);
    }

    // Closes the server
    close(server_fd);

    // Wait until all jobs finish
    for(int i = 0; i < (int) max_jobs; i++)
      if(pthread_join(jobs_ids[i], NULL) < 0){
        fprintf(stderr, "Failed to join a job thread.\n");
        destroy_and_clean();
        closedir(dir);
        return 1;
      }
      
    // Wait until all backups finish
    for(int i = 0; i < (int) ACTIVE_BACKUPS; i++)
      wait(NULL);

    closedir(dir);

    // Wait until all sessions finish
    for(int i = 0; i < MAX_SESSION_COUNT; i++)
      if(pthread_join(sessions_ids[i], NULL) < 0){
        fprintf(stderr, "failed to join a session thread.\n");
        destroy_and_clean();
        return 1;
      }
    
    //Destroys the locks and the semaphores
    destroy_and_clean();

    // Unlinks the server pipe
    unlink(argv[4]);

    return 0;
  }
}
