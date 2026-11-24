#include "gui_core.h"
#include "gui_events.h"
#include "enter_scene.h"
#include "chat_scene.h"
#include "client.h"
#include "networking.h"
#include <unistd.h>

void run_gui_app() {
    logerr = fopen("logs.txt", "w");
    if (!logerr)
        return;
    dup2(fileno(logerr), STDERR_FILENO);
    while (true) {
        client_t* client = create_client();
        pthread_t client_tid = run_client_backend(client, 8123, "127.0.0.1");
        gui_scene_t* enter_scene = create_enter_scene();
        SceneResult result = SCENE_RESULT_EXIT;
        if (enter_scene) {
            enter_scene->client = client;
            result = scene_render(enter_scene);
            cleanup_scene(enter_scene);
        }
        if (result == SCENE_RESULT_ENTER_CHAT) {
            gui_scene_t* chat_scene = create_chat_scene();
            chat_scene->client = client;
            result = scene_render(chat_scene);
            cleanup_scene(chat_scene);
        }
        call_async_stop();
        pthread_join(client_tid, NULL);
        destroy_client(client);
        if (result == SCENE_RESULT_EXIT)
            break;
    }

    if (logerr)
        fclose(logerr);

}

int main() {
    run_gui_app();
    return 0;
}
