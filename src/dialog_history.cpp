#include "dialog_history.h"
#include "sender.h"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

// The published text format uses A "name" "salt" "verifier" and
// M "from" "to" "body". Inside quotes, backslash escapes the following byte.
typedef struct history_record {
    int kind;
    char fields[3][MSG_SIZE + 1];
} history_record_t;

static int read_record(FILE* file, history_record_t* record) {
    record->kind = fgetc(file);
    if (record->kind == EOF)
        return ferror(file) ? -1 : 0;
    if (record->kind != 'A' && record->kind != 'M')
        return -1;
    for (size_t i = 0; i < 3; i++) {
        if (fgetc(file) != ' ' || fgetc(file) != '"')
            return -1;
        size_t n = 0;
        int ch;
        while ((ch = fgetc(file)) != '"') {
            if (ch == '\\')
                ch = fgetc(file);
            if (ch == EOF || ch == 0 || n == MSG_SIZE)
                return -1;
            record->fields[i][n++] = (char)ch;
        }
        record->fields[i][n] = '\0';
    }
    return fgetc(file) == '\n' ? 1 : -1;
}

static bool append_record(FILE* file, int kind, const char* fields[3], const size_t limits[3]) {
    char line[3 * (2 * MSG_SIZE + 3) + 2];
    size_t n = 0;
    line[n++] = (char)kind;
    for (size_t i = 0; i < 3; i++) {
        line[n++] = ' ';
        line[n++] = '"';
        for (size_t j = 0; j < limits[i] && fields[i][j]; j++) {
            char ch = fields[i][j];
            if (ch == '\\' || ch == '"')
                line[n++] = '\\';
            line[n++] = ch;
        }
        line[n++] = '"';
    }
    line[n++] = '\n';

    if (ferror(file) || fseeko(file, 0, SEEK_END) != 0)
        return false;
    off_t start = ftello(file);
    if (start < 0)
        return false;
    if (fwrite(line, 1, n, file) == n && fflush(file) == 0 && fsync(fileno(file)) == 0)
        return true;
    if (ftruncate(fileno(file), start) == 0 && fsync(fileno(file)) == 0)
        clearerr(file);
    return false;
}

static void to_hex(const unsigned char* bytes, size_t n, char* result) {
    const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        result[2 * i] = digits[bytes[i] >> 4];
        result[2 * i + 1] = digits[bytes[i] & 15];
    }
    result[2 * n] = '\0';
}

static bool password_digest(const char* password, const char* salt, char digest[65]) {
    unsigned char bytes[32];
    if (!PKCS5_PBKDF2_HMAC(password, strnlen(password, PSWD_SIZE), (const unsigned char*)salt,
                           strlen(salt), 600000, EVP_sha256(), sizeof(bytes), bytes))
        return false;
    to_hex(bytes, sizeof(bytes), digest);
    return true;
}

static bool verify_password(const base_client_t* account, const char* password) {
    char digest[65];
    return password_digest(password, account->salt, digest) &&
           CRYPTO_memcmp(digest, account->verifier, 64) == 0;
}

static bool load_account(chat_base_t* base, const history_record_t* record) {
    const char* name = record->fields[0];
    const char* salt = record->fields[1];
    const char* verifier = record->fields[2];
    const char hex[] = "0123456789abcdef";
    if (!name[0] || strlen(name) > NAME_SIZE || strlen(salt) != 32 || strspn(salt, hex) != 32 ||
        strlen(verifier) != 64 || strspn(verifier, hex) != 64 || base->size == base->max_size)
        return false;
    if (get_client(base, name))
        return false;
    uint64_t hash = available_name_hash(base, name);
    base_client_t* account = base->base + base->size++;
    memcpy(account->name, name, strlen(name));
    account->name_hash = hash;
    strcpy(account->salt, salt);
    strcpy(account->verifier, verifier);
    return true;
}

FILE* open_dialog_history(chat_base_t* base) {
    int fd = open("odnokursniki-data.txt", O_CREAT | O_RDWR | O_APPEND, 0600);
    if (fd < 0)
        return NULL;
    FILE* file = fdopen(fd, "a+");
    if (!file) {
        close(fd);
        return NULL;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        fclose(file);
        return NULL;
    }
    setvbuf(file, NULL, _IONBF, 0);
    rewind(file);
    history_record_t record = {};
    for (;;) {
        off_t start = ftello(file);
        int status = read_record(file, &record);
        if (!status)
            break;
        if (status < 0 && feof(file) && !ferror(file) && start >= 0 &&
            ftruncate(fd, start) == 0 && fsync(fd) == 0)
            break; // interrupted final append
        if (status < 0 || (record.kind == 'A' && !load_account(base, &record)) ||
            (record.kind == 'M' && (strlen(record.fields[0]) > NAME_SIZE || strlen(record.fields[1]) > NAME_SIZE ||
                                    !get_client(base, record.fields[0]) || !get_client(base, record.fields[1])))) {
            fclose(file);
            return NULL;
        }
    }
    base->verify_password = verify_password;
    return file;
}

bool save_dialog_account(FILE* file, base_client_t* account, const char password[PSWD_SIZE]) {
    unsigned char salt[16];
    if (RAND_bytes(salt, sizeof(salt)) != 1)
        return false;
    to_hex(salt, sizeof(salt), account->salt);
    if (!password_digest(password, account->salt, account->verifier))
        return false;
    const char* fields[] = {account->name, account->salt, account->verifier};
    const size_t limits[] = {NAME_SIZE, 32, 64};
    return append_record(file, 'A', fields, limits);
}

bool save_dialog_message(FILE* file, const chat_message_t* message) {
    const char* fields[] = {message->from, message->to, message->msg_body};
    const size_t limits[] = {NAME_SIZE, NAME_SIZE, MSG_SIZE};
    return append_record(file, 'M', fields, limits);
}

bool restore_dialog_history(server_t* server, chat_session_t* session) {
    base_client_t* account = get_client(server->client_base, session->client_name);
    FILE* file = server->history;
    if (fseeko(file, 0, SEEK_SET) != 0)
        return false;
    history_record_t record = {};
    int status;
    while ((status = read_record(file, &record)) > 0) {
        if (record.kind != 'M' || (strncmp(account->name, record.fields[0], NAME_SIZE) &&
                                   strncmp(account->name, record.fields[1], NAME_SIZE)))
            continue;
        chat_message_t msg = create_chat_message(MSG_TYPE::SYSTEM);
        msg.sys_command = COMMAND::CHAT_HISTORY;
        strncpy(msg.from, record.fields[0], NAME_SIZE);
        strncpy(msg.to, record.fields[1], NAME_SIZE);
        strncpy(msg.msg_body, record.fields[2], MSG_SIZE);
        send_message(server, session, &msg);
    }
    return status == 0;
}
