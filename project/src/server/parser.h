#ifndef KVS_PARSER_H
#define KVS_PARSER_H

#include <stddef.h>
#include "constants.h"

enum Command {
  CMD_WRITE,
  CMD_READ,
  CMD_DELETE,
  CMD_SHOW,
  CMD_WAIT,
  CMD_BACKUP,
  CMD_HELP,
  CMD_EMPTY,
  CMD_INVALID,
  EOC  // End of commands
};

/// Reads a line and returns the corresponding command.
/// @param fd File descriptor to read from.
/// @return The command read.
enum Command get_next(int fd);

/// Parses a WRITE command.
/// @param fd File descriptor to read from.
/// @param keys Array of keys to be written.
/// @param values Array of values to be written.
/// @param max_pairs number of pairs to be written.
/// @param max_string_size maximum size for keys and values.
/// @return 0 if the command was parsed successfully, 1 otherwise.
size_t parse_write(int fd, char keys[][MAX_STRING_SIZE], char values[][MAX_STRING_SIZE], size_t max_pairs, size_t max_string_size);

/// Parses a READ or DELETE command.
/// @param fd File descriptor to read from.
/// @param keys Array of keys to be written.
/// @param max_keys number of keys to be iread or deleted.
/// @param max_string_size maximum size for keys and values.
/// @return Number of keys read or deleted. 0 on failure.
size_t parse_read_delete(int fd, char keys[][MAX_STRING_SIZE], size_t max_keys, size_t max_string_size);

/// Parses a WAIT command.
/// @param fd File descriptor to read from.
/// @param delay Pointer to the variable to store the wait delay in.
/// @param thread_id Pointer to the variable to store the thread ID in. May not be set.
/// @return 0 if no thread was specified, 1 if a thread was specified, -1 on error.
int parse_wait(int fd, unsigned int *delay, unsigned int *thread_id);

#endif  // KVS_PARSER_H
