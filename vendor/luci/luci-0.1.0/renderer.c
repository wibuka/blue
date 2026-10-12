#include "luci.h"
#include <stddef.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <glad/glad.h>

#define MAX_SPRITE 1024
#define MAX_VERTEX (MAX_SPRITE * 4 * 9)
#define MAX_INDEX (MAX_SPRITE * 6)
#define FLOATS_PER_VERTEX 9
#define uint unsigned int

typedef struct {
    float x, y, z;
    float u, v;
    LUCI_Color color;
} Vertex;

const char* VERTEX_SHADER_SOURCE;
const char* FRAGMENT_SHADER_SOURCE;
const char* CIRCLE_FRAGMENT_SHADER_SOURCE;

static uint VBO, VAO, EBO;
static uint defaultShaderProgram;
static uint circleShaderProgram;
static uint currentShader;
static uint activeShader;
static LUCI_Texture white_texture = {0};
static uint current_texture_id = 0;

static int window_w = 1024;
static int window_h = 1024;

static Vertex vertex_buffer[MAX_SPRITE * 4];
static uint index_buffer[MAX_INDEX];
static uint quad_count = 0;

static LUCI_Font default_font = {0};
static LUCI_Font current_font = {0};
static float font_scale = 1.0f;

static void quad_push(float x, float y, float width, float height, float origin_x, float origin_y, float rotation, LUCI_Rectangle src, int tex_w, int tex_h, LUCI_Color tint) {
    float c = cosf(rotation);
    float s = sinf(rotation);

    float u0 = src.x / (float)tex_w;
    float u1 = (src.x + src.width) / (float)tex_w;
    float v0 = src.y / (float)tex_h;
    float v1 = (src.y + src.height) / (float)tex_h;

    // Local corners in destination-rectangle space: (0, 0) = top-left,
    // (width, height) = bottom-right. Y increases downward (screen space).
    float local[4][2] = {
        { 0.0f, 0.0f },
        { width, 0.0f },
        { width, height },
        { 0.0f, height }
    };

    // UVs for each vertex: [0]=top-left, [1]=top-right, [2]=bottom-right, [3]=bottom-left.
    // Screen-space Y is downward; the source rectangle has been flipped
    // (renderer_draw_texture_ex) to convert LUCI top-left texture coords to
    // OpenGL bottom-left texture coords, so the image is drawn right-side up.
    float uv[4][2] = {
        { u0, v1 },
        { u1, v1 },
        { u1, v0 },
        { u0, v0 }
    };

    for (int i = 0; i < 4; i++) {
        // Rotate the corner around the pivot (origin_x, origin_y), which stays
        // fixed in world space.
        float dx = (local[i][0] - origin_x) * c - (local[i][1] - origin_y) * s;
        float dy = (local[i][0] - origin_x) * s + (local[i][1] - origin_y) * c;
        float px = x + origin_x + dx;
        float py = y + origin_y + dy;

        float ndc_x = (px / window_w) * 2.0f - 1.0f;
        float ndc_y = 1.0f - (py / window_h) * 2.0f;

        Vertex* v = &vertex_buffer[quad_count * 4 + i];
        v->x = ndc_x;
        v->y = ndc_y;
        v->z = 0.0f;
        v->u = uv[i][0];
        v->v = uv[i][1];
        v->color = tint;
    }

    quad_count++;
}

static void glyph_push(float x0, float y0, float w, float h, LUCI_Glyph g, int tex_w, int tex_h, LUCI_Color tint) {
    float u0 = (g.x + 0.5f) / (float)tex_w;
    float v0 = (g.y + 0.5f) / (float)tex_h;
    float u1 = (g.x + (float)w - 0.5f) / (float)tex_w;
    float v1 = (g.y + (float)h - 0.5f) / (float)tex_h;
    if (w <= 1) { u0 = (g.x + (float)w * 0.5f) / (float)tex_w; u1 = u0; }
    if (h <= 1) { v0 = (g.y + (float)h * 0.5f) / (float)tex_h; v1 = v0; }

    float x1 = x0 + (float)w;
    float y1 = y0 + (float)h;

    float xs[4] = { x0, x1, x1, x0 };
    float ys[4] = { y0, y0, y1, y1 };
    float us[4] = { u0, u1, u1, u0 };
    float vs[4] = { v1, v1, v0, v0 };

    for (int i = 0; i < 4; i++) {
        float ndc_x = (xs[i] / window_w) * 2.0f - 1.0f;
        float ndc_y = 1.0f - (ys[i] / window_h) * 2.0f;
        Vertex* v = &vertex_buffer[quad_count * 4 + i];
        v->x = ndc_x;
        v->y = ndc_y;
        v->z = 0.0f;
        v->u = us[i];
        v->v = vs[i];
        v->color = tint;
    }
    quad_count++;
}

static void flush_batch(void) {
    if (quad_count == 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, quad_count * 4 * sizeof(Vertex), vertex_buffer);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glDrawElements(GL_TRIANGLES, quad_count * 6, GL_UNSIGNED_INT, NULL);

    quad_count = 0;
    current_texture_id = 0;
}

static void use_shader(uint program) {
    if (currentShader != program) {
        flush_batch();
        glUseProgram(program);
        currentShader = program;
    }
}

static void use_texture(LUCI_Texture texture) {
    uint texture_id = texture.id;
    if (current_texture_id != texture_id) {
        flush_batch();
        glBindTexture(GL_TEXTURE_2D, texture_id);
        current_texture_id = texture_id;
    }
}

static LUCI_Texture create_white_texture(void) {
    unsigned char white_pixel[4] = { 255, 255, 255, 255 };
    LUCI_Texture texture = { 0 };

    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    texture.width = 1;
    texture.height = 1;
    texture.format = GL_RGBA;
    return texture;
}

void renderer_set_viewport(int width, int height) {
    if (width > 0)  window_w = width;
    if (height > 0) window_h = height;
}

void renderer_begin(void) {
    glViewport(0, 0, window_w, window_h);
    glClear(GL_COLOR_BUFFER_BIT);
    activeShader = defaultShaderProgram;
    if (currentShader != defaultShaderProgram && defaultShaderProgram != 0) {
        glUseProgram(defaultShaderProgram);
        currentShader = defaultShaderProgram;
    }
}

void renderer_set_default_font(LUCI_Font font) {
    default_font = font;
    current_font = default_font;
}

void renderer_end(void) {
    flush_batch();
}

void renderer_init(int width, int height) {
    white_texture = create_white_texture();

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    for (uint i = 0; i < MAX_SPRITE; i++) {
        uint base = i * 4;
        index_buffer[i * 6 + 0] = base + 0;
        index_buffer[i * 6 + 1] = base + 1;
        index_buffer[i * 6 + 2] = base + 2;
        index_buffer[i * 6 + 3] = base + 2;
        index_buffer[i * 6 + 4] = base + 3;
        index_buffer[i * 6 + 5] = base + 0;
    }

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer), NULL, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer), index_buffer, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    uint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &VERTEX_SHADER_SOURCE, NULL);
    glCompileShader(vertexShader);

    uint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &FRAGMENT_SHADER_SOURCE, NULL);
    glCompileShader(fragmentShader);

    defaultShaderProgram = glCreateProgram();
    glAttachShader(defaultShaderProgram, vertexShader);
    glAttachShader(defaultShaderProgram, fragmentShader);
    glLinkProgram(defaultShaderProgram);

    glUseProgram(defaultShaderProgram);
    currentShader = defaultShaderProgram;
    activeShader = defaultShaderProgram;

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    uint vs2 = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs2, 1, &VERTEX_SHADER_SOURCE, NULL);
    glCompileShader(vs2);

    uint fs2 = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs2, 1, &CIRCLE_FRAGMENT_SHADER_SOURCE, NULL);
    glCompileShader(fs2);

    circleShaderProgram = glCreateProgram();
    glAttachShader(circleShaderProgram, vs2);
    glAttachShader(circleShaderProgram, fs2);
    glLinkProgram(circleShaderProgram);

    glDeleteShader(vs2);
    glDeleteShader(fs2);

    glUseProgram(defaultShaderProgram);
    currentShader = defaultShaderProgram;
    activeShader = defaultShaderProgram;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void renderer_draw_texture_ex(LUCI_Texture texture, LUCI_Rectangle source, LUCI_Rectangle dest, float origin_x, float origin_y, float rotation, LUCI_Color tint) {
    if (texture.id == 0) return;
    use_shader(activeShader);
    use_texture(texture);
    if (quad_count >= MAX_SPRITE) flush_batch();
    LUCI_Rectangle flipped = {
        source.x,
        (float)texture.height - source.y - source.height,
        source.width,
        source.height
    };
    quad_push(dest.x - origin_x, dest.y - origin_y, dest.width, dest.height, origin_x, origin_y, rotation, flipped, texture.width, texture.height, tint);
}

void renderer_draw_texture(LUCI_Texture texture, float x, float y, LUCI_Color tint) {
    renderer_draw_texture_ex(texture, (LUCI_Rectangle){ 0, 0, (float)texture.width, (float)texture.height }, (LUCI_Rectangle){ 0, 0, (float)texture.width, (float)texture.height }, 0.0f, 0.0f, 0.0f, tint);
}

void renderer_draw_rectangle_ex(float x, float y, float width, float height, float origin_x, float origin_y, float rotation, LUCI_Color tint) {
    use_shader(activeShader);
    use_texture(white_texture);
    if (quad_count >= MAX_SPRITE) flush_batch();
    float c = cosf(rotation);
    float s = sinf(rotation);

    float local[4][2] = {
        { 0.0f, 0.0f },
        { width, 0.0f },
        { width, height },
        { 0.0f, height }
    };
    float uvs[4][2] = { {0,1}, {1,1}, {1,0}, {0,0} };
    for (int i = 0; i < 4; i++) {
        float dx = (local[i][0] - origin_x) * c - (local[i][1] - origin_y) * s;
        float dy = (local[i][0] - origin_x) * s + (local[i][1] - origin_y) * c;
        float px = x + origin_x + dx;
        float py = y + origin_y + dy;
        float ndc_x = (px / window_w) * 2.0f - 1.0f;
        float ndc_y = 1.0f - (py / window_h) * 2.0f;
        Vertex* v = &vertex_buffer[quad_count * 4 + i];
        v->x = ndc_x; v->y = ndc_y; v->z = 0.0f;
        v->u = uvs[i][0]; v->v = uvs[i][1];
        v->color = tint;
    }
    quad_count++;
}

void renderer_draw_rectangle(float x, float y, float width, float height, LUCI_Color tint) {
    renderer_draw_rectangle_ex(x, y, width, height, 0.0f, 0.0f, 0.0f, tint);
}

void renderer_draw_line_ex(float x0, float y0, float x1, float y1, int thickness, LUCI_Color tint) {
    if (thickness <= 0) return;
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;

    use_shader(activeShader);
    use_texture(white_texture);
    if (quad_count >= MAX_SPRITE) flush_batch();

    float nx = -dy / len * (thickness * 0.5f);
    float ny =  dx / len * (thickness * 0.5f);

    float px[4] = { x0 + nx, x0 - nx, x1 - nx, x1 + nx };
    float py[4] = { y0 + ny, y0 - ny, y1 - ny, y1 + ny };

    for (int i = 0; i < 4; i++) {
        float ndc_x = (px[i] / window_w) * 2.0f - 1.0f;
        float ndc_y = 1.0f - (py[i] / window_h) * 2.0f;
        Vertex* v = &vertex_buffer[quad_count * 4 + i];
        v->x = ndc_x;
        v->y = ndc_y;
        v->z = 0.0f;
        v->u = (i == 0 || i == 3) ? 0.0f : 1.0f;
        v->v = (i < 2) ? 1.0f : 0.0f;
        v->color = tint;
    }
    quad_count++;
}

void renderer_draw_line(float x0, float y0, float x1, float y1, LUCI_Color tint) {
    renderer_draw_line_ex(x0, y0, x1, y1, 1, tint);
}

void renderer_draw_circle(float center_x, float center_y, float radius, LUCI_Color tint) {
    if (radius <= 0.0f) return;
    bool using_default = (activeShader == defaultShaderProgram);
    use_shader(using_default ? circleShaderProgram : activeShader);
    use_texture(white_texture);
    if (quad_count >= MAX_SPRITE) flush_batch();
    float x = (float)center_x - radius;
    float y = (float)center_y - radius;
    float size = radius * 2.0f;
    float cx = x + radius;
    float cy = y + radius;
    float hw = size / 2.0f;
    float hh = size / 2.0f;
    float corners[4][2] = { { -hw, -hh }, { hw, -hh }, { hw, hh }, { -hw, hh } };
    float uvs[4][2] = { {0,1}, {1,1}, {1,0}, {0,0} };
    for (int i = 0; i < 4; i++) {
        float dx = corners[i][0];
        float dy = corners[i][1];
        float px = cx + dx;
        float py = cy + dy;
        float ndc_x = (px / window_w) * 2.0f - 1.0f;
        float ndc_y = 1.0f - (py / window_h) * 2.0f;
        Vertex* v = &vertex_buffer[quad_count * 4 + i];
        v->x = ndc_x; v->y = ndc_y; v->z = 0.0f;
        v->u = uvs[i][0]; v->v = uvs[i][1];
        v->color = tint;
    }
    quad_count++;
}

void renderer_draw_ray_ex(float origin_x, float origin_y, float dir_x, float dir_y, float length, int thickness, LUCI_Color tint) {
    float len = sqrtf(dir_x * dir_x + dir_y * dir_y);
    if (len < 0.0001f || length <= 0.0f) return;
    float nx = dir_x / len;
    float ny = dir_y / len;
    float x1 = origin_x + nx * length;
    float y1 = origin_y + ny * length;
    renderer_draw_line_ex(origin_x, origin_y, x1, y1, thickness, tint);
}

void renderer_draw_ray(float origin_x, float origin_y, float dir_x, float dir_y, float length, LUCI_Color tint) {
    renderer_draw_ray_ex(origin_x, origin_y, dir_x, dir_y, length, 1, tint);
}

void renderer_draw_ring(float x, float y, float radius, int thickness, LUCI_Color tint) {
    if (radius <= 0.0f) return;
    if (thickness <= 0 || thickness >= radius) {
        renderer_draw_circle(x, y, radius, tint);
        return;
    }
    
    use_shader(activeShader);
    use_texture(white_texture);
    if (quad_count >= MAX_SPRITE) flush_batch();
    
    float inner_ratio = 1.0f - thickness / radius;
    GLint loc = glGetUniformLocation(activeShader, "inner_ratio");
    if (loc != -1) {
        glUniform1f(loc, inner_ratio);
    }
    
    float cx = x;
    float cy = y;
    float corners[4][2] = { { -radius, -radius }, { radius, -radius }, { radius, radius }, { -radius, radius } };
    float uvs[4][2] = { {0,1}, {1,1}, {1,0}, {0,0} };
    for (int i = 0; i < 4; i++) {
        float dx = corners[i][0];
        float dy = corners[i][1];
        float px = cx + dx;
        float py = cy + dy;
        float ndc_x = (px / window_w) * 2.0f - 1.0f;
        float ndc_y = 1.0f - (py / window_h) * 2.0f;
        Vertex* v = &vertex_buffer[quad_count * 4 + i];
        v->x = ndc_x; v->y = ndc_y; v->z = 0.0f;
        v->u = uvs[i][0]; v->v = uvs[i][1];
        v->color = tint;
    }
    quad_count++;
}

void renderer_draw_box_ex(LUCI_Rectangle rect, float origin_x, float origin_y, float rotation, int thickness, LUCI_Color tint) {
    if (rect.width <= 0 || rect.height <= 0 || thickness <= 0) return;
    
    float hw = rect.width / 2.0f;
    float hh = rect.height / 2.0f;
    
    if (thickness >= hw || thickness >= hh) {
        renderer_draw_rectangle_ex((int)(rect.x - hw), (int)(rect.y - hh), (int)rect.width, (int)rect.height, 
                                   hw + origin_x, hh + origin_y, rotation, tint);
        return;
    }
    
    float ox = rect.x + origin_x;
    float oy = rect.y + origin_y;
    float c = cosf(rotation);
    float s = sinf(rotation);
    
    float corners[4][2] = {
        { rect.x - hw, rect.y - hh },
        { rect.x + hw, rect.y - hh },
        { rect.x + hw, rect.y + hh },
        { rect.x - hw, rect.y + hh }
    };
    
    float rot[4][2];
    for (int i = 0; i < 4; i++) {
        float dx = corners[i][0] - ox;
        float dy = corners[i][1] - oy;
        rot[i][0] = ox + dx * c - dy * s;
        rot[i][1] = oy + dx * s + dy * c;
    }
    
    float ext = thickness / 2.0f;
    
    for (int i = 0; i < 4; i++) {
        int j = (i + 1) % 4;
        float dx = rot[j][0] - rot[i][0];
        float dy = rot[j][1] - rot[i][1];
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.001f) continue;
        float ux = dx / len;
        float uy = dy / len;
        
        float x0 = rot[i][0] - ux * ext;
        float y0 = rot[i][1] - uy * ext;
        float x1 = rot[j][0] + ux * ext;
        float y1 = rot[j][1] + uy * ext;
        
        renderer_draw_line_ex(x0, y0, x1, y1, thickness, tint);
    }
}

void renderer_draw_triangles(const float *data, size_t size, LUCI_Color tint) {
    if (!data || size < 6) return;
    if (size % 2 != 0) size = (size / 2) * 2;
    if (size % 6 != 0) size = (size / 6) * 6;
    if (size == 0) return;
    size_t count = size / 2;

    use_shader(activeShader);
    use_texture(white_texture);
    flush_batch();

    const int maxVerts = MAX_SPRITE * 4;
    size_t offsetVerts = 0;
    while (offsetVerts < count) {
        size_t chunkVerts = count - offsetVerts;
        if ((int)chunkVerts > maxVerts) chunkVerts = maxVerts - (maxVerts % 3);
        for (size_t i = 0; i < chunkVerts; i++) {
            float x = data[(offsetVerts + i) * 2 + 0];
            float y = data[(offsetVerts + i) * 2 + 1];
            float ndc_x = (x / (float)window_w) * 2.0f - 1.0f;
            float ndc_y = 1.0f - (y / (float)window_h) * 2.0f;
            vertex_buffer[i].x = ndc_x;
            vertex_buffer[i].y = ndc_y;
            vertex_buffer[i].z = 0.0f;
            vertex_buffer[i].u = 0.0f;
            vertex_buffer[i].v = 0.0f;
            vertex_buffer[i].color = tint;
        }
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, chunkVerts * sizeof(Vertex), vertex_buffer);
        glDrawArrays(GL_TRIANGLES, 0, chunkVerts);
        offsetVerts += chunkVerts;
    }
}

LUCI_Shader renderer_load_shader(const char* fs_src) {
    if (!fs_src) return (LUCI_Shader){0};
    const char* vs_src = VERTEX_SHADER_SOURCE;
    uint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_src, NULL);
    glCompileShader(vs);
    int ok;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glDeleteShader(vs); return (LUCI_Shader){0}; }
    uint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_src, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glDeleteShader(vs); glDeleteShader(fs); return (LUCI_Shader){0}; }
    uint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) { glDeleteProgram(prog); return (LUCI_Shader){0}; }
    return (LUCI_Shader){ prog };
}

void renderer_use_shader(LUCI_Shader shader) {
    if (shader.id == 0) {
        activeShader = defaultShaderProgram;
        use_shader(defaultShaderProgram);
        return;
    }
    activeShader = shader.id;
    use_shader(shader.id);
}

static void ensure_active_shader_bound(void) {
    if (activeShader == 0) return;
    if (currentShader != activeShader) {
        glUseProgram(activeShader);
        currentShader = activeShader;
    }
}

void renderer_shader_set_bool(const char* name, bool value) {
    if (!name || activeShader == 0) return;
    ensure_active_shader_bound();
    GLint loc = glGetUniformLocation(activeShader, name);
    if (loc != -1) glUniform1i(loc, value ? 1 : 0);
}
void renderer_shader_set_int(const char* name, int value) {
    if (!name || activeShader == 0) return;
    ensure_active_shader_bound();
    GLint loc = glGetUniformLocation(activeShader, name);
    if (loc != -1) glUniform1i(loc, value);
}
void renderer_shader_set_float(const char* name, float value) {
    if (!name || activeShader == 0) return;
    ensure_active_shader_bound();
    GLint loc = glGetUniformLocation(activeShader, name);
    if (loc != -1) glUniform1f(loc, value);
}
void renderer_shader_set_vec2(const char* name, float x, float y) {
    if (!name || activeShader == 0) return;
    ensure_active_shader_bound();
    GLint loc = glGetUniformLocation(activeShader, name);
    if (loc != -1) glUniform2f(loc, x, y);
}
void renderer_shader_set_vec3(const char* name, float x, float y, float z) {
    if (!name || activeShader == 0) return;
    ensure_active_shader_bound();
    GLint loc = glGetUniformLocation(activeShader, name);
    if (loc != -1) glUniform3f(loc, x, y, z);
}
void renderer_shader_set_vec4(const char* name, float x, float y, float z, float w) {
    if (!name || activeShader == 0) return;
    ensure_active_shader_bound();
    GLint loc = glGetUniformLocation(activeShader, name);
    if (loc != -1) glUniform4f(loc, x, y, z, w);
}
void renderer_shader_set_mat2(const char* name, const float* values) {
    if (!name || !values || activeShader == 0) return;
    ensure_active_shader_bound();
    GLint loc = glGetUniformLocation(activeShader, name);
    if (loc != -1) glUniformMatrix2fv(loc, 1, GL_FALSE, values);
}
void renderer_shader_set_mat3(const char* name, const float* values) {
    if (!name || !values || activeShader == 0) return;
    ensure_active_shader_bound();
    GLint loc = glGetUniformLocation(activeShader, name);
    if (loc != -1) glUniformMatrix3fv(loc, 1, GL_FALSE, values);
}
void renderer_shader_set_mat4(const char* name, const float* values) {
    if (!name || !values || activeShader == 0) return;
    ensure_active_shader_bound();
    GLint loc = glGetUniformLocation(activeShader, name);
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, values);
}

void renderer_use_font(LUCI_Font font) {
    current_font = font;
}

void renderer_use_default_font(void) {
    current_font = default_font;
}

void renderer_set_font_scale(float scale) {
    font_scale = scale;
}

float renderer_get_font_scale(void) {
    return font_scale;
}

void renderer_draw_text(const char* text, int x, int y, LUCI_Color tint) {
    use_shader(activeShader);
    LUCI_Font font = current_font;
    if (font.texture.id == 0 || font.glyphs == NULL || text == NULL) return;

    float pen_x = (float)x;
    float baseline = (float)y + (float)font.ascent * font_scale;

    for (const char* c = text; *c; c++) {
        if (quad_count >= MAX_SPRITE) flush_batch();

        if (*c == '\n') {
            pen_x = (float)x;
            baseline += (float)font.line_height * font_scale;
            continue;
        }

        unsigned char ch = (unsigned char)*c;
        if (ch < 32 || ch >= 32 + font.length) continue;

        LUCI_Glyph g = font.glyphs[ch - 32];
        if (g.w == 0 || g.h == 0) {
            pen_x += g.advance * font_scale;
            continue;
        }

        float x0 = roundf(pen_x + (float)g.bearing_x * font_scale);
        float y0 = roundf(baseline - (float)g.bearing_y * font_scale);

        use_texture(font.texture);
        glyph_push(x0, y0, (float)g.w * font_scale, (float)g.h * font_scale, g, font.texture.width, font.texture.height, tint);

        pen_x += g.advance * font_scale;
    }
}
