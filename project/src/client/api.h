/**
 * @file api.h
 * 
 * @author Pedro Vicente (ist1109852), Pedro Jerónimo (ist1110375)
 * 
 * @brief An api in order to process the information that will be sent
 * to the server by the client to execute its requets and receive the
 * server's response.
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef CLIENT_API_H
#define CLIENT_API_H

#include <stddef.h>
#include "src/common/constants.h"

/// Connects to a kvs server.
/// @param req_pipe_path Path to the name pipe to be created for requests.
/// @param resp_pipe_path Path to the name pipe to be created for responses.
/// @param server_pipe_path Path to the name pipe where the server is listening.
/// @return 0 if the connection was established successfully, 1 otherwise.
int kvs_connect(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path,
                char const* notif_pipe_path, int* notif_pipe);

/// Disconnects from an KVS server.
/// @return 0 in case of success, 1 if it failed but api can still be 
/// used, 2 if it failed and api is corrupted.
int kvs_disconnect(int force_closing);

/// Requests a subscription for a key.
/// @param key Key to be subscribed.
/// @return 0 if the key was subscribed successfully (key existed),
/// 1 if it failed but api can still be used, 2 if it failed and api 
/// is corrupted.
int kvs_subscribe(const char* key);

/// Remove a subscription for a key.
/// @param key Key to be unsubscribed.
/// @return 0 if the key was unsubscribed successfully 
/// (subscription existed), 1 if it failed but api can still be used, 
/// 2 if it failed and api is corrupted.
int kvs_unsubscribe(const char* key);
 
#endif  // CLIENT_API_H
