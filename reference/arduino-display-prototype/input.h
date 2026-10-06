#ifndef INPUT_H
#define INPUT_H

enum InputEvent {
    EVT_NONE = 0,
    EVT_LEFT,
    EVT_RIGHT,
    EVT_PRESS_SHORT,
    EVT_PRESS_LONG
};

void input_init();
InputEvent input_poll();      // einmal pro loop() aufrufen
bool input_any_activity();    // true, wenn seit letztem Aufruf irgendein Input kam (fuer Idle-Timer)

#endif
