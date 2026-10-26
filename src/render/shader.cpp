#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include "render/shader.hpp"

#include <GL/gl.h>
#include <GL/glext.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace strikers::render {

namespace {

constexpr std::string_view kFallbackVertexShader = R"GLSL(#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_MVP;
uniform mat4 u_Model;

out vec3 v_WorldNormal;
out vec2 v_TexCoord;

void main() {
    v_WorldNormal = mat3(u_Model) * a_Normal;
    v_TexCoord = a_TexCoord;
    gl_Position = u_MVP * vec4(a_Position, 1.0);
}
)GLSL";

constexpr std::string_view kFallbackFragmentShader = R"GLSL(#version 330 core
in vec3 v_WorldNormal;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform int u_HasTexture;
uniform vec3 u_LightDir;
uniform vec4 u_TintColor;
uniform float u_AmbientStrength;

out vec4 FragColor;

void main() {
    vec4 tex_color = (u_HasTexture != 0) ? texture(u_Texture, v_TexCoord) : vec4(1.0);
    vec4 base_color = tex_color * u_TintColor;

    if (base_color.a < 0.10) {
        discard;
    }

    vec3 n = (length(v_WorldNormal) > 1e-5) ? normalize(v_WorldNormal) : vec3(0.0, 1.0, 0.0);
    vec3 l = (length(u_LightDir) > 1e-5) ? normalize(u_LightDir) : vec3(0.35, 0.85, 0.40);

    float diff = max(dot(n, l), 0.0);
    float light_factor = clamp(u_AmbientStrength + (1.0 - u_AmbientStrength) * diff, 0.0, 1.15);

    FragColor = vec4(base_color.rgb * light_factor, base_color.a);
}
)GLSL";

bool ReadTextFile(const std::filesystem::path& path, std::string& out_text) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    out_text = ss.str();
    return !out_text.empty();
}

} // namespace

Shader::~Shader() noexcept {
    Destroy();
}

Shader::Shader(Shader&& other) noexcept
    : program_id_(std::exchange(other.program_id_, 0u)) {}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        Destroy();
        program_id_ = std::exchange(other.program_id_, 0u);
    }
    return *this;
}

void Shader::Destroy() noexcept {
    if (program_id_ != 0u) {
        glDeleteProgram(program_id_);
        program_id_ = 0u;
    }
}

std::uint32_t Shader::CompileStage(std::uint32_t gl_shader_type, std::string_view source) {
    const GLuint shader = glCreateShader(gl_shader_type);
    if (shader == 0u) {
        return 0u;
    }

    const char* src_ptr = source.data();
    const auto src_len = static_cast<GLint>(source.size());
    glShaderSource(shader, 1, &src_ptr, &src_len);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLint log_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
        std::vector<char> info_log(static_cast<std::size_t>(std::max(1, log_len)));
        glGetShaderInfoLog(shader, log_len, nullptr, info_log.data());
        std::cerr << "[Shader] Erro de compilacao GLSL ("
                  << (gl_shader_type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT")
                  << "): " << info_log.data() << "\n";
        glDeleteShader(shader);
        return 0u;
    }

    return shader;
}

bool Shader::CompileFromSource(std::string_view vertex_src, std::string_view fragment_src) {
    Destroy();

    const std::uint32_t vs = CompileStage(GL_VERTEX_SHADER, vertex_src);
    if (vs == 0u) {
        return false;
    }

    const std::uint32_t fs = CompileStage(GL_FRAGMENT_SHADER, fragment_src);
    if (fs == 0u) {
        glDeleteShader(vs);
        return false;
    }

    const GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);

    // Garante binding consistente dos atributos de vértice (0=Pos, 1=Normal, 2=UV)
    glBindAttribLocation(prog, 0, "a_Position");
    glBindAttribLocation(prog, 1, "a_Normal");
    glBindAttribLocation(prog, 2, "a_TexCoord");

    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint log_len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &log_len);
        std::vector<char> info_log(static_cast<std::size_t>(std::max(1, log_len)));
        glGetProgramInfoLog(prog, log_len, nullptr, info_log.data());
        std::cerr << "[Shader] Erro de linkagem do programa GLSL: " << info_log.data() << "\n";
        glDeleteProgram(prog);
        return false;
    }

    program_id_ = prog;
    return true;
}

bool Shader::LoadFromFiles(
    const std::filesystem::path& vertex_path,
    const std::filesystem::path& fragment_path
) {
    std::string vert_src;
    std::string frag_src;
    if (!ReadTextFile(vertex_path, vert_src) || !ReadTextFile(fragment_path, frag_src)) {
        return false;
    }
    return CompileFromSource(vert_src, frag_src);
}

bool Shader::LoadDefaultModelShader(
    const std::filesystem::path& vert_path,
    const std::filesystem::path& frag_path
) {
    if (LoadFromFiles(vert_path, frag_path)) {
        return true;
    }
    return CompileFromSource(kFallbackVertexShader, kFallbackFragmentShader);
}

void Shader::Use() const noexcept {
    glUseProgram(program_id_);
}

void Shader::Unbind() noexcept {
    glUseProgram(0);
}

void Shader::SetInt(const char* name, int value) const noexcept {
    if (program_id_ == 0u) return;
    const GLint loc = glGetUniformLocation(program_id_, name);
    if (loc >= 0) {
        glUniform1i(loc, value);
    }
}

void Shader::SetFloat(const char* name, float value) const noexcept {
    if (program_id_ == 0u) return;
    const GLint loc = glGetUniformLocation(program_id_, name);
    if (loc >= 0) {
        glUniform1f(loc, value);
    }
}

void Shader::SetVec3(const char* name, const physics::Vec3& v) const noexcept {
    if (program_id_ == 0u) return;
    const GLint loc = glGetUniformLocation(program_id_, name);
    if (loc >= 0) {
        glUniform3f(loc, v.x, v.y, v.z);
    }
}

void Shader::SetVec4(const char* name, float x, float y, float z, float w) const noexcept {
    if (program_id_ == 0u) return;
    const GLint loc = glGetUniformLocation(program_id_, name);
    if (loc >= 0) {
        glUniform4f(loc, x, y, z, w);
    }
}

void Shader::SetMat4(const char* name, const Mat4& mat) const noexcept {
    if (program_id_ == 0u) return;
    const GLint loc = glGetUniformLocation(program_id_, name);
    if (loc >= 0) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, mat.m.data());
    }
}

} // namespace strikers::render
