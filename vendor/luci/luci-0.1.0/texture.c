#define STB_IMAGE_IMPLEMENTATION

#include "luci.h"
#include "stb/stb_image.h"
#include "glad/glad.h"

#define uint unsigned int

LUCI_Texture texture_load(const char* path)
{
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(path, &width, &height, &channels, 0);

    if (!data) {
        fprintf(stderr, "failed to load texture '%s': %s\n", path, stbi_failure_reason());
        return (LUCI_Texture){0};
    }

    if (channels < 1 || channels > 4) {
        fprintf(stderr, "unsupported texture format (%d channels): '%s'\n", channels, path);
        stbi_image_free(data);
        return (LUCI_Texture){0};
    }

    const GLenum formats[] = { 0, GL_RED, GL_RG, GL_RGB, GL_RGBA };
    GLenum format = formats[channels];

    LUCI_Texture texture = {0};
    glGenTextures(1, &texture.id);

    if (texture.id == 0) {
        fprintf(stderr, "failed to create OpenGL texture: '%s'\n", path);
        stbi_image_free(data);
        return (LUCI_Texture){0};
    }

    glBindTexture(GL_TEXTURE_2D, texture.id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);

    texture.width  = width;
    texture.height = height;
    texture.format = channels;

    return texture;
}

LUCI_Texture texture_create(const unsigned char* data, int width, int height, int channels)
{
    if (channels < 1 || channels > 4) {
        fprintf(stderr, "unsupported texture format (%d channels)\n", channels);
        return (LUCI_Texture){0};
    }

    const GLenum formats[] = { 0, GL_RED, GL_RG, GL_RGB, GL_RGBA };
    GLenum format = formats[channels];

    LUCI_Texture texture = {0};
    glGenTextures(1, &texture.id);

    if (texture.id == 0) {
        fprintf(stderr, "failed to create OpenGL texture from data\n");
        return (LUCI_Texture){0};
    }

    glBindTexture(GL_TEXTURE_2D, texture.id);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    texture.width  = width;
    texture.height = height;
    texture.format = channels;

    return texture;
}
