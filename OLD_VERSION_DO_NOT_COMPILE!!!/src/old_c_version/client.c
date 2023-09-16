#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <ncurses.h>
#include <pthread.h>

#define MAX_SIZE 1000
int CHANGED_STATUS = 0;
int user_num = 0;

int REG_STATUS = 1;
int ONLINE = 1;
int SIGN_IN_STATUS = 1;
int ENTERING_ERROR = 1;
int NAME_IS_TAKEN = 1;
int NEW_MESSAGE = 1;
int INIT_COMPLETE = 0;
int TRUE_SIGN_IN_STATUS = 1;

void write2(uv_stream_t* stream, char *data, int len2);

char* get_command (char* server_reply) {
    int i = 0;
    char* command = malloc (sizeof(char)*MAX_SIZE);

    while (*(server_reply + i) != ':' && i < strlen(server_reply))
    {
        if (server_reply[i] == '*')
            return NULL;

        command[i] = *(server_reply + i);
        i++;
    }
    command[i+1] = '\0';
    return command;
}
typedef struct _choices {
    char* choice; 
}string_massiv;

typedef struct _mess_queue {
    char* new_message;
    struct _mess_queue* next;
} mess_queue;

typedef struct _users_queue {
    char* name;
    struct _users_queue* next;
} users_queue;

typedef struct _win {
    WINDOW* win;
    int highth;
    int width;
    int start_y;
    int start_x;
    int string_num;
} window;

typedef struct _user {
    mess_queue* start; //begining of the messages queue
    mess_queue* current_message;
    window win;
    int PRINT_FLAG;
    char* name;
    char status;
} user;

user *user_base;
char* server_reply;

users_queue* origin = NULL;
users_queue* current_user = NULL;

static uv_loop_t* loop = NULL;

typedef struct _user_input {
    uv_stream_t* stream;
    char* user_input;
    uv_connect_t* connect;
} potok_data;

int get_num (char* name) {
    for (int i = 0; i < user_num; i++) {
        if (!strcmp(name, user_base[i].name)) {
            return i;
        }

    }
    return -1;
}

void on_write(uv_write_t* req, int status)
{
  if (status) {
    perror ("uv_write error ");
        return;
  }
    free(req);
}

void user_win_init (user* user_base, int num, int width, int highth, int start_y, int start_x, int string_num)
{
        user_base [num].win.win  = newwin(highth, width, start_y, start_x);
        user_base [num].win.width = width;
        user_base [num].win.highth = highth;
        user_base [num].win.start_y = start_y;
        user_base [num].win.start_x = start_x;
        user_base [num].win.string_num = string_num;
}

char* win_input (WINDOW* win, int start_y, int start_x, int mode)
{    
    char* input = malloc (sizeof(char)*MAX_SIZE);

    if (mode)
    {
        echo();
        int pointer = 0;

        while (1) 
        {
            char c = wgetch (win);

                if ((int)c == 127 && pointer > 0)
                {
                    pointer--;
                    wmove (win, start_y, pointer + start_x);
                    wclrtoeol(win);
                    box (win, 0, 0);
                    bzero (input + pointer, MAX_SIZE);
                    wrefresh (win);
                }
                else if ((int)c != 10 && (int)c != -1 && (int)c != 127)
                {
                    input [pointer] = c;
                    pointer ++;
                }
                else if ((int)c != -1 && pointer)
                {   
                return input; 
                }
                else {
                    wmove (win, start_y, start_x + pointer);
                    wclrtoeol(win);
                 
                    box (win, 0, 0);
                    wrefresh(win);
                }
        }
    }
    else
    {
        noecho();
        int pointer = 0;
        
        while (1) {
            char c = wgetch (win);
            if ((int)c == 127 && pointer > 0)
            {
                pointer--;
                wmove (win, start_y, start_x + pointer);
                wclrtoeol(win);
                box (win, 0, 0);
                bzero (input + pointer, MAX_SIZE);
                wrefresh (win);
            }
            else if ((int)c != 10 && (int)c != -1 && (int)c != 127)
            {
                mvwprintw (win, start_y, start_x + pointer, "*");
                input [pointer] = c;
                pointer ++;
            }
             else if ((int)c != -1 && pointer)
            {   
                return input; 
            }
            else {
                wmove (win, start_y, start_x + pointer);
                wclrtoeol(win);
                 
                box (win, 0, 0);
                wrefresh(win);
            }

        }
    }
}

void user_input (WINDOW* display, user* user, potok_data* stream_data, WINDOW* menu_win) {

    char* message = malloc (sizeof(char)*MAX_SIZE);
    int pointer = 0;
    char c;
    int start_pos = 0;
    int xMAX, yMAX;
    getmaxyx(stdscr, yMAX, xMAX);

    refresh ();

    int heigth = 5;

    WINDOW* input_win = newwin (heigth, user -> win.width - 2, yMAX - 6, user -> win.start_x + 1);
    refresh ();
    nodelay(input_win, true);
    echo();

    box (input_win, 0, 0);
    wmove (input_win, 1, 1);    
    wrefresh (input_win);


    while ((int)c != 27) 
    {

    for (int i = 0; i < user_num; i ++) 
    {   
                if (i > 0){
                        if (user_base[i].status == 1)
                        {   
                            wclrtoeol (menu_win);
                            mvwprintw (menu_win, i + 1, xMAX/3 - 10, "online");
                        }
                        else 
                        {
                            mvwprintw (menu_win, i + 1, xMAX/3 - 10, "offline");
                        }
                }
            }

            box(menu_win, 0, 0); 
            wrefresh (menu_win);
            
    while (user -> current_message != NULL)
    {
        mvwprintw (display, user -> win.string_num, 1, user -> current_message -> new_message);
        box (display, 0, 0);
        wrefresh (display);
        user -> win.string_num ++;
        mess_queue* new = user -> current_message -> next;
        free((user -> current_message) -> new_message);
        user -> current_message = new;
    }
    
    c = wgetch(input_win);

            if ((int)c == 127 && pointer > 0)
            {
                pointer--;
                wmove (input_win, 1, pointer + 1);
                wclrtoeol(input_win);
                bzero (message + pointer, MAX_SIZE);
                 
                box (input_win, 0, 0);
                wrefresh (input_win);
            }

            else if ((int)c != 10 && (int)c != -1 && (int)c != 127 && (int)c != 27)
            {
            
                message[pointer] = c;
                pointer ++;
                mvwchgat (input_win, 1, pointer + 1, 1,  A_REVERSE, 2, NULL);
                wrefresh (input_win);
                
            }
        
            else if ((int)c != -1 && pointer) //нажат enter и сообщение не пусто 
            {   
                char* final_message = stream_data -> user_input;
                strcpy (final_message, "TO"); 
                strcat (final_message, user -> name);
                strcat (final_message, " ");
                strcat (final_message, message);

                mvwprintw (display, user -> win.string_num, 1, message);
                wrefresh (display);

                uv_buf_t buffer[] = {
                {.base = final_message, .len = strlen (final_message)}
                };
                uv_write_t *req = malloc(sizeof(uv_write_t));
                uv_write(req, stream_data -> stream, buffer, 1, on_write);

                bzero (final_message, MAX_SIZE);
                bzero (message, MAX_SIZE);

                user -> win.string_num ++;

                pointer = 0;
                wclear(input_win);
                box (input_win, 0, 0);
                wrefresh(input_win);

                wmove (input_win, 1, 1);
            }

            else 
            {
                wmove (input_win, 1, pointer + 1);
                wclrtoeol(input_win);
                mvwchgat (input_win, 1, pointer + 1, 1,  A_REVERSE, 2, NULL);
                box (input_win, 0, 0);
                wrefresh(input_win);
            }
    }
}

void usrwin_open (user* user, potok_data* stream_data, WINDOW* menu_win)
{
    WINDOW* dialog_win = user -> win.win;
    refresh();
    box (dialog_win, 0, 0);
    wrefresh(dialog_win);

    user_input (dialog_win, user, stream_data, menu_win);

    //usrwin_close();  
}

void on_close(uv_handle_t* handle)
{
  printw("closed.");
  refresh();
}

char* user_choice (int choice, WINDOW* win, int yMAX, int xMAX)
{
    box (win, 0, 0);
    mvwprintw (win, 1, (xMAX/2 - 12)/2, "ODNOKURSNIKI");
    wrefresh (win);
    curs_set (1);

    if (choice == 0) {
        mvwprintw (win, 3, (xMAX/2 - 33)/2,  "ENTER YOUR USERNAME AND PASSWORD");
        mvwprintw (win, 5, 1, "USERNAME: ");
        wrefresh (win);

        char* user_name = win_input (win, 5, 11, 1);

        mvwprintw (win, 6, 1, "PASSWORD: ");
        wrefresh (win);

        char* password = win_input (win, 6, 11,  0);

        char* to_server = malloc (sizeof(char)*MAX_SIZE);
        char space [] = " ";
        strcpy (to_server, "SIGN_IN ");
        strcat (to_server, user_name);
        strcat (to_server, space);
        strcat (to_server, password);

        wrefresh(win);
       
        return to_server;
        
    }
    else {
        mvwprintw (win, 3, (xMAX/2 - 33)/2,  "ENTER YOUR USERNAME AND PASSWORD");
        mvwprintw (win, 5, 1, "CREATE USERNAME: ");
        wrefresh (win);

        char* user_name = win_input (win, 5, 18, 1);

        mvwprintw (win, 6, 1, "CREATE PASSWORD: ");
        wrefresh (win);

        char* password = win_input (win, 6, 18,  1);

        char* to_server = malloc (sizeof(char)*MAX_SIZE);
        char space [] = " ";

        strcpy (to_server, "REGISTER ");
        strcat (to_server, user_name);
        strcat (to_server, space);
        strcat (to_server, password);
        return to_server;        
    }
    
}

void user_enter (int yMAX, int xMAX, uv_stream_t* stream) {

    string_massiv choices[2];

    WINDOW* register_win = newwin (yMAX/2, xMAX/2, yMAX/4, xMAX/4);
    refresh();

    box (register_win, 0, 0);
    mvwprintw (register_win, 1, (xMAX/2 - 12)/2, "ODNOKURSNIKI");
    mvwprintw (register_win, 2, (xMAX/2 - 17)/2, "SIGN IN/REGISTER?");
    wrefresh (register_win);
    keypad (register_win, true);

    choices[0].choice = malloc (sizeof(char)*MAX_SIZE);
    choices[0].choice = "SIGN IN";

    choices[1].choice = malloc (sizeof(char)*MAX_SIZE);
    choices[1].choice = "REGISTER";

    char* to_server;
    int reply = 0;
    int highlight = 0;
    noecho();
    nodelay(register_win, true);
    int k = 0;

    while (!INIT_COMPLETE){
        if (k > 0)
        {
            mvwprintw (register_win, 1, (xMAX/2 - 12)/2, "ODNOKURSNIKI");
            mvwprintw (register_win, 2, (xMAX/2 - 35)/2, "INCORRECT USER NAME OR PASSWORD!!!");
            box (register_win, 0, 0);
            wrefresh(register_win);
            k = 0;
        }
        
        for (int i = 0; i < 2; i++) {
            if (highlight == i) 
                wattron (register_win, A_REVERSE);
        
            mvwprintw (register_win, yMAX/4 - 2 + i, (xMAX/2 - 7)/2, choices[i].choice);
            wattroff (register_win, A_REVERSE);
        }

        int c = wgetch(register_win);

        switch (c)
        {
            case KEY_UP:
                if (highlight == 0)
                    highlight = 0;
                else 
                    highlight --;
            break;

            case KEY_DOWN:
                if (highlight == 1)
                    highlight = 1;
                else
                    highlight ++;
            break;

            case 10:
            
                wclear (register_win);
                wrefresh(register_win);

                to_server = user_choice (highlight, register_win, yMAX, xMAX);
                curs_set (0);
                
                uv_buf_t buffer[] = {
                {.base = to_server, .len = strlen(to_server)}
                };
                uv_write_t *req = malloc(sizeof(uv_write_t));
                uv_write(req, stream, buffer, 1, on_write);

                wclear (register_win);
                wrefresh(register_win);
                k ++;
            break;

            default:
            break;
        } 

    }
    wclear (register_win);
    wrefresh (register_win);
    delwin(register_win);  
}

void *potok_func (void* input)
{   
    potok_data* stream_data = (potok_data*) input; 
    user_base = malloc(sizeof(user) * MAX_SIZE);

    initscr();
    noecho();
    curs_set (0);
    
    int yMAX, xMAX;
    getmaxyx(stdscr, yMAX, xMAX);

    user_enter (yMAX, xMAX, stream_data -> stream);

    write2 (stream_data -> stream, "ONLINE", 7); //отправляем на сервер запрос на отображение онлайн юзеров

    WINDOW* menu_win = newwin (yMAX - 1, xMAX/3, 1, 1);    
    refresh();

    nodelay (menu_win, true);
    box (menu_win, 0, 0);
    wrefresh (menu_win);
    wmove (menu_win, 1, 1);
    keypad (menu_win, true);
    int highlight = 0;

    //users_queue* new_user_pointer = current_user;

     
           
    user_base[user_num].name = malloc(sizeof(char)*MAX_SIZE);
    user_base[user_num].name = "QUIT";
    user_num ++;

    int QUIT = 0;
    int k = 2;

    //user's input function
    while (!QUIT) 
    {    
        noecho();

        while (/*new_user_pointer*/ current_user != NULL)
        {
            user_base[user_num].name = malloc (sizeof(char) * MAX_SIZE);
            user_base[user_num].name = /*new_user_pointer*/ current_user -> name;
            user_base[user_num].status = 1;
            user_win_init (user_base, user_num, (2*xMAX/3) - 3, yMAX - 1, 1, xMAX/3 + 3, 1);
            user_num ++;
            /*new_user_pointer */current_user = /*new_user_pointer*/ current_user -> next;
        }

            for (int i = 0; i < user_num; i ++) 
            {
                if (highlight == i) 
                    wattron (menu_win, A_REVERSE);
                
                if (i > 0){
                        mvwprintw (menu_win, i + 1, 1, user_base[i].name);
                        if (user_base[i].status == 1)
                        {   
                            wclrtoeol (menu_win);
                            mvwprintw (menu_win, i + 1, xMAX/3 - 10, "online");
                            wattroff (menu_win, A_REVERSE);
                        }
                        else 
                        {
                            mvwprintw (menu_win, i + 1, xMAX/3 - 10, "offline");
                            wattroff (menu_win, A_REVERSE);

                        }
                }
                else {
                        mvwprintw (menu_win, i + 1, 1, user_base[i].name);
                        wattroff (menu_win, A_REVERSE);
                }
            }

            box(menu_win, 0, 0);

        int c = wgetch (menu_win);

        switch (c)
        {
            case KEY_UP:
                if (highlight == 0)
                    highlight = 0;
                else 
                    highlight --;
            break;

            case KEY_DOWN:
                if (highlight == user_num - 1)
                    highlight = user_num - 1;
                else
                    highlight ++;
            break;

            case 10:
                if (highlight > 0){
                    usrwin_open (&(user_base[highlight]), stream_data, menu_win);
                }
                else {
                    wclear (menu_win);
                    wrefresh (menu_win);
                    QUIT = 1;
                    curs_set (1);
                    write2 (stream_data -> stream, "QUIT", 5);
                }
            break;

            default:
                break;
        }  

    }
    delwin (menu_win);
    endwin();
    return 0;
}

void write2(uv_stream_t* stream, char *data, int len2) {
    uv_buf_t buffer[] = {
        {.base = data, .len = len2}
    };
    uv_write_t *req = malloc(sizeof(uv_write_t));
    uv_write(req, stream, buffer, 1, on_write);
}

void alloc_buffer (uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    if(buf->len == 0)
    {
        buf -> base = (char*)malloc(suggested_size);
    }
    else
    {
    buf -> base = (char*)realloc(buf -> base, suggested_size);
    }
    buf -> len  = suggested_size;
    uv_handle_set_data(handle, buf -> base);
}

void on_read(uv_stream_t* tcp, ssize_t nread, uv_buf_t* buf)
{ 
    if (nread > 0) 
    {
        server_reply = buf -> base;
        //fprintf (stderr, "server reply: %s\n", server_reply);

        //printf ("printed\n");
        char* user_name;
        int possition = -777;
        char* old_name;
        char* new_name;
        
            if (INIT_COMPLETE == 0) {
                REG_STATUS = strncmp (server_reply, "Registartion is successful\n", strlen ("Registartion is successful\n"));
                SIGN_IN_STATUS = strncmp (server_reply, "Welcome back\n" , strlen("Welcome back\n"));
                TRUE_SIGN_IN_STATUS = strcmp (server_reply, "Welcome back\n");
                ENTERING_ERROR = strncmp (server_reply, "0X1\n", strlen ("0X1\n"));
                NAME_IS_TAKEN = strncmp(server_reply, "Name is taken!\n", strlen("Name is taken!\n"));
            }

            NAME_IS_TAKEN = strncmp(server_reply, "Name is taken!\n", strlen ("Name is taken!\n"));
            NEW_MESSAGE = strncmp (server_reply, "from", 4);
            ONLINE = strncmp (server_reply, "ONLINE", strlen ("ONLINE"));

            if (!REG_STATUS || !SIGN_IN_STATUS && NAME_IS_TAKEN) {
                INIT_COMPLETE = 1;
            }

            if (!ONLINE) { //this "if" parsing a list of online users

                int read = strlen("ONLINE") + 1;

                while (get_command(server_reply + read))
                {
                    user_name = get_command (server_reply + read);
                    read += strlen(user_name) + 2;
                    users_queue* new_user = (users_queue*)malloc(sizeof(users_queue));
                    new_user -> next = NULL;
                    new_user -> name = user_name;

                    if (origin == NULL) {
                        origin = new_user; 
                        current_user = new_user;
                    }
                    else {
                        new_user -> next = current_user;
                        current_user = new_user;
                    }
              
                }
                ONLINE = 1;
            }

            else if (!strncmp(server_reply, "changed", 7)){ 

                old_name = get_command (server_reply + strlen("changed") + 1);
                possition = get_num (old_name);

                new_name = get_command (server_reply + strlen(old_name) + 3 + strlen("changed"));
                        bzero (user_base[possition].name, MAX_SIZE);
                        user_base[possition].name = new_name;
            }
        
            else if (!strncmp (server_reply, "switched", 8)) {
        
                user_name = get_command (server_reply + strlen("switched") + 1);

                possition = get_num(user_name);

                if (possition == -1) {

                    users_queue* new_user = (users_queue*)malloc(sizeof(users_queue));
                    new_user -> next = NULL;
                    new_user -> name = user_name;

                    if (origin == NULL) {
                        origin = new_user;
                        current_user = new_user;
                    }
                    else {
                        new_user -> next = current_user;
                        current_user = new_user;
                    }
                }

                else if (user_base[possition].status == 1)
                {   
                    user_base[possition].status = 0;
                }
                else 
                {
                    user_base[possition].status = 1;
                }       
            }  

            else if (!NEW_MESSAGE) 
            {
                user_name = get_command (server_reply + strlen("from") + 1); //+ 1 for space, suuus
                int sender_num = get_num (user_name);

                    if (sender_num > 0)
                    {
                        mess_queue* message = (mess_queue*)malloc(sizeof(mess_queue));
                        message->next = NULL;
                        message -> new_message = (char*)calloc(MAX_SIZE, sizeof(char));
                        memcpy (message->new_message, server_reply, strlen(server_reply)); 

                    if (user_base[sender_num].start == NULL)
                    {
                        user_base[sender_num].start = message;
                        user_base[sender_num].current_message = message;
                    }
                    else 
                    {    
                        message -> next = user_base[sender_num].current_message;
                        user_base[sender_num].current_message = message;
                    }
                    }
            }  

        else {
        bzero(buf->base, buf->len);
        }

        bzero(buf->base, buf->len);
    }
    else {
    uv_close((uv_handle_t*)tcp, on_close);
    } 
    free(buf->base);
}


void on_connect (uv_connect_t* connect, int status) 
{ 
    fprintf (stderr, "%d\n", status);
    if (status < 0) 
    {
        fprintf (stderr, "New connection error %s\n", uv_strerror(status));
        return;
    }
    else
    {
        uv_stream_t* stream = connect -> handle; //this is our handle
        write2(stream, "PING\n", 6);

        char* final_message = malloc (sizeof(char)*MAX_SIZE);

        potok_data* input = malloc (sizeof(potok_data));

        input -> stream = stream;
        input -> user_input = final_message;
        input -> connect = connect;

        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        
        pthread_create(&tid, &attr, potok_func, input); //this turns on graph interfece
        uv_read_start(stream, alloc_buffer, on_read); //this guy reading answer from server
        
        free (connect);

    }
}


void establishing_connection(char* IP_addr, int nport, uv_tcp_t** user_socket)
{
    uv_connect_t* connect = (uv_connect_t*)malloc(sizeof(uv_connect_t));
    uv_tcp_init (loop, *user_socket);

    struct sockaddr_in dest; // where to connect
    uv_ip4_addr (IP_addr, nport, &dest); //return a struct with adresses for programm 
    
    uv_tcp_connect(connect, *user_socket, (const struct sockaddr*) &dest, on_connect);
    
}

int main() {

    uv_tcp_t* socket = (uv_tcp_t*)malloc(sizeof(uv_tcp_t)); 
    server_reply = malloc(MAX_SIZE);

    loop = uv_default_loop ();

    establishing_connection("0.0.0.0", 8000, &socket);

    uv_run(loop, UV_RUN_DEFAULT);  

    return 0;
}