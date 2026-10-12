#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct LUCI_Texture {
    unsigned int id;
    int width;
    int height;
    int format;
} LUCI_Texture;

typedef struct LUCI_Rectangle {
    float x;
    float y;
    float width;
    float height;
} LUCI_Rectangle;

typedef struct LUCI_Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} LUCI_Color;

typedef struct LUCI_Glyph {
    uint32_t code;
    int x, y;
    int w, h;
    int bearing_x;
    int bearing_y;
    float advance; 
} LUCI_Glyph;

typedef struct LUCI_Font {
    LUCI_Texture texture;
    LUCI_Glyph* glyphs;
    size_t length;
    int ascent;
    int line_height;
    int font_size;
} LUCI_Font;

typedef struct LUCI_Shader {
    unsigned int id;
} LUCI_Shader;

typedef LUCI_Shader Shader;

#define LUCI_COLOR_WHITE       ((LUCI_Color){ 255, 255, 255, 255 })
#define LUCI_COLOR_BLACK       ((LUCI_Color){ 0, 0, 0, 255 })
#define LUCI_COLOR_RED         ((LUCI_Color){ 255, 0, 0, 255 })
#define LUCI_COLOR_GREEN       ((LUCI_Color){ 0, 255, 0, 255 })
#define LUCI_COLOR_BLUE        ((LUCI_Color){ 0, 0, 255, 255 })
#define LUCI_COLOR_YELLOW      ((LUCI_Color){ 255, 255, 0, 255 })
#define LUCI_COLOR_ORANGE      ((LUCI_Color){ 255, 165, 0, 255 })
#define LUCI_COLOR_PURPLE      ((LUCI_Color){ 128, 0, 128, 255 })
#define LUCI_COLOR_CYAN        ((LUCI_Color){ 0, 255, 255, 255 })
#define LUCI_COLOR_MAGENTA     ((LUCI_Color){ 255, 0, 255, 255 })
#define LUCI_COLOR_PINK        ((LUCI_Color){ 255, 192, 203, 255 })
#define LUCI_COLOR_BROWN       ((LUCI_Color){ 139, 69, 19, 255 })
#define LUCI_COLOR_LIME        ((LUCI_Color){ 50, 205, 50, 255 })
#define LUCI_COLOR_GRAY        ((LUCI_Color){ 128, 128, 128, 255 })
#define LUCI_COLOR_LIGHT_GRAY  ((LUCI_Color){ 200, 200, 200, 255 })
#define LUCI_COLOR_DARK_GRAY   ((LUCI_Color){ 50, 50, 50, 255 })


typedef enum LUCI_KeyboardKey {
    LUCI_KEY_NULL         = 0,
    LUCI_KEY_ESCAPE       = 27,
    LUCI_KEY_BACKTICK     = 96,
    LUCI_KEY_0            = 48,
    LUCI_KEY_1            = 49,
    LUCI_KEY_2            = 50,
    LUCI_KEY_3            = 51,
    LUCI_KEY_4            = 52,
    LUCI_KEY_5            = 53,
    LUCI_KEY_6            = 54,
    LUCI_KEY_7            = 55,
    LUCI_KEY_8            = 56,
    LUCI_KEY_9            = 57,
    LUCI_KEY_MINUS        = 45,
    LUCI_KEY_EQUAL        = 61,
    LUCI_KEY_BACKSPACE    = 8,
    LUCI_KEY_TAB          = 9,
    LUCI_KEY_SPACE        = 32,
    LUCI_KEY_A            = 97,
    LUCI_KEY_B            = 98,
    LUCI_KEY_C            = 99,
    LUCI_KEY_D            = 100,
    LUCI_KEY_E            = 101,
    LUCI_KEY_F            = 102,
    LUCI_KEY_G            = 103,
    LUCI_KEY_H            = 104,
    LUCI_KEY_I            = 105,
    LUCI_KEY_J            = 106,
    LUCI_KEY_K            = 107,
    LUCI_KEY_L            = 108,
    LUCI_KEY_M            = 109,
    LUCI_KEY_N            = 110,
    LUCI_KEY_O            = 111,
    LUCI_KEY_P            = 112,
    LUCI_KEY_Q            = 113,
    LUCI_KEY_R            = 114,
    LUCI_KEY_S            = 115,
    LUCI_KEY_T            = 116,
    LUCI_KEY_U            = 117,
    LUCI_KEY_V            = 118,
    LUCI_KEY_W            = 119,
    LUCI_KEY_X            = 120,
    LUCI_KEY_Y            = 121,
    LUCI_KEY_Z            = 122,
    LUCI_KEY_PERIOD       = 46,
    LUCI_KEY_COMMA        = 44,
    LUCI_KEY_SLASH        = 47,
    LUCI_KEY_BRACKET      = 91,
    LUCI_KEY_CLOSE_BRACKET= 93,
    LUCI_KEY_SEMICOLON    = 59,
    LUCI_KEY_APOSTROPHE   = 39,
    LUCI_KEY_BACKSLASH    = 92,
    LUCI_KEY_ENTER        = 10,
    LUCI_KEY_RETURN       = 10,
    LUCI_KEY_DELETE       = 127,
    LUCI_KEY_F1           = 128,
    LUCI_KEY_F2           = 129,
    LUCI_KEY_F3           = 130,
    LUCI_KEY_F4           = 131,
    LUCI_KEY_F5           = 132,
    LUCI_KEY_F6           = 133,
    LUCI_KEY_F7           = 134,
    LUCI_KEY_F8           = 135,
    LUCI_KEY_F9           = 136,
    LUCI_KEY_F10          = 137,
    LUCI_KEY_F11          = 138,
    LUCI_KEY_F12          = 139,
    LUCI_KEY_F13          = 140,
    LUCI_KEY_F14          = 141,
    LUCI_KEY_F15          = 142,
    LUCI_KEY_F16          = 143,
    LUCI_KEY_F17          = 144,
    LUCI_KEY_F18          = 145,
    LUCI_KEY_F19          = 146,
    LUCI_KEY_F20          = 147,
    LUCI_KEY_F21          = 148,
    LUCI_KEY_F22          = 149,
    LUCI_KEY_F23          = 150,
    LUCI_KEY_F24          = 151,
    LUCI_KEY_F25          = 152,
    LUCI_KEY_CAPS_LOCK    = 153,
    LUCI_KEY_LEFT_SHIFT   = 154,
    LUCI_KEY_LEFT_CONTROL = 155,
    LUCI_KEY_LEFT_ALT     = 156,
    LUCI_KEY_LEFT_SUPER   = 157,
    LUCI_KEY_RIGHT_SHIFT  = 158,
    LUCI_KEY_RIGHT_CONTROL= 159,
    LUCI_KEY_RIGHT_ALT    = 160,
    LUCI_KEY_RIGHT_SUPER  = 161,
    LUCI_KEY_UP           = 162,
    LUCI_KEY_DOWN         = 163,
    LUCI_KEY_LEFT         = 164,
    LUCI_KEY_RIGHT        = 165,
    LUCI_KEY_INSERT       = 166,
    LUCI_KEY_MENU         = 167,
    LUCI_KEY_END          = 168,
    LUCI_KEY_HOME         = 169,
    LUCI_KEY_PAGE_UP      = 170,
    LUCI_KEY_PAGE_DOWN    = 171,
    LUCI_KEY_NUM_LOCK     = 172,
    LUCI_KEY_KP_SLASH     = 173,
    LUCI_KEY_KP_MULTIPLY  = 174,
    LUCI_KEY_KP_ADD       = 175,
    LUCI_KEY_KP_SUBTRACT  = 176,
    LUCI_KEY_KP_EQUAL     = 177,
    LUCI_KEY_KP_1         = 178,
    LUCI_KEY_KP_2         = 179,
    LUCI_KEY_KP_3         = 180,
    LUCI_KEY_KP_4         = 181,
    LUCI_KEY_KP_5         = 182,
    LUCI_KEY_KP_6         = 183,
    LUCI_KEY_KP_7         = 184,
    LUCI_KEY_KP_8         = 185,
    LUCI_KEY_KP_9         = 186,
    LUCI_KEY_KP_0         = 187,
    LUCI_KEY_KP_DECIMAL   = 188,
    LUCI_KEY_KP_ENTER     = 189,
    LUCI_KEY_SCROLL_LOCK  = 190,
    LUCI_KEY_PRINT_SCREEN = 191,
    LUCI_KEY_PAUSE        = 192,
    LUCI_KEY_WORLD_1      = 193,
    LUCI_KEY_WORLD_2      = 194,
    LUCI_KEY_LAST         = 256
} LUCI_KeyboardKey;

typedef LUCI_KeyboardKey LUCI_Key;

typedef enum LUCI_MouseButton {
    LUCI_MOUSE_BUTTON_LEFT    = 0,
    LUCI_MOUSE_BUTTON_MIDDLE  = 1,
    LUCI_MOUSE_BUTTON_RIGHT   = 2,
    LUCI_MOUSE_BUTTON_SIDE    = 3,
    LUCI_MOUSE_BUTTON_EXTRA   = 4,
    LUCI_MOUSE_BUTTON_FORWARD = 3,
    LUCI_MOUSE_BUTTON_BACK    = 4,
    LUCI_MOUSE_LEFT           = 0,
    LUCI_MOUSE_MIDDLE         = 1,
    LUCI_MOUSE_RIGHT          = 2
} LUCI_MouseButton;

typedef enum LUCI_GamepadButton {
    LUCI_GAMEPAD_BUTTON_UNKNOWN          = -1,
    LUCI_GAMEPAD_BUTTON_LEFT_FACE_UP     = 0,
    LUCI_GAMEPAD_BUTTON_LEFT_FACE_RIGHT  = 1,
    LUCI_GAMEPAD_BUTTON_LEFT_FACE_DOWN   = 2,
    LUCI_GAMEPAD_BUTTON_LEFT_FACE_LEFT   = 3,
    LUCI_GAMEPAD_BUTTON_RIGHT_FACE_UP    = 4,
    LUCI_GAMEPAD_BUTTON_RIGHT_FACE_RIGHT = 5,
    LUCI_GAMEPAD_BUTTON_RIGHT_FACE_DOWN  = 6,
    LUCI_GAMEPAD_BUTTON_RIGHT_FACE_LEFT  = 7,
    LUCI_GAMEPAD_BUTTON_LEFT_TRIGGER_1   = 8,
    LUCI_GAMEPAD_BUTTON_LEFT_TRIGGER_2   = 9,
    LUCI_GAMEPAD_BUTTON_RIGHT_TRIGGER_1  = 10,
    LUCI_GAMEPAD_BUTTON_RIGHT_TRIGGER_2  = 11,
    LUCI_GAMEPAD_BUTTON_MIDDLE_LEFT      = 12,
    LUCI_GAMEPAD_BUTTON_MIDDLE           = 13,
    LUCI_GAMEPAD_BUTTON_MIDDLE_RIGHT     = 14,
    LUCI_GAMEPAD_BUTTON_LEFT_THUMB       = 15,
    LUCI_GAMEPAD_BUTTON_RIGHT_THUMB      = 16
} LUCI_GamepadButton;

typedef enum LUCI_GamepadAxis {
    LUCI_GAMEPAD_AXIS_LEFT_X        = 0,
    LUCI_GAMEPAD_AXIS_LEFT_Y        = 1,
    LUCI_GAMEPAD_AXIS_RIGHT_X       = 2,
    LUCI_GAMEPAD_AXIS_RIGHT_Y       = 3,
    LUCI_GAMEPAD_AXIS_LEFT_TRIGGER  = 4,
    LUCI_GAMEPAD_AXIS_RIGHT_TRIGGER = 5
} LUCI_GamepadAxis;

// Window
void LUCI_init();
void LUCI_close();
void LUCI_window_hide();
void LUCI_window_show();
void LUCI_window_title(const char* title);
void LUCI_window_resize(uint32_t width, uint32_t height);
void LUCI_window_position(int x, int y);
void LUCI_window_resizable(bool resizable);
void LUCI_window_always_on_top(bool always_on_top);
void LUCI_window_undecorated(bool undecorated);
bool LUCI_window_should_close(void);
int  LUCI_window_width(void);
int  LUCI_window_height(void);

// Draw
void LUCI_begin_drawing(void);
void LUCI_end_drawing(void);
void LUCI_draw_texture(LUCI_Texture texture, float x, float y, LUCI_Color color);
/**
 * Draw a texture with extended parameters.
 *
 * @param texture     Texture to draw
 * @param source      Source rectangle (texture coordinates)
 * @param dest        Destination rectangle (screen coordinates)
 * @param origin_x    Origin offset in dest rect: (0,0) = top-left, (width/2,height/2) = center
 * @param origin_y    Origin offset in dest rect: (0,0) = top-left, (width/2,height/2) = center
 * @param rotation    Rotation angle in radians (clockwise, as Y increases downward)
 * @param color       Tint color
 */
void LUCI_draw_texture_ex(LUCI_Texture texture, LUCI_Rectangle source, LUCI_Rectangle dest, float origin_x, float origin_y, float rotation, LUCI_Color color);
void LUCI_draw_rectangle(float x, float y, float width, float height, LUCI_Color color);
void LUCI_draw_rectangle_ex(float x, float y, float width, float height, float origin_x, float origin_y, float rotation, LUCI_Color color);
void LUCI_draw_line(float x0, float y0, float x1, float y1, LUCI_Color color);
void LUCI_draw_line_ex(float x0, float y0, float x1, float y1, int thickness, LUCI_Color color);
void LUCI_draw_circle(float center_x, float center_y, float radius, LUCI_Color color);
void LUCI_draw_ray(float origin_x, float origin_y, float dir_x, float dir_y, float length, LUCI_Color color);
void LUCI_draw_ray_ex(float origin_x, float origin_y, float dir_x, float dir_y, float length, int thickness, LUCI_Color color);
void LUCI_draw_text(const char* text, float x, float y, LUCI_Color color);
void LUCI_draw_triangles(const float *data, size_t size, LUCI_Color color);
void LUCI_draw_box(float x, float y, float width, float height, LUCI_Color color);
void LUCI_draw_box_ex(LUCI_Rectangle rect, float origin_x, float origin_y, float rotation, int thickness, LUCI_Color color);
void LUCI_draw_ring(float x, float y, float radius, int thickness, LUCI_Color color);

// Input: Keyboard
bool LUCI_is_key_pressed(int key);
bool LUCI_is_key_down(int key);
bool LUCI_is_key_released(int key);
bool LUCI_is_key_up(int key);
int  LUCI_get_key_pressed(void);
void LUCI_set_exit_key(int key);

// Input: Mouse
bool  LUCI_is_mouse_button_pressed(int button);
bool  LUCI_is_mouse_button_down(int button);
bool  LUCI_is_mouse_button_released(int button);
bool  LUCI_is_mouse_button_up(int button);
int   LUCI_get_mouse_x(void);
int   LUCI_get_mouse_y(void);
void  LUCI_get_mouse_position(int *x, int *y);
void  LUCI_get_mouse_delta(float *x, float *y);
float LUCI_get_mouse_wheel(void);
float LUCI_get_mouse_wheel_x(void);
float LUCI_get_mouse_wheel_y(void);
bool  LUCI_is_mouse_inside(void);

// Input: Gamepad
bool  LUCI_is_gamepad_available(int gamepad);
const char *LUCI_get_gamepad_name(int gamepad);
bool  LUCI_is_gamepad_button_pressed(int gamepad, int button);
bool  LUCI_is_gamepad_button_down(int gamepad, int button);
bool  LUCI_is_gamepad_button_released(int gamepad, int button);
bool  LUCI_is_gamepad_button_up(int gamepad, int button);
int   LUCI_get_gamepad_button_pressed(void);
int   LUCI_get_gamepad_axis_count(int gamepad);
float LUCI_get_gamepad_axis_movement(int gamepad, int axis);

// Texture & Font
LUCI_Texture LUCI_load_texture(const char* path);
LUCI_Texture LUCI_create_texture(const unsigned char* data, int width, int height, int channels);

LUCI_Font LUCI_load_font(const char* path, unsigned int size);
LUCI_Font LUCI_load_font_from_memory(const unsigned char* data, size_t data_size, unsigned int size);

void  LUCI_use_font(LUCI_Font font);
void  LUCI_use_default_font(void);
void  LUCI_set_font_scale(float scale);
float LUCI_get_font_scale(void);

// Shader
LUCI_Shader LUCI_load_shader(const char* fs_src);

void LUCI_use_shader(LUCI_Shader shader);
void LUCI_reset_shader(void);

void LUCI_shader_set_bool(const char* name, bool value);
void LUCI_shader_set_int(const char* name, int value);
void LUCI_shader_set_float(const char* name, float value);
void LUCI_shader_set_vec2(const char* name, float x, float y);
void LUCI_shader_set_vec3(const char* name, float x, float y, float z);
void LUCI_shader_set_vec4(const char* name, float x, float y, float z, float w);
void LUCI_shader_set_mat2(const char* name, const float* values);
void LUCI_shader_set_mat3(const char* name, const float* values);
void LUCI_shader_set_mat4(const char* name, const float* values);
