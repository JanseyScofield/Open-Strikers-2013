#ifndef STRIKERS_RENDER_MESH_HPP_
#define STRIKERS_RENDER_MESH_HPP_

#include "physics/kinematic_body.hpp"
#include "render/camera.hpp"
#include "render/shader.hpp"
#include "render/texture.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace strikers::render {

/**
 * Vértice 3D intercalado (Posição XYZ, Normal XYZ e Coordenada UV).
 */
struct Vertex {
    physics::Vec3 position{0.0f, 0.0f, 0.0f};
    physics::Vec3 normal{0.0f, 1.0f, 0.0f};
    float u{0.0f};
    float v{0.0f};
};

/**
 * Sub-malha associada a um material e textura difusa, gerenciando
 * seu próprio Vertex Array Object (VAO), Vertex Buffer Object (VBO)
 * e Element Buffer Object (EBO) no OpenGL.
 */
class SubMesh {
public:
    SubMesh() noexcept = default;
    SubMesh(
        std::string group_name,
        std::string material_name,
        std::vector<Vertex> vertices,
        std::vector<std::uint32_t> indices,
        std::shared_ptr<Texture> texture = nullptr
    );
    ~SubMesh() noexcept;

    SubMesh(const SubMesh&) = delete;
    SubMesh& operator=(const SubMesh&) = delete;
    SubMesh(SubMesh&& other) noexcept;
    SubMesh& operator=(SubMesh&& other) noexcept;

    /**
     * Cria e popula VAO, VBO e EBO na GPU caso haja um contexto OpenGL ativo.
     */
    bool UploadToGpu();

    void DestroyGpuBuffers() noexcept;

    void Draw(const Shader& shader) const noexcept;

    [[nodiscard]] const std::string& GetGroupName() const noexcept { return group_name_; }
    [[nodiscard]] const std::string& GetMaterialName() const noexcept { return material_name_; }
    [[nodiscard]] const std::vector<Vertex>& GetVertices() const noexcept { return vertices_; }
    [[nodiscard]] const std::vector<std::uint32_t>& GetIndices() const noexcept { return indices_; }
    [[nodiscard]] std::shared_ptr<Texture> GetTexture() const noexcept { return texture_; }
    [[nodiscard]] bool IsUploadedToGpu() const noexcept { return vao_ != 0u; }

private:
    std::string group_name_{};
    std::string material_name_{};
    std::vector<Vertex> vertices_{};
    std::vector<std::uint32_t> indices_{};
    std::shared_ptr<Texture> texture_{nullptr};

    std::uint32_t vao_{0u};
    std::uint32_t vbo_{0u};
    std::uint32_t ebo_{0u};
};

/**
 * Modelo 3D composto por uma ou mais sub-malhas (`SubMesh`) e suas texturas (`Texture`).
 */
class Mesh {
public:
    Mesh() noexcept = default;
    ~Mesh() noexcept = default;

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) noexcept = default;
    Mesh& operator=(Mesh&&) noexcept = default;

    void AddSubMesh(SubMesh submesh);
    bool UploadToGpu();

    /**
     * Renderiza todas as sub-malhas do modelo aplicando matrizes Model e MVP,
     * iluminação direcional difusa e amostragem de textura 2D.
     */
    void Draw(
        const Shader& shader,
        const Mat4& model_matrix,
        const Mat4& view_proj_matrix,
        const physics::Vec3& light_dir = {0.35f, 0.85f, 0.40f},
        float tint_r = 1.0f,
        float tint_g = 1.0f,
        float tint_b = 1.0f,
        float tint_a = 1.0f
    ) const noexcept;

    [[nodiscard]] bool IsEmpty() const noexcept { return submeshes_.empty(); }
    [[nodiscard]] std::size_t GetSubMeshCount() const noexcept { return submeshes_.size(); }
    [[nodiscard]] std::size_t GetTotalVertexCount() const noexcept;
    [[nodiscard]] std::size_t GetTotalIndexCount() const noexcept;
    [[nodiscard]] std::size_t GetTotalTriangleCount() const noexcept { return GetTotalIndexCount() / 3u; }
    [[nodiscard]] const std::vector<SubMesh>& GetSubMeshes() const noexcept { return submeshes_; }

private:
    std::vector<SubMesh> submeshes_{};
};

} // namespace strikers::render

#endif // STRIKERS_RENDER_MESH_HPP_
