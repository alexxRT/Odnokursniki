#include "chat_base.h"


uint64_t hash_djb2(unsigned char *str) //Dan Bernstein hash//
{
    uint64_t hash = 5381;
    char c = *str;

    while (c) {
        hash = ((hash << 5) + hash) + (int)c; /* hash * 33 + c */
        c = *(++str);
    }
    
    return hash;
}


chat_base_t* create_client_base(size_t base_size) {
    chat_base_t* base = CALLOC(1, chat_base_t);

    base->size     = 0;
    base->max_size = base_size;

    base->base = CALLOC(base_size, chat_client_t);
    base->hash_client = hash_djb2;

    for (size_t i = 0; i < base_size; i ++) {
        base->base[i].client_stream = NULL;
        base->base[i].status        = OFFLINE;
        base->base[i].name_hash = 0;
        base->base[i].pswd_hash = 0;
    }

    return base;
}

void destoy_client_base(chat_base_t* base) {
    for (size_t i = 0; i < base->size; i ++)
        FREE(base->base[i].client_stream);

    FREE(base->base);
    FREE(base);

    return;
}

void change_status(chat_client_t* client) {
    if (client->status == ONLINE)
        client->status = OFFLINE;
    else 
        client->status = ONLINE;

    return;
}

int look_up_client(chat_base_t* base, uint64_t name_hash) {
    for (size_t i = 0; i < base->size; i ++) {
        if (base->base[i].name_hash == name_hash)
            return i;
    }

    return -1; 
}


bool add_new_client(chat_base_t* base, uv_tcp_t* stream, const char* log_in_buf) {
    char usr_name[NAME_SIZE] = {0};
    char password[PSWD_SIZE] = {0};

    strncpy(usr_name, log_in_buf,  NAME_SIZE);
    strncpy(password, log_in_buf + NAME_SIZE, NAME_SIZE);

    uint64_t name_hash = base->hash_client(*(unsigned char**)&usr_name);
    uint64_t pswd_hash = base->hash_client(*(unsigned char**)&password);

    int client_indx = look_up_client(base, name_hash);

    //if client allready exists with this name
    if (client_indx >= 0)
        return false;

    //if base capacity is not enough to fit new user
    if (base->size >= base->max_size)
        return false;

    //add new user
    chat_client_t client = {};
    client.client_stream = stream;
    client.name_hash = name_hash;
    client.pswd_hash = pswd_hash;
    change_status(&client);

    base->base[base->size] = client;
    base->size ++;

    return true;
}

bool log_in_client(chat_base_t* base, uv_tcp_t* stream, const char* log_in_buf) {
    char usr_name[NAME_SIZE] = {0};
    char password[PSWD_SIZE] = {0};

    strncpy(usr_name, log_in_buf,  NAME_SIZE);
    strncpy(password, log_in_buf + NAME_SIZE, NAME_SIZE);

    uint64_t name_hash = base->hash_client(*(unsigned char**)&usr_name);
    uint64_t pswd_hash = base->hash_client(*(unsigned char**)&password);

    int client_indx = look_up_client(base, name_hash);

    //client found
    if (client_indx >= 0) {
        if (base->base[client_indx].pswd_hash == pswd_hash) {
            change_status(&base->base[client_indx]);
            return true;
        }
        else
            return false;
    }

    //client does not exist
    return false;
}

bool log_out_client(chat_base_t* base, const char* log_out_buf) {
    char usr_name[NAME_SIZE] = {0};

    strncpy(usr_name, log_out_buf, NAME_SIZE);

    uint64_t name_hash = base->hash_client(*(unsigned char**)&usr_name);
    int client_indx = look_up_client(base, name_hash);

    //client found
    if (client_indx >= 0) {
        FREE(base->base[client_indx].client_stream);
        change_status(&base->base[client_indx]);
        return true;
    }

    //client does not exist
    return false;
}

int main () {

    return 0;
}