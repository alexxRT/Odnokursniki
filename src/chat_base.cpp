#include "chat_base.h"
#include "chat_configs.h"
#include "uv.h"
#include <assert.h>


uint64_t hash_djb2(const char sample[]) //Dan Bernstein hash//
{
    uint64_t hash = 5381;
    char c = *sample;

    while (c) {
        hash = ((hash << 5) + hash) + (int)c; /* hash * 33 + c */
        c = *(++sample);
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
    //it should be managed by networking thread on close connection

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

base_client_t* registr_client(chat_base_t* base, uv_stream_t* endpoint, const char* log_in_buf) {
    char usr_name[NAME_SIZE] = {0};
    char password[PSWD_SIZE] = {0};

    strncpy(usr_name, log_in_buf,  NAME_SIZE);
    strncpy(password, log_in_buf + NAME_SIZE, PSWD_SIZE);

    // fprintf(stderr, "REGISTERING ...\n");
    fprintf(stderr, "name [%s]\n", usr_name);
    fprintf(stderr, "usr_pswd [%s]\n", password);

    base_client_t* client = get_client(base, usr_name);

    //if client allready exists with this name
    if (client)
        return NULL;

    //if base capacity is not enough to fit new user
    if (base->size >= base->max_size)
        return NULL;

    //add new user
    base_client_t new_client = {};
    new_client.client_stream = endpoint;

    uint64_t name_hash = base->hash_client(usr_name);
    uint64_t pswd_hash = base->hash_client(password);
    new_client.name_hash = name_hash;
    new_client.pswd_hash = pswd_hash;
    
    strncpy(new_client.name, usr_name, NAME_SIZE);

    base_client_t* new_client_addr = base->base + base->size;
    base->base[base->size] = new_client;
    base->size ++;

    return new_client_addr;
}

//when log in or log out: log_in_buf, log_out_buf contain only pswd and name hashes
base_client_t* log_in_client(chat_base_t* base, const char* log_in_buf) {
    char usr_name[NAME_SIZE] = {0};
    char password[PSWD_SIZE] = {0};

    strncpy(usr_name, log_in_buf,  NAME_SIZE);
    strncpy(password, log_in_buf + NAME_SIZE, PSWD_SIZE);

    uint64_t pswd_hash = base->hash_client(password);

    base_client_t* client = get_client(base, usr_name);

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

    base_client_t* client = get_client(base, usr_name);

    //client found
    if (client)
        return client;

    //client does not exist
    return NULL;
}

// //TO DO:   
// int look_up_client(chat_base_t* base, uv_stream_t* endpoint) {
//     assert(base);
//     assert(endpoint);

//     for (size_t i = 0; i < base->size; i ++) {
//         if (endpoint == base->base[i].client_stream)
//             return 1;
//     }

//     return -1;
// };

// int look_up_client(chat_base_t* base, char* name) {
//     assert(base);
//     assert(name);

//     uint64_t name_hash = base->hash_client(name);

//     for (size_t i = 0; i < base->size; i ++) {
//         if (name_hash == base->base[i].name_hash)
//             return i;
//     }
    
//     return -1;
// };

// int look_up_client(chat_base_t* base, uint64_t name_hash) {
//     assert(base);

//     for (size_t i = 0; i < base->size; i ++) {
//         if (name_hash == base->base[i].name_hash)
//             return i;
//     }
    
//     return -1;
// };

base_client_t* get_client(chat_base_t* base, uint64_t name_hash) {
    assert(base);

    for (size_t i = 0; i < base->size; i ++) {
        if (name_hash == base->base[i].name_hash)
            return base->base + i;
    }

    return NULL;
};

base_client_t* get_client(chat_base_t* base, char* name) {
    assert(base);
    assert(name);

    uint64_t name_hash = base->hash_client(name);

    for (size_t i = 0; i < base->size; i ++) {
        if (name_hash == base->base[i].name_hash)
            return base->base + i;
    }

    return NULL;
};
base_client_t* get_client(chat_base_t* base, uv_stream_t* endpoint) {
    assert(base);
    assert(endpoint);

    for (size_t i = 0; i < base->size; i ++) {
        if (endpoint == base->base[i].client_stream)
            return base->base + i;
    }

    return NULL;
};