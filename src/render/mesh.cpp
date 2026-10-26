#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include "render/mesh.hpp"

#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL2/SDL.h>
#include <cstddef>
#include <utility>

namespace strikers::render {

SubMesh::SubMesh(
    std::string group_name,
    std::string material_name,
    std::vector<Vertex> vertices,
    std::vector<std::uint32_t> indices,
    std::shared_ptr<Texture> texture
)
    : group_name_(std::move(group_name)),
      material_name_(std::move(material_name)),
      vertices_(std::move(vertices)),
      indices_(std::move(indices)),
      texture_(std::move(texture)) {}

SubMesh::~SubMesh() noexcept {
    DestroyGpuBuffers();
}

SubMesh::SubMesh(SubMesh&& other) noexcept
    : group_name_(std::move(other.group_name_)),
      material_name_(std::move(other.material_name_)),
      vertices_(std::move(other.vertices_)),
      indices_(std::move(other.indices_)),
      texture_(std::move(other.texture_)),
      vao_(std::exchange(other.vao_, 0u)),
      vbo_(std::exchange(other.vbo_, 0u)),
      ebo_(std::exchange(other.ebo_, 0u)) {}

SubMesh& SubMesh::operator=(SubMesh&& other) noexcept {
    if (this != &other) {
        DestroyGpuBuffers();
        group_name_ = std::move(other.group_name_);
        material_name_ = std::move(other.material_name_);
        vertices_ = std::move(other.vertices_);
        indices_ = std::move(other.indices_);
        texture_ = std::move(other.texture_);
        vao_ = std::exchange(other.vao_, 0u);
        vbo_ = std::exchange(other.vbo_, 0u);
        ebo_ = std::exchange(other.ebo_, 0u);
    }
    return *this;
}

void SubMesh::DestroyGpuBuffers() noexcept {
    if (SDL_GL_GetCurrentContext() != nullptr) {
        if (ebo_ != 0u) {
            const GLuint id = ebo_;
            glDeleteBuffers(1, &id);
        }
        if (vbo_ != 0u) {
            const GLuint id = vbo_;
            glDeleteBuffers(1, &id);
        }
        if (vao_ != 0u) {
            const GLuint id = vao_;
            glDeleteVertexArrays(1, &id);
        }
    }
    vao_ = 0u;
    vbo_ = 0u;
    ebo_ = 0u;
}

bool SubMesh::UploadToGpu() {
    if (vao_ != 0u) {
        return true;
    }
    if (vertices_.empty() || indices_.empty() || SDL_GL_GetCurrentContext() == nullptr) {
        return false;
    }

    if (texture_ && !texture_->HasGpuTexture()) {
        texture_->UploadToGpu();
    }

    GLuint vao = 0u;
    GLuint vbo = 0u;
    GLuint ebo = 0u;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    if (vao == 0u || vbo == 0u || ebo == 0u) {
        if (ebo != 0u) glDeleteBuffers(1, &ebo);
        if (vbo != 0u) glDeleteBuffers(1, &vbo);
        if (vao != 0u) glDeleteVertexArrays(1, &vao);
        return false;
    }

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)),
        vertices_.data(),
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices_.size() * sizeof(std::uint32_t)),
        indices_.data(),
        GL_STATIC_DRAW
    );

    // Atributo 0: Posição (X, Y, Z)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<const void*>(offsetof(Vertex, position))
    );

    // Atributo 1: Normal (NX, NY, NZ)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<const void*>(offsetof(Vertex, normal))
    );

    // Atributo 2: Coordenada UV (U, V)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<const void*>(offsetof(Vertex, u))
    );

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    vao_ = vao;
    vbo_ = vbo;
    ebo_ = ebo;
    return true;
}

void SubMesh::Draw(const Shader& shader) const noexcept {
    if (vao_ == 0u || indices_.empty()) {
        return;
    }

    if (texture_ && texture_->HasGpuTexture()) {
        texture_->Bind(0u);
        shader.SetInt("u_Texture", 0);
        shader.SetInt("u_HasTexture", 1);
    } else {
        Texture::Unbind();
        shader.SetInt("u_HasTexture", 0);
    }

    glBindVertexArray(vao_);
    glDrawElements(
        GL_TRIANGLES,
        static_cast<GLsizei>(indices_.size()),
        GL_UNSIGNED_INT,
        nullptr
    );
    glBindVertexArray(0);
}

void Mesh::AddSubMesh(SubMesh submesh) {
    submeshes_.push_back(std::move(submesh));
}

bool Mesh::UploadToGpu() {
    bool all_ok = !submeshes_.empty();
    for (auto& sm : submeshes_) {
        if (!sm.UploadToGpu()) {
            all_ok = false;
        }
    }
    return all_ok;
}

void Mesh::Draw(
    const Shader& shader,
    const Mat4& model_matrix,
    const Mat4& view_proj_matrix,
    const physics::Vec3& light_dir,
    float tint_r,
    float tint_g,
    float tint_b,
    float tint_a
) const noexcept {
    if (!shader.IsValid() || submeshes_.empty()) {
        return;
    }

    const Mat4 mvp = view_proj_matrix * model_matrix;

    shader.Use();
    shader.SetMat4("u_MVP", mvp);
    shader.SetMat4("u_Model", model_matrix);
    shader.SetVec3("u_LightDir", light_dir);
    shader.SetVec4("u_TintColor", tint_r, tint_g, tint_b, tint_a);
    shader.SetFloat("u_AmbientStrength", 0.58f);

    for (const auto& sm : submeshes_) {
        sm.Draw(shader);
    }

    Texture::Unbind();
    Shader::Unbind();
}

std::size_t Mesh::GetTotalVertexCount() const noexcept {
    std::size_t total = 0u;
    for (const auto& sm : submeshes_) {
        total += sm.GetVertices().size();
    }
    return total;
}

std::size_t Mesh::GetTotalIndexCount() const noexcept {
    std::size_t total = 0u;
    for (const auto& sm : submeshes_) {
        total += sm.GetIndices().size();
    }
    return total;
}

} // namespace strikers::render
