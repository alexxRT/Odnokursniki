#include "gui_core.h"
#include "gui_events.h"
#include "enter_scene.h"
#include "chat_scene.h"
#include "client.h"

void run_gui_app() {
    logerr = fopen("logs.txt", "w");
    client_t* client = create_client();
    pthread_t client_tid;

    if (client)
        client_tid = run_client_backend(client, 8123, "127.0.0.1");

    while (true) {
        gui_scene_t* enter_scene = create_enter_scene();
        if (!enter_scene)
            return;
        // wire up networking client
        enter_scene->client = client;

        SceneResult result = scene_render(enter_scene);
        cleanup_scene(enter_scene);

        if (result == SCENE_RESULT_EXIT)
            break;
        if (result != SCENE_RESULT_ENTER_CHAT)
            continue;

        gui_scene_t* chat_scene = create_chat_scene();
        if (!chat_scene)
            break;
        // same with networking here
        chat_scene->client = client;

        result = scene_render(chat_scene);
        cleanup_scene(chat_scene);

        if (result == SCENE_RESULT_EXIT)
            break;
    }

    if (logerr)
        fclose(logerr);

    if (client) {
        destroy_client(client);
        pthread_join(client_tid, NULL);
    }
}

int main() {
    run_gui_app();
    return 0;
}
