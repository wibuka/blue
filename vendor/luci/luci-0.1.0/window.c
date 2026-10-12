#define RGFW_IMPLEMENTATION
#define RGFW_OPENGL

#include <RGFW/RGFW.h>
#include <glad/glad.h>
#include <stdbool.h>

typedef struct {
    const char* title;
    uint32_t width, height;
    int x, y;
    bool resizable;
    bool always_on_top;
    bool undecorated;
} WindowState;

static RGFW_window* window = NULL;
static WindowState state = { "luci", 400, 400, 0, 0, true, false, false };

void window_init(void) {
    RGFW_init("luci", RGFW_initOpenGL);
    uint32_t flags = RGFW_windowHide | RGFW_windowOpenGL;
    if (state.undecorated) flags |= RGFW_windowNoBorder;
    window = RGFW_createWindow(state.title, state.x, state.y, (int)state.width, (int)state.height, flags);
    RGFW_window_show(window);
    if (window) {
        RGFW_window_makeCurrentContext_OpenGL(window);
        gladLoadGLLoader((GLADloadproc)RGFW_getProcAddress_OpenGL);
        if (state.always_on_top) RGFW_window_setFloating(window, true);
        if (state.undecorated) RGFW_window_setBorder(window, false);
    }
}

void window_close(void) {
    if (window) {
        RGFW_window_close(window);
        window = NULL;
    }
    RGFW_deinit();
}

void window_show(void) {
    if (!window) return;

    RGFW_window_setName(window, state.title);
    RGFW_window_resize(window, (int)state.width, (int)state.height);
    RGFW_window_move(window, state.x, state.y);

    int limit = state.resizable ? 0 : (int)state.width;
    int limit_h = state.resizable ? 0 : (int)state.height;
    RGFW_window_setMinSize(window, limit, limit_h);
    RGFW_window_setMaxSize(window, limit, limit_h);
    RGFW_window_setFloating(window, state.always_on_top);
    RGFW_window_setBorder(window, !state.undecorated);

    RGFW_window_show(window);
}

void window_hide(void) {
    if (window) RGFW_window_hide(window);
}

void window_set_title(const char* title) {
    state.title = title;

    if (window) RGFW_window_setName(window, title);
}

void window_resize(uint32_t width, uint32_t height) {
    state.width = width;
    state.height = height;

    if (window) RGFW_window_resize(window, (int)width, (int)height);
}

void window_position(int x, int y) {
    state.x = x;
    state.y = y;

    if (window) RGFW_window_move(window, x, y);
}

void window_set_resizable(bool resizable) {
    state.resizable = resizable;

    if (!window)
        return;

    int min_w = resizable ? 0 : (int)state.width;
    int min_h = resizable ? 0 : (int)state.height;
    int max_w = resizable ? 0 : (int)state.width;
    int max_h = resizable ? 0 : (int)state.height;

    RGFW_window_setMinSize(window, min_w, min_h);
    RGFW_window_setMaxSize(window, max_w, max_h);
}

void window_set_always_on_top(bool always_on_top) {
    state.always_on_top = always_on_top;
    if (window) RGFW_window_setFloating(window, always_on_top);
}

void window_set_undecorated(bool undecorated) {
    state.undecorated = undecorated;
    if (window) RGFW_window_setBorder(window, !undecorated);
}

bool window_should_close(void) {
    return window ? RGFW_window_shouldClose(window) : true;
}

RGFW_window* window_get_handle(void) { return window; }

void window_set_exit_key(int key) {
    if (window) RGFW_window_setExitKey(window, (RGFW_key)key);
}

bool window_is_key_pressed(int key)  { return RGFW_isKeyPressed((RGFW_key)key); }
bool window_is_key_down(int key)     { return RGFW_isKeyDown((RGFW_key)key); }
bool window_is_key_released(int key) { return RGFW_isKeyReleased((RGFW_key)key); }
bool window_is_key_up(int key)       { return !RGFW_isKeyDown((RGFW_key)key); }
int  window_get_key_pressed(void) {
    for (int i = 0; i < 256; i++) if (RGFW_isKeyPressed((RGFW_key)i)) return i;
    return 0;
}
int  window_get_char_pressed(void) {
    return 0;
}

bool window_is_mouse_button_pressed(int b)  { return RGFW_isMousePressed((RGFW_mouseButton)b); }
bool window_is_mouse_button_down(int b)     { return RGFW_isMouseDown((RGFW_mouseButton)b); }
bool window_is_mouse_button_released(int b) { return RGFW_isMouseReleased((RGFW_mouseButton)b); }
bool window_is_mouse_button_up(int b)       { return !RGFW_isMouseDown((RGFW_mouseButton)b); }
int  window_get_mouse_x(void) { int x=0,y=0; if(window) RGFW_window_getMouse(window,&x,&y); return x; }
int  window_get_mouse_y(void) { int x=0,y=0; if(window) RGFW_window_getMouse(window,&x,&y); return y; }
void window_get_mouse_position(int *x, int *y) {
    if (x) *x = 0; if (y) *y = 0;
    if (window) RGFW_window_getMouse(window, (i32*)x, (i32*)y);
}
void window_get_mouse_delta(float *x, float *y) {
    float dx=0, dy=0;
    RGFW_getMouseVector(&dx, &dy);
    if (x) *x = dx; if (y) *y = dy;
}
float window_get_mouse_wheel(void) {
    float wx=0, wy=0;
    RGFW_getMouseScroll(&wx, &wy);
    return wy;
}
float window_get_mouse_wheel_x(void) {
    float wx=0, wy=0;
    RGFW_getMouseScroll(&wx, &wy);
    return wx;
}
float window_get_mouse_wheel_y(void) {
    float wx=0, wy=0;
    RGFW_getMouseScroll(&wx, &wy);
    return wy;
}
bool window_is_mouse_inside(void) {
    return window ? RGFW_window_isMouseInside(window) : false;
}

bool window_is_gamepad_available(int g) { (void)g; return false; }
const char* window_get_gamepad_name(int g) { (void)g; return NULL; }
bool window_is_gamepad_button_pressed(int g,int b)  { (void)g;(void)b; return false; }
bool window_is_gamepad_button_down(int g,int b)     { (void)g;(void)b; return false; }
bool window_is_gamepad_button_released(int g,int b) { (void)g;(void)b; return false; }
bool window_is_gamepad_button_up(int g,int b)       { (void)g;(void)b; return true; }
int  window_get_gamepad_button_pressed(void) { return 0; }
int  window_get_gamepad_axis_count(int g) { (void)g; return 0; }
float window_get_gamepad_axis_movement(int g,int a) { (void)g;(void)a; return 0.0f; }

void window_begin(void) {
    if (!window) return;
    RGFW_pollEvents();
}

void window_end(void) {
    if (!window) return;
    RGFW_window_swapBuffers_OpenGL(window);
}

int window_width(void) {
    return window ? (int)window->w : (int)state.width;
}

int window_height(void) {
    return window ? (int)window->h : (int)state.height;
}
