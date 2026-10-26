#ifndef STRIKERS_RENDER_MESH_LOADER_HPP_
#define STRIKERS_RENDER_MESH_LOADER_HPP_

#include "render/mesh.hpp"

#include <filesystem>

namespace strikers::render {

/**
 * Carregador C++20 de arquivos Wavefront `.obj` e bibliotecas de materiais `.mtl`
 * gerados pelo pipeline de extração (`output/models/`).
 */
class MeshLoader {
public:
    /**
     * Analisa um arquivo `.obj` e seu `.mtl` vinculado, construindo os buffers
     * de vértices/índices (`VAO`/`VBO`/`EBO`) e carregando as texturas difusas (`map_Kd`).
     * Lança `std::runtime_error` se o arquivo `.obj` não puder ser aberto ou estiver vazio.
     */
    [[nodiscard]] static Mesh LoadObj(const std::filesystem::path& obj_path);
};

} // namespace strikers::render

#endif // STRIKERS_RENDER_MESH_LOADER_HPP_
