#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_LINE 256
#define MAX_CONNECTIONS 20
#define MAX_MESSAGE 6144
#define STATUS_ONLINE 1
#define STATUS_OFFLINE 0
#define IN_WAIT 20
#define MAX_MESS 1024


typedef int (*cmd_handler)(uv_buf_t* buf, uv_tcp_t* client);

int cmd_quit();
int cmd_to();
int cmd_name_set();
int cmd_ping();
int cmd_online();
int cmd_register();
int cmd_sign_in();
void on_conn_write();

struct _command {
	char name[16];
	cmd_handler handler;
};

struct _command COMMANDS[] = {
	{"QUIT", cmd_quit},
	{"TO", cmd_to},
    {"NAME_SET", cmd_name_set},
    {"PING", cmd_ping},
    {"ONLINE", cmd_online},
    {"REGISTER", cmd_register},
    {"SIGN_IN", cmd_sign_in},
	{"", NULL}
};

typedef struct _Messages_list{
    char* message;
    struct _Messages_list* next;
} Messages_list;

typedef struct _User{
    uv_tcp_t* client ;
    char* name;
    char* password;
    int status;
    Messages_list* messages;
} User;

int in_wait = 0;
struct _User users[MAX_CONNECTIONS];
uv_loop_t *loop;

void init_users(User* users){
    for(int i = 0; i < MAX_CONNECTIONS; i ++){
        users[i].client = NULL;
        users[i].name = (char*)malloc(MAX_LINE*sizeof(char));
        users[i].status = STATUS_OFFLINE;
        users[i].password = (char*)malloc(MAX_LINE*sizeof(char));   
        users[i].messages = NULL;
    }
    fprintf(stderr, "Users are intialized\n");
}

int client_lookup(uv_tcp_t* client) {
    for(int i = 0 ; i < MAX_CONNECTIONS; i++) {
        if (users[i].client == client)
            return 1;
    }
    return 0;
}

int send_message(uv_buf_t* buf, int offset, uv_stream_t* recipient, uv_tcp_t* client) 
{
    char message[MAX_MESSAGE];
    bzero( message, MAX_MESSAGE);

    int stat, id; //why do they declared here???

    sscanf( buf->base + offset + 1, "%[^\n]", message);
    fprintf(stderr, "message read %s\n", message);

    if ( strlen(message) < MAX_MESSAGE - 1) {
        message[strlen(message) + 1] = '\0';
        message[strlen(message)] = '\n';
    }
    else {
        message[MAX_MESSAGE - 1] = '\0';
        message[MAX_MESSAGE - 2] = '\n';
    }

    char* name = (char*)malloc(MAX_LINE * sizeof(char));
    bzero(name, MAX_LINE);

    sprintf (name, "from ");

    for(int i = 0; i < MAX_CONNECTIONS; i++){
        if (users[i].client == client){
            strcat(name, users[i].name);
            break;
        }
    }
    for(int i = 0; i < MAX_CONNECTIONS; i++){
        if (users[i].client == (uv_tcp_t*)recipient){
            stat = users[i].status;
            id = i;
            break;
        }
    }

    char dots[] = ":";
    strcat(name, dots);
    strcat(name, message);

    if ( stat == STATUS_ONLINE){
        uv_buf_t* recbuf = (uv_buf_t*)malloc(sizeof(uv_buf_t));
        recbuf->base = name;
        recbuf->len = strlen(name);
        uv_write_t* req = (uv_write_t*)malloc(sizeof(uv_write_t));
        uv_write(req, recipient, recbuf, 1, on_conn_write);
        fprintf(stderr, "message sent %s\n", recbuf->base);
        bzero(recbuf->base, recbuf->len);
        recbuf->len = 0;
        free(recbuf->base);
        free(recbuf);
        buf->len = snprintf(buf->base, buf->len, "Message sent\n");
    } else {
        Messages_list* old = users[id].messages;
        Messages_list* new = (Messages_list*)malloc(sizeof(Messages_list));
        new->message = name;
        new->next = old;
        users[id].messages = new;
        buf->len = snprintf(buf->base, buf->len, "Message sent\n");
    }
    return 0;
}

int read_name(uv_buf_t* buf, char* name, int offset, uv_stream_t** recipient) {
    sscanf(buf->base + offset, "%s", name);
    fprintf(stderr, "%s\n", name);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if ( !strcmp(users[i].name, name) ){
            *recipient = (uv_stream_t*)users[i].client;
            fprintf(stderr, "name read %s\n", users[i].name);
            return 1;
        }
    }
    return 0;
}

int pop_messages(uv_buf_t* buf, int id){
    //char* res = NULL; // 
    char* res = (char*)malloc(MAX_MESSAGE*MAX_MESS*sizeof(char)); //FUUUUUUUCK PIZDETZ
    Messages_list* messages = users[id].messages; 
    while( messages ){
        res = messages->message;
        //strcat(res, messages->message);
        messages = messages->next;
        uv_buf_t* recbuf = (uv_buf_t*)malloc(sizeof(uv_buf_t));
        recbuf->base = res;
        recbuf->len = strlen(res);
        uv_write_t* req = (uv_write_t*)malloc(sizeof(uv_write_t));
        uv_write(req, (uv_stream_t*)users[id].client, recbuf, 1, on_conn_write);
        fprintf(stderr, "%s\n", res);
        bzero(recbuf->base, recbuf->len);
        recbuf->len = 0;
        free(recbuf->base);
        free(recbuf);
    }
    buf->len = snprintf(buf->base, buf->len, "Welcome back\n");
    return 0;
}

int name_lookup(uv_buf_t* buf, char* name, int* id){
    for(int i = 0; i < MAX_CONNECTIONS; i++){
        if ( !strcmp(users[i].name, name) ){
            *id = i;
            fprintf(stderr, "name found %s\n", users[i].name);
            return 1;
        }
    }
    *id = -1;
    return 0;
}

int cmd_online(uv_buf_t* buf, uv_tcp_t* client)
{
    if ( client_lookup(client) ){
        int k = 0;

        char* online_list = (char*)calloc(MAX_MESS, sizeof(char));
        strcat (online_list, "ONLINE ");

        for (int i = 0; i < MAX_CONNECTIONS; i++) 
        {
            if (users[i].status == STATUS_ONLINE && users[i].client != client)
            {
                strcat(online_list, users[i].name);
                strcat(online_list, ":");

                k++;
            }
        }

        if (!k) 
            buf->len = snprintf(buf->base, buf->len, "Noone is online\n");
        else 
        {
            fprintf (stderr, "list to send: [%s]\n", online_list);
            free (online_list);
            // uv_buf_t* recbuf = (uv_buf_t*)malloc(sizeof(uv_buf_t));
            // recbuf->base = name;
            // recbuf->len = strlen(name);

            // uv_write_t* req = (uv_write_t*)malloc(sizeof(uv_write_t));
                
            // uv_write(req, (uv_stream_t*)client, recbuf, 1, on_conn_write);
            // fprintf(stderr, "message sent: [%s]\n", recbuf->base);

            // bzero(recbuf->base, recbuf->len);
            // recbuf->len = 0;
            // free(recbuf->base);
            // free(recbuf);
        }
    }
    else 
        buf->len = snprintf(buf->base, buf->len, "Please register or sign in\n");


    return 0;
}

int cmd_quit(uv_buf_t* buf, uv_tcp_t* client) {
	buf->len = snprintf(buf->base, buf->len, "GOOD BYE!\n");
    char mes[] = " status changed\n";
    int id;
    for (int i = 0; i < MAX_CONNECTIONS; i++){
        if (users[i].client == client){
            id = i;
            break;
        }
    }
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if ( users[i].status == STATUS_ONLINE && users[i].client != client){
            char* name = (char*)malloc(MAX_LINE*sizeof(char));
            bzero(name, MAX_LINE);
            strcpy(name, users[id].name);
            //strcat(res, name);
            strcat(name, mes);
            fprintf(stderr, "%s has left\n", name);
            // fprintf(stderr, "%s is online\n", users[i].name);
            //fprintf(stderr, "%s\n", res);
            uv_buf_t* recbuf = (uv_buf_t*)malloc(sizeof(uv_buf_t));
            recbuf->base = name;
            recbuf->len = strlen(name);
            uv_write_t* req = (uv_write_t*)malloc(sizeof(uv_write_t));
            uv_write(req, (uv_stream_t*)users[i].client, recbuf, 1, on_conn_write);
            fprintf(stderr, "%s\n", recbuf->base);
            bzero(recbuf->base, recbuf->len);
            recbuf->len = 0;
            free(recbuf->base);
            free(recbuf);	
        }
    }
	return 1;
}

int cmd_ping(uv_buf_t* buf, uv_tcp_t* client) {
	buf->len = snprintf(buf->base, buf->len, "PINGED\n");
	return 0;
}

int cmd_to( uv_buf_t* buf, uv_tcp_t* client) {
    if ( client_lookup(client)){
        char *name = (char*)calloc(MAX_LINE, sizeof(char));
        
        uv_stream_t** recipient = (uv_stream_t**)malloc(sizeof(uv_stream_t*));;
        if ( read_name(buf, name, 2, recipient) ){
            send_message(buf, 2 + strlen(name), *recipient, client);
        } else buf->len = snprintf(buf->base, buf->len, "Wrong name!\n");
    }   else buf->len = snprintf(buf->base, buf->len, "Please register or sign in\n");
    return 0;
}

int cmd_sign_in(uv_buf_t* buf, uv_tcp_t* client) {
    char* name = (char*)calloc(MAX_LINE, sizeof(char));
    char* password = (char*)calloc(MAX_LINE, sizeof(char));
    sscanf(buf->base + 8, "%s", name);
    sscanf(buf->base + 9 + strlen(name), "%s", password);
    fprintf(stderr, "name read %s\n", name);
    int id;
    if ( name_lookup(buf, name, &id) ){
        if ( !strcmp(users[id].password, password) ){
            users[id].client = client;
            users[id].status = STATUS_ONLINE;
            //buf->len = snprintf(buf->base, buf->len, "Sign in is successful\n");
            pop_messages(buf, id);
            char mes[] = "switched ";
                for (int i = 0; i < MAX_CONNECTIONS; i++) {
                    if ( users[i].status == STATUS_ONLINE && users[i].client != client){
                        char* res = (char*)malloc(MAX_LINE*sizeof(char));
                        bzero(res, MAX_LINE);
                        strcpy(res, name);
                        //strcat(res, name);
                        strcat(mes, res);
                        fprintf(stderr, "%s\n", mes);
                        // fprintf(stderr, "%s is online\n", users[i].name);
                        //fprintf(stderr, "%s\n", res);
                        uv_buf_t* recbuf = (uv_buf_t*)malloc(sizeof(uv_buf_t));
                        recbuf->base = mes;
                        recbuf->len = strlen(mes);
                        uv_write_t* req = (uv_write_t*)malloc(sizeof(uv_write_t));
                        uv_write(req, (uv_stream_t*)users[i].client, recbuf, 1, on_conn_write);
                        fprintf(stderr, "message sent %s\n", recbuf->base);
                        bzero(recbuf->base, recbuf->len);
                        recbuf->len = 0;
                        free(recbuf->base);
                        free(recbuf);	
                    }
                }
        } else buf->len = snprintf(buf->base, buf->len, "Wrong name or password\n");
    } else buf->len = snprintf(buf->base, buf->len, "Wrong name or password\n");
    return 0;
}

int cmd_register(uv_buf_t* buf, uv_tcp_t* client) {
    char* name = (char*)calloc(MAX_LINE, sizeof(char));
    char* password = (char*)calloc(MAX_LINE, sizeof(char));
    char check[] = "";

    sscanf(buf->base + 9, "%s", name);
    sscanf(buf->base + 10 + strlen(name), "%s", password);

    fprintf(stderr, "name read %s\n", name);

    int *id = (int*)malloc(sizeof(int)); //why do u allocate memory for one int elem???? where u free it???
    if ( !name_lookup(buf, name, id) ){

        fprintf (stderr, "if works\n");

        for(int i = 0; i < MAX_CONNECTIONS; i++){
            if ( !strcmp(users[i].name, check ) ){ //if u find a free place, i supouse...
                users[i].name = name;
                users[i].client = client;
                users[i].status = STATUS_ONLINE;
                users[i].password = password;
                buf->len = snprintf(buf->base, buf->len, "Registartion is successful\n");

                char* mes = (char*)calloc(MAX_LINE*2, sizeof(char));
                sprintf (mes, "switched ");

                fprintf (stderr, "registration completed\n");

                for (int i = 0; i < MAX_CONNECTIONS; i++) {
                    if ( users[i].status == STATUS_ONLINE && users[i].client != client){ //it sends to new user, who is online

                        fprintf (stderr, "starting sending...\n");

                        char* res = (char*)malloc(MAX_LINE*sizeof(char)); //alocating maaaaany time
                        bzero(res, MAX_LINE);

                        //fprintf (stderr, "falls here1\n");
                        strncpy(res, name, MAX_LINE);

                        //fprintf (stderr, "falls here2\n");
                        //strcat(res, name);
                        mes = strcat(mes, res);
                        //fprintf (stderr, "falls here3\n");

                        fprintf(stderr, "%s is online\n", mes);

                        // fprintf(stderr, "%s is online\n", users[i].name);
                        //fprintf(stderr, "%s\n", res);
                        uv_buf_t* recbuf = (uv_buf_t*)malloc(sizeof(uv_buf_t));
                        recbuf->base = mes;
                        recbuf->len = strlen(mes);

                        fprintf (stderr, "message to send: %s\n", recbuf->base);

                        uv_write_t* req = (uv_write_t*)malloc(sizeof(uv_write_t));
                        uv_write(req, (uv_stream_t*)users[i].client, recbuf, 1, on_conn_write);
                        fprintf(stderr, "message sent %s\n", recbuf->base);

                        bzero(recbuf->base, recbuf->len);
                        recbuf->len = 0;

                        //free(recbuf->base);
                        free(recbuf);	

                        fprintf (stderr, "end sending\n");
                    }
                }

                printf ("i am here\n");
                printf ("mes ptr is %p\n", mes);

                //got it, free (recbuf->base) does it's chores
                free (mes);

                printf ("die here\n");

                return 0;
            }
        }
        buf->len = snprintf(buf->base, buf->len, "Sorry, server is full :(\n");
    } else buf->len = snprintf(buf->base, buf->len, "Name is taken!\n");
    return 0;
}

int cmd_name_set(uv_buf_t* buf, uv_tcp_t* client) {
    if ( client_lookup(client)){
        char *name = (char*)calloc(MAX_LINE, sizeof(char));
        char *res = (char*)calloc(MAX_LINE, sizeof(char));
        uv_stream_t** check = (uv_stream_t**)malloc(sizeof(uv_stream_t*));
        if ( read_name(buf, name, 8, check) ){
            buf->len = snprintf(buf->base, buf->len, "Name is taken!\n");
        } else {
            for( int i = 0; i < MAX_CONNECTIONS; i++) {
                if ( users[i].client == client){
                    strcpy(res, users[i].name);
                    users[i].name = name;
                    fprintf(stderr, "name sat %s\n", users[i].name);
                    break;
                }
            }
            char mes[] = "switched ";
            char space[] = " ";
            for (int i = 0; i < MAX_CONNECTIONS; i++) {
                if ( users[i].client != client && users[i].status == STATUS_ONLINE){
                    uv_buf_t* recbuf = (uv_buf_t*)malloc(sizeof(uv_buf_t));
                    strcat(mes, res);
                    strcat(mes, space);
                    strcat(mes, name);
                    recbuf->base = res;
                    recbuf->len = strlen(res);
                    uv_write_t* req = (uv_write_t*)malloc(sizeof(uv_write_t));
                    uv_write(req, (uv_stream_t*)users[i].client, recbuf, 1, on_conn_write);
                    bzero(recbuf->base, recbuf->len);
                    recbuf->len = 0;
                    free(recbuf->base);
                    free(recbuf);
                }
            }
            buf->len = snprintf(buf->base, buf->len, "Name sat!\n");
        }
    }  else buf->len = snprintf(buf->base, buf->len, "Please register or sign in\n");
    return 0;
}


void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = uv_handle_get_data(handle);
    buf->base = (char*)realloc(buf->base, suggested_size);
    buf->len = suggested_size;
    uv_handle_set_data(handle, buf->base);
}

void on_conn_close(uv_handle_t* client) {
	void* buf = uv_handle_get_data(client);
	if (buf) {
		fprintf(stderr, "Freeing buf\n");
		free(buf);
	}
	fprintf(stderr, "Freeing client\n");
    for(int i = 0; i < MAX_CONNECTIONS; i++ ) {
        if ( users[i].client == (uv_tcp_t*)client) {
            users[i].client = NULL;
            users[i].status = STATUS_OFFLINE;
            //free(users[i].name);
            fprintf(stderr, "%p\n", users[i].client);
            fprintf(stderr, "%p\n", users[i].name);
            i = MAX_CONNECTIONS;
        }
    }
	free(client);
}


void on_conn_write(uv_write_t* req, int status) {
	if (status) {
		fprintf( stderr, "Write error: %s\n", uv_strerror(status));
	}
	free(req);
}

int process_userinput(ssize_t nread, uv_buf_t *buf, uv_tcp_t* client) {
	struct _command* cmd;
	for(cmd = COMMANDS; cmd->handler; cmd++ ) {
		if( !strncmp(buf->base, cmd->name, strlen(cmd->name)) ) {
			return cmd->handler(buf, client);
		}
	}
    //buf->len = snprintf(buf->base, buf->len, "UNKNOWN COMMAND '%3s'\n", buf->base); works incorrect . why?
	buf->len = snprintf(buf->base, buf->len, "UNKNOWN COMMAND \n"/*, buf->base*/); //there was '%3s'
	return 0;
}

void on_client_read(uv_stream_t* client, ssize_t nread, const uv_buf_t *buf) {
	if(nread < 0) {
		if(nread != UV_EOF){
		fprintf(stderr, "read error %s\n", uv_err_name(nread));
		uv_close((uv_handle_t*)client, on_conn_close);
		}
	} else if (nread > 0) {
		int res;
		uv_buf_t wrbuf = uv_buf_init(buf->base, buf->len);
		res = process_userinput(nread, &wrbuf, (uv_tcp_t*)client);
		uv_write_t* req = (uv_write_t*)malloc(sizeof(uv_write_t));
	    uv_write(req, client, &wrbuf, 1, on_conn_write);
        bzero(buf->base, buf->len);	
		if( res ) {
			uv_close((uv_handle_t*)client, on_conn_close);
		}
	}
}

void on_new_connection(uv_stream_t* server, int status) {
    if (status < 0) {
        fprintf( stderr, "Conection error...%s\n", uv_strerror(status));
        return;
    }
    uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    fprintf(stderr, "client allocated\n");
    bzero(client, sizeof(uv_tcp_t));
    uv_tcp_init(loop,client);
    if ( uv_accept( server, (uv_stream_t*)client) == 0  && in_wait < IN_WAIT){
        fprintf(stderr, "Client %p accepted\n", client);
        //need to know the name of user
        in_wait++;
        uv_read_start((uv_stream_t*)client, alloc_buffer, on_client_read);
    } else {
        fprintf(stderr, "Error while accepting...\n");
        uv_close((uv_handle_t*)client, on_conn_close);
    }
}

int main(){
    int result;
    struct sockaddr_in addr;
    uv_tcp_t server;
    loop = uv_default_loop();
    uv_tcp_init( loop, &server);
    uv_ip4_addr("0.0.0.0", 8000, &addr );
    uv_tcp_bind( &server, (const struct sockaddr *)&addr, 0);
    init_users(users);
    int r = uv_listen((uv_stream_t *)&server, 1024, on_new_connection);
    if ( r ){
            fprintf(stderr, "Listen error...%s\n", uv_strerror(r));
            return 1;
    }
    result = uv_run(loop, UV_RUN_DEFAULT);
    fprintf(stderr, "Finalization...\n");
    return result;
}
