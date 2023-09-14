#include "chat_base.h"
#include "chat_configs.h"
#include "uv.h"


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

    base->base = CALLOC(base_size, base_client_t);
    base->hash_client = hash_djb2;

    for (size_t i = 0; i < base_size; i ++) {
        base->base[i].client_stream = NULL;
        base->base[i].status        = STATUS::OFFLINE;
        base->base[i].name_hash = 0;
        base->base[i].pswd_hash = 0;
    }

    return base;
}

void destoy_client_base(chat_base_t* base) {
    for (size_t i = 0; i < base->size; i ++)
        if (base->base[i].status == STATUS::ONLINE)//only online users have unreleased stream handlers
            FREE(base->base[i].client_stream);

    FREE(base->base);
    FREE(base);

    return;
}

void change_status(base_client_t* client) {
    if (client->status == STATUS::ONLINE)
        client->status = STATUS::OFFLINE;
    else 
        client->status = STATUS::ONLINE;

    return;
}

int look_up_client(chat_base_t* base, uint64_t name_hash) {
    for (size_t i = 0; i < base->size; i ++) {
        if (base->base[i].name_hash == name_hash)
            return i;
    }

    return -1; 
}

base_client_t* registr_client(chat_base_t* base, uv_stream_t* endpoint, const char* log_in_buf) {
    char usr_name[NAME_SIZE] = {0};
    char password[PSWD_SIZE] = {0};

    strncpy(usr_name, log_in_buf,  NAME_SIZE);
    strncpy(password, log_in_buf + NAME_SIZE, NAME_SIZE);

    uint64_t name_hash = base->hash_client(*(unsigned char**)&usr_name);
    uint64_t pswd_hash = base->hash_client(*(unsigned char**)&password);

    base_client_t* client = get_client(base, name_hash);

    //if client allready exists with this name
    if (client)
        return NULL;

    //if base capacity is not enough to fit new user
    if (base->size >= base->max_size)
        return NULL;

    //add new user
    base_client_t new_client = {};
    new_client.client_stream = endpoint;
    new_client.name_hash = name_hash;
    new_client.pswd_hash = pswd_hash;

    base->base[base->size] = new_client;
    base->size ++;

    return client;
}

//when log in or log out: log_in_buf, log_out_buf contain only pswd and name hashes
base_client_t* log_in_client(chat_base_t* base, const char* log_in_buf) {
    char usr_name[NAME_SIZE] = {0};
    char password[PSWD_SIZE] = {0};

    strncpy(usr_name, log_in_buf,  NAME_SIZE);
    strncpy(password, log_in_buf + NAME_SIZE, NAME_SIZE);

    uint64_t name_hash = base->hash_client(*(unsigned char**)&usr_name);
    uint64_t pswd_hash = base->hash_client(*(unsigned char**)&password);

    base_client_t* client = get_client(base, name_hash);

    //client found
    if (client) {
        //client entered//
        if (client->pswd_hash == pswd_hash)
            return client;
        else
            return NULL;
    }

    //client does not exist
    return NULL;
}

base_client_t* log_out_client(chat_base_t* base, const char* log_out_buf) {
    char usr_name[NAME_SIZE] = {0};

    strncpy(usr_name, log_out_buf, NAME_SIZE);

    uint64_t name_hash = base->hash_client(*(unsigned char**)&usr_name);
    base_client_t* client = get_client(base, name_hash);

    //client found
    if (client)
        return client;

    //client does not exist
    return NULL;
}