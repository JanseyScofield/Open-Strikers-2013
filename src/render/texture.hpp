#ifndef STRIKERS_RENDER_TEXTURE_HPP_
#define STRIKERS_RENDER_TEXTURE_HPP_

#include <cstdint>
#include <filesystem>
#include <vector>

namespace strikers::render {

/**
 * Textura 2D OpenGL carregada a partir de imagens PNG (via stb_image)
 * com suporte a filtragem bilinear/trilinear (mipmaps) e repetição UV (`GL_REPEAT`).
 */
class Texture {
public:
    Texture() noexcept = default;
    ~Texture() noexcept;

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    /**
     * Carrega um arquivo PNG do disco (RGBA8) e, se houver contexto OpenGL ativo,
     * cria a textura 2D na GPU com mipmaps trilineares.
     */
    bool LoadFromFile(const std::filesystem::path& png_path, bool flip_vertically = true);

    /**
     * Faz o upload dos pixels RGBA8 em memória para a GPU caso ainda não tenha sido enviado.
     */
    bool UploadToGpu();

    void Destroy() noexcept;

    void Bind(std::uint32_t texture_unit = 0u) const noexcept;
    static void Unbind() noexcept;

    [[nodiscard]] bool IsLoaded() const noexcept { return width_ > 0 && height_ > 0; }
    [[nodiscard]] bool HasGpuTexture() const noexcept { return texture_id_ != 0u; }
    [[nodiscard]] std::uint32_t GetTextureId() const noexcept { return texture_id_; }
    [[nodiscard]] int GetWidth() const noexcept { return width_; }
    [[nodiscard]] int GetHeight() const noexcept { return height_; }
    [[nodiscard]] int GetChannels() const noexcept { return channels_; }
    [[nodiscard]] const std::filesystem::path& GetFilePath() const noexcept { return file_path_; }

private:
    std::filesystem::path file_path_{};
    std::uint32_t texture_id_{0u};
    int width_{0};
    int height_{0};
    int channels_{0};
    std::vector<std::uint8_t> pixels_rgba_{};
};

} // namespace strikers::render

#endif // STRIKERS_RENDER_TEXTURE_HPP_
