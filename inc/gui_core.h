#ifndef OK_GUI_CORE_H
#define OK_GUI_CORE_H

#include <stdio.h>
#include "gui.h"

extern FILE* logerr;
extern bool g_ncurses_init;

#define stderr logerr

void init_ncurses_once();
bool ask_active(gui_scene_t* scene);
bool ask_active(window_t* window);
void terminate_scene(gui_scene_t* scene);
void cleanup_scene(gui_scene_t* scene);

#endif // OK_GUI_CORE_H
