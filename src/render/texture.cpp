#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "render/texture.hpp"

#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL2/SDL.h>
#include <iostream>
#include <utility>

namespace strikers::render {

Texture::~Texture() noexcept {
    Destroy();
}

Texture::Texture(Texture&& other) noexcept
    : file_path_(std::move(other.file_path_)),
      texture_id_(std::exchange(other.texture_id_, 0u)),
      width_(std::exchange(other.width_, 0)),
      height_(std::exchange(other.height_, 0)),
      channels_(std::exchange(other.channels_, 0)),
      pixels_rgba_(std::move(other.pixels_rgba_)) {}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        Destroy();
        file_path_ = std::move(other.file_path_);
        texture_id_ = std::exchange(other.texture_id_, 0u);
        width_ = std::exchange(other.width_, 0);
        height_ = std::exchange(other.height_, 0);
        channels_ = std::exchange(other.channels_, 0);
        pixels_rgba_ = std::move(other.pixels_rgba_);
    }
    return *this;
}

void Texture::Destroy() noexcept {
    if (texture_id_ != 0u && SDL_GL_GetCurrentContext() != nullptr) {
        const GLuint id = texture_id_;
        glDeleteTextures(1, &id);
    }
    texture_id_ = 0u;
    width_ = 0;
    height_ = 0;
    channels_ = 0;
    pixels_rgba_.clear();
}

bool Texture::LoadFromFile(const std::filesystem::path& png_path, bool flip_vertically) {
    Destroy();
    file_path_ = png_path;

    stbi_set_flip_vertically_on_load(flip_vertically ? 1 : 0);
    int w = 0;
    int h = 0;
    int ch = 0;
    stbi_uc* data = stbi_load(png_path.string().c_str(), &w, &h, &ch, 4);
    if (data == nullptr || w <= 0 || h <= 0) {
        std::cerr << "[Texture] Falha ao carregar PNG '" << png_path.string()
                  << "': " << stbi_failure_reason() << "\n";
        if (data != nullptr) {
            stbi_image_free(data);
        }
        return false;
    }

    width_ = w;
    height_ = h;
    channels_ = ch;
    const std::size_t byte_count = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u;
    pixels_rgba_.assign(data, data + byte_count);
    stbi_image_free(data);

    if (SDL_GL_GetCurrentContext() != nullptr) {
        return UploadToGpu();
    }
    return true;
}

bool Texture::UploadToGpu() {
    if (texture_id_ != 0u) {
        return true;
    }
    if (pixels_rgba_.empty() || width_ <= 0 || height_ <= 0 || SDL_GL_GetCurrentContext() == nullptr) {
        return false;
    }

    GLuint tex = 0u;
    glGenTextures(1, &tex);
    if (tex == 0u) {
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Envelopamento REPEAT para coordenadas UV fora de [0, 1] (nativo do Wii GX em MDL0)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Filtragem trilinear na minificação e bilinear na magnificação
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width_,
        height_,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels_rgba_.data()
    );
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    texture_id_ = tex;
    return true;
}

void Texture::Bind(std::uint32_t texture_unit) const noexcept {
    glActiveTexture(GL_TEXTURE0 + texture_unit);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
}

void Texture::Unbind() noexcept {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace strikers::render
