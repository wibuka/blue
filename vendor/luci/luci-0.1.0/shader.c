const char* VERTEX_SHADER_SOURCE = 
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec2 aTexCoord;\n"
    "layout (location = 2) in vec4 aColor;\n"
    "out vec2 TexCoord;\n"
    "out vec4 Color;\n"
    "void main() {\n"
    "   gl_Position = vec4(aPos, 1.0);\n"
    "   TexCoord = aTexCoord;\n"
    "   Color = aColor;\n"
    "}";

const char* FRAGMENT_SHADER_SOURCE = 
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in vec2 TexCoord;\n"
    "in vec4 Color;\n"
    "uniform sampler2D Texture;\n"
    "void main() {\n"
    "   FragColor = texture(Texture, TexCoord) * Color;\n"
    "}";

const char* CIRCLE_FRAGMENT_SHADER_SOURCE =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in vec2 TexCoord;\n"
    "in vec4 Color;\n"
    "uniform float inner_ratio;\n"
    "void main() {\n"
    "    vec2 p = TexCoord * 2.0 - 1.0;\n"
    "    float dist = length(p);\n"
    "    if (dist > 1.0) discard;\n"
    "    if (dist < inner_ratio) discard;\n"
    "    FragColor = Color;\n"
    "}";
