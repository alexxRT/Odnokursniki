#ifndef SENDER_H
#define SENDER_H

#include "client.h"
#include "server.h"
#include "chat_configs.h"


ERR_STAT send_message(server_t* server, chat_session_t* session, chat_message_t* msg);
ERR_STAT broadcast_message(server_t* server, chat_message_t* msg);

ERR_STAT send_message(client_t* client, chat_message_t* msg);

ERR_STAT send_message_login(client_t* client, const char username[NAME_SIZE],
                                              const char password[PSWD_SIZE]);
ERR_STAT send_message_register(client_t* client, const char username[NAME_SIZE],
                                                 const char password[PSWD_SIZE]);
ERR_STAT send_message_text(client_t* client, const char addressant[NAME_SIZE],
                                             const char message_txt[MSG_SIZE]);


#endif //SENDER_H