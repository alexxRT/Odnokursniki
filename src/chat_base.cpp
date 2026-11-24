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

uint64_t hash_djb2_data(void* data, size_t size) {
    char* data_hash = (char*)data;
    uint64_t hash = 5381;

    size_t count = 0;
    while (count < size) {
        hash = ((hash << 5) + hash) + (int)data_hash[count];
        count ++;
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
    FREE(base->base);
    FREE(base);

    return;
}


ERR_STAT registr_client(chat_base_t* base, chat_session_t* session, const char* log_in_buf) {
    char usr_name[NAME_SIZE + 1] = {0};
    char password[PSWD_SIZE + 1] = {0};

    strncpy(usr_name, log_in_buf,  NAME_SIZE);
    strncpy(password, log_in_buf + NAME_SIZE, PSWD_SIZE);

    fprintf(stderr, "[INFO] Regitering new client; session_id=[%llu]\n", session->client_id);

    base_client_t* client = get_client(base, usr_name);

    //if client allready exists with this name
    if (client) {
        fprintf(stderr, "[ERROR] Username is taken; session_id=[%llu]\n", session->client_id);
        return ERR_STAT::NAME_TAKEN;
    }

    //if base capacity is not enough to fit new user
    if (base->size >= base->max_size) {
        fprintf(stderr, "[ERROR] User database max capacity reached; session_id=[%llu]\n", session->client_id);
        return ERR_STAT::UBASE_OVERFLOW;
    }

    //add new user
    base_client_t new_client = {};

    uint64_t name_hash = base->hash_client(usr_name);
    uint64_t pswd_hash = base->hash_client(password);
    session->client_name = name_hash;
    new_client.name_hash = name_hash;
    new_client.pswd_hash = pswd_hash;

    strncpy(new_client.name, usr_name, NAME_SIZE);

    base->base[base->size] = new_client;
    base->size ++;

    fprintf(stderr, "[INFO] New user added to ubase; session_id=[%llu]\n", session->client_id);
    return ERR_STAT::SUCCESS;
}

//when log in or log out: log_in_buf, log_out_buf contain only pswd and name hashes
ERR_STAT log_in_client(chat_base_t* base, chat_session_t* session, const char* authorization_buffer) {
    if (session->state == CLIENT_STATE::LOGGED_IN) {
        fprintf(stderr, "[WARNING] Client double log in attempt, no effect; session_id=[%llu]\n",
                            session->client_id);
        return ERR_STAT::SUCCESS;
    }

    char username[NAME_SIZE + 1] = {0};
    strncpy(username, authorization_buffer, NAME_SIZE);

    base_client_t* client = get_client(base, username);

    if (client && strncmp(client->name, username, NAME_SIZE) == 0) {
        char password[PSWD_SIZE + 1] = {0};
        strncpy(password, authorization_buffer + NAME_SIZE, PSWD_SIZE);
        uint64_t pswd_hash = base->hash_client(password);

        fprintf(stderr, "[INFO] Client found in database; session_id=[%llu]\n", session->client_id);
        fprintf(stderr, "[INFO] Validate client password; session_id=[%llu]\n", session->client_id);

        if (client->pswd_hash == pswd_hash) {
            session->client_name = client->name_hash; // attach client name to support search by name
            return ERR_STAT::SUCCESS;
        }
        else {
            return ERR_STAT::BAD_PSWD;
        }
    }

    fprintf(stderr, "[FATAL] Unable log in not regitered client; session_id=[%llu]\n", session->client_id);
    return ERR_STAT::BAD_LOGIN;
}

base_client_t* log_out_client(chat_base_t* base, const char* log_out_buf) {
    char usr_name[NAME_SIZE + 1] = {0};

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