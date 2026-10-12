#include "luci.h"

// Internal_____________________________________________________________________________________________________________________

void window_init(void);
void window_close(void);
void window_show(void);
void window_hide(void);
void window_set_title(const char* title);
void window_resize(uint32_t width, uint32_t height);
void window_position(int x, int y);
void window_set_resizable(bool resizable);
void window_set_always_on_top(bool always_on_top);
void window_set_undecorated(bool undecorated);
bool window_should_close(void);
void window_begin(void);
void window_end(void);
int window_width(void);
int window_height(void);
void window_set_exit_key(int key);
bool window_is_key_pressed(int key);
bool window_is_key_down(int key);
bool window_is_key_released(int key);
bool window_is_key_up(int key);
int  window_get_key_pressed(void);
bool window_is_mouse_button_pressed(int button);
bool window_is_mouse_button_down(int button);
bool window_is_mouse_button_released(int button);
bool window_is_mouse_button_up(int button);
int  window_get_mouse_x(void);
int  window_get_mouse_y(void);
void window_get_mouse_position(int *x, int *y);
void window_get_mouse_delta(float *x, float *y);
float window_get_mouse_wheel(void);
float window_get_mouse_wheel_x(void);
float window_get_mouse_wheel_y(void);
bool window_is_mouse_inside(void);
bool window_is_gamepad_available(int gamepad);
const char *window_get_gamepad_name(int gamepad);
bool window_is_gamepad_button_pressed(int gamepad, int button);
bool window_is_gamepad_button_down(int gamepad, int button);
bool window_is_gamepad_button_released(int gamepad, int button);
bool window_is_gamepad_button_up(int gamepad, int button);
int  window_get_gamepad_button_pressed(void);
int  window_get_gamepad_axis_count(int gamepad);
float window_get_gamepad_axis_movement(int gamepad, int axis);

void renderer_draw_texture(LUCI_Texture texture, float x, float y, LUCI_Color tint);
void renderer_draw_texture_ex(LUCI_Texture texture, LUCI_Rectangle source, LUCI_Rectangle dest, float origin_x, float origin_y, float rotation, LUCI_Color tint);

void renderer_set_default_font(LUCI_Font font);
void renderer_init(int width, int height);
void renderer_draw_rectangle(float x, float y, float width, float height, LUCI_Color tint);
void renderer_draw_rectangle_ex(float x, float y, float width, float height, float origin_x, float origin_y, float rotation, LUCI_Color tint);
void renderer_draw_line(float x0, float y0, float x1, float y1, LUCI_Color tint);
void renderer_draw_line_ex(float x0, float y0, float x1, float y1, int thickness, LUCI_Color tint);
void renderer_draw_circle(float center_x, float center_y, float radius, LUCI_Color tint);
void renderer_draw_ray(float origin_x, float origin_y, float dir_x, float dir_y, float length, LUCI_Color tint);
void renderer_draw_ray_ex(float origin_x, float origin_y, float dir_x, float dir_y, float length, int thickness, LUCI_Color tint);
void renderer_draw_box_ex(LUCI_Rectangle rect, float origin_x, float origin_y, float rotation, int thickness, LUCI_Color tint);
void renderer_draw_ring(float x, float y, float radius, int thickness, LUCI_Color tint);
void renderer_draw_triangles(const float *data, size_t size, LUCI_Color tint);
void renderer_use_font(LUCI_Font font);
void renderer_use_default_font(void);
void renderer_set_font_scale(float scale);
float renderer_get_font_scale(void);
void renderer_draw_text(const char* text, int x, int y, LUCI_Color tint);
void renderer_use_shader(LUCI_Shader shader);
void renderer_shader_set_bool(const char* name, bool value);
void renderer_shader_set_int(const char* name, int value);
void renderer_shader_set_float(const char* name, float value);
void renderer_shader_set_vec2(const char* name, float x, float y);
void renderer_shader_set_vec3(const char* name, float x, float y, float z);
void renderer_shader_set_vec4(const char* name, float x, float y, float z, float w);
void renderer_shader_set_mat2(const char* name, const float* values);
void renderer_shader_set_mat3(const char* name, const float* values);
void renderer_shader_set_mat4(const char* name, const float* values);
void renderer_set_viewport(int width, int height);
void renderer_begin(void);
void renderer_end(void);


LUCI_Texture texture_load(const char* path);
LUCI_Texture texture_create(const unsigned char* data, int width, int height, int channels);
LUCI_Font    load_font(const char* path, unsigned int size);
LUCI_Font    load_font_from_memory(const unsigned char* data, size_t data_size, unsigned int size);

unsigned char pixel9_ttf[];
unsigned int pixel9_ttf_len;

LUCI_Shader renderer_load_shader(const char* fs_src);

// Public API_____________________________________________________________________________________________________________________________
void LUCI_init(void) { 
    window_init();
    renderer_init(window_width(), window_height());
    renderer_set_default_font(load_font_from_memory(pixel9_ttf, pixel9_ttf_len, 18));
}
void LUCI_close(void) { window_close(); }

// Window
void LUCI_window_show(void) { window_show(); }
void LUCI_window_hide(void) { window_hide(); }
void LUCI_window_title(const char* title) { window_set_title(title); }
void LUCI_window_resize(uint32_t width, uint32_t height) { window_resize(width, height); }
void LUCI_window_position(int x, int y) { window_position(x, y); }
void LUCI_window_resizable(bool resizable) { window_set_resizable(resizable); }
void LUCI_window_always_on_top(bool always_on_top) { window_set_always_on_top(always_on_top); }
void LUCI_window_undecorated(bool undecorated) { window_set_undecorated(undecorated); }
bool LUCI_window_should_close(void) { return window_should_close(); }
int  LUCI_window_width(void)  { return window_width(); }
int  LUCI_window_height(void) { return window_height(); }

// Draw
void LUCI_begin_drawing(void) {
    window_begin();
    renderer_set_viewport(window_width(), window_height());
    renderer_begin();
}
void LUCI_end_drawing(void) {
    renderer_end();
    window_end();
}
void LUCI_draw_texture(LUCI_Texture texture, float x, float y, LUCI_Color color) { 
    renderer_draw_texture(texture, x, y, color); 
}
void LUCI_draw_texture_ex(LUCI_Texture texture, LUCI_Rectangle source, LUCI_Rectangle dest, float origin_x, float origin_y, float rotation, LUCI_Color color) {
    renderer_draw_texture_ex(texture, source, dest, origin_x, origin_y, rotation, color);
}
void LUCI_draw_rectangle(float x, float y, float width, float height, LUCI_Color color) {
    renderer_draw_rectangle(x, y, width, height, color);
}
void LUCI_draw_rectangle_ex(float x, float y, float width, float height, float origin_x, float origin_y, float rotation, LUCI_Color color) {
    renderer_draw_rectangle_ex(x, y, width, height, origin_x, origin_y, rotation, color);
}
void LUCI_draw_line(float x0, float y0, float x1, float y1, LUCI_Color color) {
    renderer_draw_line(x0, y0, x1, y1, color);
}
void LUCI_draw_line_ex(float x0, float y0, float x1, float y1, int thickness, LUCI_Color color) {
    renderer_draw_line_ex(x0, y0, x1, y1, thickness, color);
}
void LUCI_draw_circle(float center_x, float center_y, float radius, LUCI_Color color) {
    renderer_draw_circle(center_x, center_y, radius, color);
}
void LUCI_draw_ray(float origin_x, float origin_y, float dir_x, float dir_y, float length, LUCI_Color color) {
    renderer_draw_ray(origin_x, origin_y, dir_x, dir_y, length, color);
}
void LUCI_draw_ray_ex(float origin_x, float origin_y, float dir_x, float dir_y, float length, int thickness, LUCI_Color color) {
    renderer_draw_ray_ex(origin_x, origin_y, dir_x, dir_y, length, thickness, color);
}
void LUCI_draw_text(const char* text, float x, float y, LUCI_Color color) {
    renderer_draw_text(text, x, y, color);
}
void LUCI_draw_triangles(const float *data, size_t size, LUCI_Color color) {
    renderer_draw_triangles(data, size, color);
}

void LUCI_draw_box(float x, float y, float width, float height, LUCI_Color color) {
    renderer_draw_box_ex((LUCI_Rectangle){ x, y, width, height }, 0.0f, 0.0f, 0.0f, 1.0f, color);
}

void LUCI_draw_box_ex(LUCI_Rectangle rect, float origin_x, float origin_y, float rotation, int thickness, LUCI_Color color) {
    renderer_draw_box_ex(rect, origin_x, origin_y, rotation, thickness, color);
}

void LUCI_draw_ring(float x, float y, float radius, int thickness, LUCI_Color color) {
    renderer_draw_ring(x, y, radius, thickness, color);
}

// Input: Keyboard
bool LUCI_is_key_pressed(int key)  { return window_is_key_pressed(key); }
bool LUCI_is_key_down(int key)     { return window_is_key_down(key); }
bool LUCI_is_key_released(int key) { return window_is_key_released(key); }
bool LUCI_is_key_up(int key)       { return window_is_key_up(key); }
int  LUCI_get_key_pressed(void)    { return window_get_key_pressed(); }
void LUCI_set_exit_key(int key)    { window_set_exit_key(key); }

// Input: Mouse
bool  LUCI_is_mouse_button_pressed(int b)    { return window_is_mouse_button_pressed(b); }
bool  LUCI_is_mouse_button_down(int b)       { return window_is_mouse_button_down(b); }
bool  LUCI_is_mouse_button_released(int b)   { return window_is_mouse_button_released(b); }
bool  LUCI_is_mouse_button_up(int b)         { return window_is_mouse_button_up(b); }
int   LUCI_get_mouse_x(void)                 { return window_get_mouse_x(); }
int   LUCI_get_mouse_y(void)                 { return window_get_mouse_y(); }
void  LUCI_get_mouse_position(int *x,int *y) { window_get_mouse_position(x,y); }
void  LUCI_get_mouse_delta(float *x,float *y){ window_get_mouse_delta(x,y); }
float LUCI_get_mouse_wheel(void)             { return window_get_mouse_wheel(); }
float LUCI_get_mouse_wheel_x(void)           { return window_get_mouse_wheel_x(); }
float LUCI_get_mouse_wheel_y(void)           { return window_get_mouse_wheel_y(); }
bool  LUCI_is_mouse_inside(void)             { return window_is_mouse_inside(); }

// Input: Gamepad
bool        LUCI_is_gamepad_available(int g)              { return window_is_gamepad_available(g); }
const char *LUCI_get_gamepad_name(int g)                  { return window_get_gamepad_name(g); }
bool        LUCI_is_gamepad_button_pressed(int g,int b)   { return window_is_gamepad_button_pressed(g,b); }
bool        LUCI_is_gamepad_button_down(int g,int b)      { return window_is_gamepad_button_down(g,b); }
bool        LUCI_is_gamepad_button_released(int g,int b)  { return window_is_gamepad_button_released(g,b); }
bool        LUCI_is_gamepad_button_up(int g,int b)        { return window_is_gamepad_button_up(g,b); }
int         LUCI_get_gamepad_button_pressed(void)         { return window_get_gamepad_button_pressed(); }
int         LUCI_get_gamepad_axis_count(int g)            { return window_get_gamepad_axis_count(g); }
float       LUCI_get_gamepad_axis_movement(int g,int a)   { return window_get_gamepad_axis_movement(g,a); }

// Texture & Font
LUCI_Texture LUCI_load_texture(const char* path) { 
    return texture_load(path); 
}
LUCI_Texture LUCI_create_texture(const unsigned char* data, int width, int height, int channels) { 
    return texture_create(data, width, height, channels); 
}
LUCI_Font LUCI_load_font(const char* path, unsigned int size) { 
    return load_font(path, size); 
}
LUCI_Font LUCI_load_font_from_memory(const unsigned char* data, size_t data_size, unsigned int size) { 
    return load_font_from_memory(data, data_size, size);
}
void LUCI_use_font(LUCI_Font font) { 
    renderer_use_font(font); 
}
void LUCI_use_default_font(void) {
    renderer_use_default_font();
}
void LUCI_set_font_scale(float scale) {
    renderer_set_font_scale(scale);
}
float LUCI_get_font_scale(void) {
    return renderer_get_font_scale();
}

// Shader
LUCI_Shader LUCI_load_shader(const char* fs_src) {
    return renderer_load_shader(fs_src);
}
void LUCI_use_shader(LUCI_Shader shader) {
    renderer_use_shader(shader);
}
void LUCI_reset_shader(void) {
    renderer_use_shader((LUCI_Shader){0});
}
void LUCI_shader_set_bool(const char* name, bool value)  { renderer_shader_set_bool(name, value); }
void LUCI_shader_set_int(const char* name, int value)    { renderer_shader_set_int(name, value); }
void LUCI_shader_set_float(const char* name, float value){ renderer_shader_set_float(name, value); }
void LUCI_shader_set_vec2(const char* name, float x, float y) { renderer_shader_set_vec2(name, x, y); }
void LUCI_shader_set_vec3(const char* name, float x, float y, float z) { renderer_shader_set_vec3(name, x, y, z); }
void LUCI_shader_set_vec4(const char* name, float x, float y, float z, float w) { renderer_shader_set_vec4(name, x, y, z, w); }
void LUCI_shader_set_mat2(const char* name, const float* v) { renderer_shader_set_mat2(name, v); }
void LUCI_shader_set_mat3(const char* name, const float* v) { renderer_shader_set_mat3(name, v); }
void LUCI_shader_set_mat4(const char* name, const float* v) { renderer_shader_set_mat4(name, v); }

#include "window.c"
#include "renderer.c"
#include "texture.c"
#include "font.c"
#include "shader.c"
