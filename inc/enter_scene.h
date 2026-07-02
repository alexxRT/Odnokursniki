#ifndef OK_ENTER_SCENE_H
#define OK_ENTER_SCENE_H

#include "gui.h"

gui_scene_t* create_enter_scene();
bool handle_enter_resize_event(gui_scene_t* scene, gui_event_t* event);

#endif // OK_ENTER_SCENE_H
