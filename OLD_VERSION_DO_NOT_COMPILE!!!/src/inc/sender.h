#ifndef SENDER_H
#define SENDER_H

#include "client.h"
#include "server.h"
#include "chat_configs.h"


ERR_STAT send_message(server_t* server, chat_message_t* msg);
ERR_STAT send_message(client_t* client, chat_message_t* msg);

void run_sender(server_t* server);
void run_sender(client_t* client);


#endif //SENDER_H