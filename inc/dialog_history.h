#ifndef DIALOG_HISTORY_H
#define DIALOG_HISTORY_H

#include "server.h"

FILE* open_dialog_history(chat_base_t* base);
bool save_dialog_account(FILE* file, base_client_t* account, const char password[PSWD_SIZE]);
bool save_dialog_message(FILE* file, const chat_message_t* message);
bool restore_dialog_history(server_t* server, chat_session_t* session);

#endif
