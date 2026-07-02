#ifndef OK_GUI_EVENTS_H
#define OK_GUI_EVENTS_H

#include "gui.h"

void handle_pending_events(gui_scene_t* scene);
SceneResult scene_render(gui_scene_t* scene);
int schedule_scene_events(gui_event_t* event);
bool handle_resize_event(gui_scene_t* scene, gui_event_t* event);
bool handle_finalize_event(gui_scene_t* scene, gui_event_t* event);

#endif // OK_GUI_EVENTS_H
