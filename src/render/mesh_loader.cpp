#include "render/mesh_loader.hpp"

#include <SDL2/SDL.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace strikers::render {

namespace {

struct MaterialDef {
    std::string name;
    std::filesystem::path diffuse_tex_path;
};

struct FaceIndexTriplet {
    int v_idx{-1};
    int vt_idx{-1};
    int vn_idx{-1};
};

std::string Trim(std::string_view sv) {
    const auto first = sv.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = sv.find_last_not_of(" \t\r\n");
    return std::string(sv.substr(first, last - first + 1));
}

FaceIndexTriplet ParseFaceToken(const std::string& token, int num_v, int num_vt, int num_vn) {
    FaceIndexTriplet out{};

    const auto s1 = token.find('/');
    const auto resolve_index = [](int raw_idx, int total) -> int {
        if (raw_idx > 0) return raw_idx - 1;
        if (raw_idx < 0) return total + raw_idx;
        return -1;
    };

    if (s1 == std::string::npos) {
        out.v_idx = resolve_index(std::atoi(token.c_str()), num_v);
        return out;
    }

    out.v_idx = resolve_index(std::atoi(token.substr(0, s1).c_str()), num_v);
    const auto s2 = token.find('/', s1 + 1);
    if (s2 == std::string::npos) {
        const std::string vt_str = token.substr(s1 + 1);
        if (!vt_str.empty()) {
            out.vt_idx = resolve_index(std::atoi(vt_str.c_str()), num_vt);
        }
        return out;
    }

    const std::string vt_str = token.substr(s1 + 1, s2 - (s1 + 1));
    if (!vt_str.empty()) {
        out.vt_idx = resolve_index(std::atoi(vt_str.c_str()), num_vt);
    }

    const std::string vn_str = token.substr(s2 + 1);
    if (!vn_str.empty()) {
        out.vn_idx = resolve_index(std::atoi(vn_str.c_str()), num_vn);
    }
    return out;
}

std::unordered_map<std::string, MaterialDef> ParseMtlFile(
    const std::filesystem::path& mtl_path
) {
    std::unordered_map<std::string, MaterialDef> materials;
    std::ifstream file(mtl_path);
    if (!file.is_open()) {
        return materials;
    }

    const std::filesystem::path base_dir = mtl_path.parent_path();
    std::string current_mtl;
    std::string line;
    while (std::getline(file, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        std::istringstream iss(trimmed);
        std::string keyword;
        iss >> keyword;
        if (keyword == "newmtl") {
            iss >> current_mtl;
            materials[current_mtl] = MaterialDef{current_mtl, {}};
        } else if (keyword == "map_Kd" && !current_mtl.empty()) {
            std::string rel_tex;
            std::getline(iss, rel_tex);
            rel_tex = Trim(rel_tex);
            if (!rel_tex.empty()) {
                materials[current_mtl].diffuse_tex_path = base_dir / rel_tex;
            }
        }
    }

    return materials;
}

} // namespace

Mesh MeshLoader::LoadObj(const std::filesystem::path& obj_path) {
    std::ifstream file(obj_path);
    if (!file.is_open()) {
        throw std::runtime_error("Nao foi possivel abrir o arquivo OBJ: " + obj_path.string());
    }

    const std::filesystem::path base_dir = obj_path.parent_path();

    std::vector<physics::Vec3> positions;
    std::vector<std::pair<float, float>> texcoords;
    std::vector<physics::Vec3> normals;

    positions.reserve(12000);
    texcoords.reserve(12000);
    normals.reserve(12000);

    std::unordered_map<std::string, MaterialDef> materials;
    std::unordered_map<std::string, std::shared_ptr<Texture>> texture_cache;

    struct SubMeshBuilder {
        std::string group_name{"default"};
        std::string material_name{};
        std::vector<Vertex> vertices{};
        std::vector<std::uint32_t> indices{};
    };

    std::vector<SubMeshBuilder> builders;
    SubMeshBuilder current_builder{};

    auto flush_current_builder = [&]() {
        if (!current_builder.vertices.empty() && !current_builder.indices.empty()) {
            builders.push_back(std::move(current_builder));
            current_builder = SubMeshBuilder{};
        }
    };

    std::string active_group = "default";
    std::string active_material;
    std::string line;

    while (std::getline(file, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        std::istringstream iss(trimmed);
        std::string prefix;
        iss >> prefix;

        if (prefix == "mtllib") {
            std::string mtl_rel;
            std::getline(iss, mtl_rel);
            mtl_rel = Trim(mtl_rel);
            if (!mtl_rel.empty()) {
                auto parsed_mtls = ParseMtlFile(base_dir / mtl_rel);
                for (auto& [k, v] : parsed_mtls) {
                    materials[k] = std::move(v);
                }
            }
        } else if (prefix == "g" || prefix == "o") {
            std::string grp_name;
            iss >> grp_name;
            if (!grp_name.empty() && grp_name != active_group) {
                flush_current_builder();
                active_group = grp_name;
                current_builder.group_name = active_group;
                current_builder.material_name = active_material;
            }
        } else if (prefix == "usemtl") {
            std::string mtl_name;
            iss >> mtl_name;
            if (mtl_name != active_material) {
                flush_current_builder();
                active_material = mtl_name;
                current_builder.group_name = active_group;
                current_builder.material_name = active_material;
            }
        } else if (prefix == "v") {
            float x = 0.0f, y = 0.0f, z = 0.0f;
            iss >> x >> y >> z;
            positions.push_back({x, y, z});
        } else if (prefix == "vt") {
            float u = 0.0f, v = 0.0f;
            iss >> u >> v;
            texcoords.emplace_back(u, v);
        } else if (prefix == "vn") {
            float nx = 0.0f, ny = 1.0f, nz = 0.0f;
            iss >> nx >> ny >> nz;
            normals.push_back({nx, ny, nz});
        } else if (prefix == "f") {
            std::vector<FaceIndexTriplet> face_verts;
            std::string token;
            while (iss >> token) {
                face_verts.push_back(ParseFaceToken(
                    token,
                    static_cast<int>(positions.size()),
                    static_cast<int>(texcoords.size()),
                    static_cast<int>(normals.size())
                ));
            }

            if (face_verts.size() < 3u) {
                continue;
            }

            const bool is_decal_layer =
                (active_material == "mark_00" || active_material == "numb_00" || active_material == "symb_00");

            auto emit_vertex = [&](const FaceIndexTriplet& ft) -> std::uint32_t {
                Vertex vtx{};
                if (ft.v_idx >= 0 && static_cast<std::size_t>(ft.v_idx) < positions.size()) {
                    vtx.position = positions[static_cast<std::size_t>(ft.v_idx)];
                }
                if (ft.vn_idx >= 0 && static_cast<std::size_t>(ft.vn_idx) < normals.size()) {
                    vtx.normal = normals[static_cast<std::size_t>(ft.vn_idx)];
                }
                if (ft.vt_idx >= 0 && static_cast<std::size_t>(ft.vt_idx) < texcoords.size()) {
                    vtx.u = texcoords[static_cast<std::size_t>(ft.vt_idx)].first;
                    vtx.v = texcoords[static_cast<std::size_t>(ft.vt_idx)].second;
                }
                if (is_decal_layer) {
                    // Elevação de 1.2mm ao longo da normal para evitar z-fighting em números e emblemas da camisa
                    vtx.position = vtx.position + (vtx.normal * 0.0012f);
                }

                const auto idx = static_cast<std::uint32_t>(current_builder.vertices.size());
                current_builder.vertices.push_back(vtx);
                return idx;
            };

            // Triangulação em leque (suporta triângulos e polígonos convexos)
            const std::uint32_t i0 = emit_vertex(face_verts[0]);
            std::uint32_t prev_idx = emit_vertex(face_verts[1]);
            for (std::size_t k = 2; k < face_verts.size(); ++k) {
                const std::uint32_t curr_idx = emit_vertex(face_verts[k]);
                current_builder.indices.push_back(i0);
                current_builder.indices.push_back(prev_idx);
                current_builder.indices.push_back(curr_idx);
                prev_idx = curr_idx;
            }
        }
    }

    flush_current_builder();

    if (builders.empty()) {
        throw std::runtime_error("Nenhuma geometria valida encontrada no arquivo OBJ: " + obj_path.string());
    }

    Mesh mesh;
    for (auto& b : builders) {
        std::shared_ptr<Texture> tex_ptr = nullptr;
        const auto mtl_it = materials.find(b.material_name);
        if (mtl_it != materials.end() && !mtl_it->second.diffuse_tex_path.empty()) {
            const std::string tex_key = mtl_it->second.diffuse_tex_path.string();
            auto cache_it = texture_cache.find(tex_key);
            if (cache_it != texture_cache.end()) {
                tex_ptr = cache_it->second;
            } else {
                auto new_tex = std::make_shared<Texture>();
                if (new_tex->LoadFromFile(mtl_it->second.diffuse_tex_path, true)) {
                    texture_cache[tex_key] = new_tex;
                    tex_ptr = std::move(new_tex);
                }
            }
        }

        SubMesh sm(
            std::move(b.group_name),
            std::move(b.material_name),
            std::move(b.vertices),
            std::move(b.indices),
            std::move(tex_ptr)
        );
        if (SDL_GL_GetCurrentContext() != nullptr) {
            sm.UploadToGpu();
        }
        mesh.AddSubMesh(std::move(sm));
    }

    return mesh;
}

} // namespace strikers::render
