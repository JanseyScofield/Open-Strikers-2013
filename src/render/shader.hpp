#ifndef STRIKERS_RENDER_SHADER_HPP_
#define STRIKERS_RENDER_SHADER_HPP_

#include "physics/kinematic_body.hpp"
#include "render/camera.hpp"

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace strikers::render {

/**
 * Gerenciador RAII de programa de shader GLSL (Vertex + Fragment Shader).
 * Suporta projeção MVP, iluminação direcional difusa e amostragem de textura 2D.
 */
class Shader {
public:
    Shader() noexcept = default;
    ~Shader() noexcept;

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    /**
     * Compila e vincula o programa GLSL a partir de strings de código-fonte.
     */
    bool CompileFromSource(std::string_view vertex_src, std::string_view fragment_src);

    /**
     * Carrega, compila e vincula os shaders a partir de arquivos `.vert` e `.frag` no disco.
     */
    bool LoadFromFiles(
        const std::filesystem::path& vertex_path,
        const std::filesystem::path& fragment_path
    );

    /**
     * Inicializa o shader padrão de modelos 3D (tenta ler de `src/render/shaders/model.{vert,frag}`
     * com fallback embutido para robustez independente do diretório de execução).
     */
    bool LoadDefaultModelShader(
        const std::filesystem::path& vert_path = "src/render/shaders/model.vert",
        const std::filesystem::path& frag_path = "src/render/shaders/model.frag"
    );

    void Destroy() noexcept;

    void Use() const noexcept;
    static void Unbind() noexcept;

    [[nodiscard]] bool IsValid() const noexcept { return program_id_ != 0u; }
    [[nodiscard]] std::uint32_t GetProgramId() const noexcept { return program_id_; }

    void SetInt(const char* name, int value) const noexcept;
    void SetFloat(const char* name, float value) const noexcept;
    void SetVec3(const char* name, const physics::Vec3& v) const noexcept;
    void SetVec4(const char* name, float x, float y, float z, float w) const noexcept;
    void SetMat4(const char* name, const Mat4& mat) const noexcept;

private:
    static std::uint32_t CompileStage(std::uint32_t gl_shader_type, std::string_view source);

    std::uint32_t program_id_{0u};
};

} // namespace strikers::render

#endif // STRIKERS_RENDER_SHADER_HPP_
