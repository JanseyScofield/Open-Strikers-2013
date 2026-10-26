#!/usr/bin/env python3
"""
Extrator de Modelos 3D (NW4R MDL0 v11) e Texturas Wii GX (TEX0 v3)
de Inazuma Eleven Strikers 2013 (grp.bin + dat.bin idx=75).

Decodifica contêineres comprimidos com Shade LZSS (0xFCAA55A7) em `grp.bin`,
lê estruturas BRRES (`bres`), malhas 3D (`MDL0` v11) em Big-Endian (`>`)
incluindo árvores de ossos (`ResBoneData`), matrizes de skinning (`NodeMix`),
bytecode de renderização (`DrawOpa`), arrays de vértices/normais/UVs e Display
Lists do hardware Wii GX (`ResShpData`), além de texturas em blocos (`TEX0` v3:
CMPR/DXT1, RGB565, RGB5A3, IA4, I8, RGBA8).

Exporta malhas `.obj` + `.mtl` em `output/models/` e texturas `.png` em
`output/models/textures/`.
"""

from __future__ import annotations

import json
import math
import struct
import zlib
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from parse_character_table import StrikersDatParser, shade_decompress

# =============================================================================
# Constantes de Hardware Nintendo Wii GX & NW4R BRRES
# =============================================================================

SHADE_LZSS_MAGIC = b"\xfc\xaa\x55\xa7"
BRRES_MAGIC = b"bres"
MDL0_MAGIC = b"MDL0"
TEX0_MAGIC = b"TEX0"

# GXCompType: (struct_char, byte_size)
GX_COMP_TYPES: Dict[int, Tuple[str, int]] = {
    0: ("B", 1),  # GX_U8
    1: ("b", 1),  # GX_S8
    2: ("H", 2),  # GX_U16
    3: ("h", 2),  # GX_S16
    4: ("f", 4),  # GX_F32
}

# GXTexFmt: Nomes legíveis dos formatos de textura nativos do Wii
GX_TEX_FORMAT_NAMES: Dict[int, str] = {
    0x0: "I4",
    0x1: "I8",
    0x2: "IA4",
    0x3: "IA8",
    0x4: "RGB565",
    0x5: "RGB5A3",
    0x6: "RGBA8",
    0x8: "CI4",
    0x9: "CI8",
    0xA: "CI14X2",
    0xE: "CMPR",
}

Matrix3x4 = Tuple[
    Tuple[float, float, float, float],
    Tuple[float, float, float, float],
    Tuple[float, float, float, float],
]

IDENTITY_MTX3X4: Matrix3x4 = (
    (1.0, 0.0, 0.0, 0.0),
    (0.0, 1.0, 0.0, 0.0),
    (0.0, 0.0, 1.0, 0.0),
)


# =============================================================================
# Operações Matriciais 3x4 (Transformação de Vértices e Normais)
# =============================================================================


def transform_point(mtx: Matrix3x4, pt: Tuple[float, float, float]) -> Tuple[float, float, float]:
    """Aplica matriz afim 3x4 (Big-Endian NW4R) a um ponto 3D (x, y, z, 1.0)."""
    x, y, z = pt
    rx = mtx[0][0] * x + mtx[0][1] * y + mtx[0][2] * z + mtx[0][3]
    ry = mtx[1][0] * x + mtx[1][1] * y + mtx[1][2] * z + mtx[1][3]
    rz = mtx[2][0] * x + mtx[2][1] * y + mtx[2][2] * z + mtx[2][3]
    return (rx, ry, rz)


def transform_normal(mtx: Matrix3x4, nrm: Tuple[float, float, float]) -> Tuple[float, float, float]:
    """Aplica a submatriz 3x3 de rotação/escala a um vetor normal e normaliza."""
    nx, ny, nz = nrm
    rx = mtx[0][0] * nx + mtx[0][1] * ny + mtx[0][2] * nz
    ry = mtx[1][0] * nx + mtx[1][1] * ny + mtx[1][2] * nz
    rz = mtx[2][0] * nx + mtx[2][1] * ny + mtx[2][2] * nz
    length = math.sqrt(rx * rx + ry * ry + rz * rz)
    if length > 1e-8:
        return (rx / length, ry / length, rz / length)
    return (nx, ny, nz)


# =============================================================================
# Estruturas Declarativas de Tabelas (dat.bin idx=75 e grp.bin)
# =============================================================================


@dataclass
class ModelDefEntry:
    """
    Registro de definição de modelo em `dat.bin` (`idx=75`, seção 0, stride=0x5C).
    Carregado por `onm_load__Fv` (`0x80029C24`) e referenciado por `MDLRegist__Fi`.
    """

    model_def_id: int
    mdl_file_id: int        # +0x00 (>I): ID 1-based em grp.bin (grp_index = mdl_file_id - 1)
    tex_set_index: int      # +0x04 (>I): Índice na tabela de conjuntos de texturas (sec[1])
    bound_radius: float     # +0x14 (>f): Raio de colisão/culling
    bound_height: float     # +0x18 (>f): Altura de colisão/culling
    scale: float            # +0x28 (>f): Escala base do modelo

    @classmethod
    def from_bytes(cls, buf: bytes, offset: int, model_def_id: int) -> ModelDefEntry:
        mdl_file_id, tex_set_index = struct.unpack_from(">II", buf, offset + 0x00)
        radius, height = struct.unpack_from(">ff", buf, offset + 0x14)
        scale = struct.unpack_from(">f", buf, offset + 0x28)[0]
        return cls(
            model_def_id=model_def_id,
            mdl_file_id=mdl_file_id,
            tex_set_index=tex_set_index,
            bound_radius=round(radius, 4),
            bound_height=round(height, 4),
            scale=round(scale, 4),
        )


@dataclass
class TexSetDefEntry:
    """
    Registro de conjunto de texturas em `dat.bin` (`idx=75`, seção 1, stride=0x28).
    """

    tex_set_id: int
    tex_file_ids: List[int]  # +0x10..+0x27 (>6I): IDs 1-based em grp.bin (não-zero)

    @classmethod
    def from_bytes(cls, buf: bytes, offset: int, tex_set_id: int) -> TexSetDefEntry:
        vals = struct.unpack_from(">10I", buf, offset)
        file_ids = [v for v in vals[4:10] if v != 0]
        return cls(tex_set_id=tex_set_id, tex_file_ids=file_ids)


class GrpContainer:
    """
    Leitor do contêiner gráfico `grp.bin` da Level-5 / Shade.
    Cabeçalho (0x00..0x13, Little-Endian `<5I`):
      +0x00: entry_count  (4973)
      +0x04: offset_align (0x4000)
      +0x08: size_mult    (0x11)
      +0x0C: bit_shift    (17)
      +0x10: bit_mask     (0x1FFFF)
    """

    def __init__(self, path: Path) -> None:
        self.raw = path.read_bytes()
        (
            self.entry_count,
            self.offset_align,
            self.size_mult,
            self.bit_shift,
            self.bit_mask,
        ) = struct.unpack_from("<5I", self.raw, 0x00)
        self.descriptors = struct.unpack_from(f"<{self.entry_count}I", self.raw, 0x14)

    def get_by_file_id(self, file_id_1based: int) -> bytes:
        """Lê e descomprime um arquivo de `grp.bin` usando o ID 1-based do jogo."""
        if file_id_1based <= 0 or file_id_1based > self.entry_count:
            raise ValueError(f"file_id fora do intervalo: {file_id_1based}")
        return self.get_by_index(file_id_1based - 1)

    def get_by_index(self, index: int) -> bytes:
        """Lê e descomprime a entrada `index` (0-based) de `grp.bin`."""
        desc = self.descriptors[index]
        offset = (desc >> self.bit_shift) * self.offset_align
        magic = self.raw[offset : offset + 4]
        if magic == SHADE_LZSS_MAGIC:
            dec_size, cmp_size = struct.unpack_from("<II", self.raw, offset + 4)
            decompressed = shade_decompress(self.raw[offset : offset + cmp_size])
            if len(decompressed) != dec_size:
                raise ValueError(
                    f"grp[{index}] falha de integridade LZSS: esperado={dec_size}, obtido={len(decompressed)}"
                )
            return decompressed
        raise ValueError(f"grp[{index}] cabeçalho desconhecido em {offset:#x}: {magic!r}")


# =============================================================================
# Estruturas Declarativas NW4R BRRES (MDL0 v11 e TEX0 v3)
# =============================================================================


def parse_res_dic(buf: bytes, dic_offset: int) -> List[Tuple[str, int]]:
    """
    Decodifica um dicionário `ResDic` da biblioteca NintendoWare for Revolution (NW4R).
    Cabeçalho:
      +0x00 (>I): byte_size
      +0x04 (>I): num_entries (excluindo nó raiz)
      +0x08..+0x17: nó raiz (16 bytes)
      +0x18..: `num_entries` nós de 16 bytes (`>HHHBx ii`: id, flags, prev, next, name_off, data_off)
    Os offsets `name_off` e `data_off` são relativos a `dic_offset`.
    """
    _, num_entries = struct.unpack_from(">II", buf, dic_offset)
    items: List[Tuple[str, int]] = []
    for i in range(num_entries):
        entry_off = dic_offset + 0x18 + i * 0x10
        name_rel, data_rel = struct.unpack_from(">ii", buf, entry_off + 0x08)
        name_pos = dic_offset + name_rel
        name_end = buf.find(b"\x00", name_pos)
        name = buf[name_pos:name_end].decode("ascii", errors="replace")
        items.append((name, dic_offset + data_rel))
    return items


@dataclass
class ResBoneData:
    """
    Estrutura de osso `ResBoneData` (`MDL0` seção 1, `0xD0` bytes em Big-Endian).
      +0x0C (>I): node_id
      +0x10 (>I): mtx_id
      +0x20 (>3f): scale (sx, sy, sz)
      +0x2C (>3f): rotation_deg (rx, ry, rz)
      +0x38 (>3f): translation (tx, ty, tz)
      +0x70 (>12f): world_matrix (3x4 bind pose model-space matrix)
      +0xA0 (>12f): inv_bind_matrix (3x4 inverse bind matrix)
    """

    name: str
    node_id: int
    mtx_id: int
    flags: int
    scale: Tuple[float, float, float]
    rotation: Tuple[float, float, float]
    translation: Tuple[float, float, float]
    world_matrix: Matrix3x4
    inv_bind_matrix: Matrix3x4

    @classmethod
    def from_bytes(cls, name: str, buf: bytes, offset: int) -> ResBoneData:
        node_id, mtx_id, flags = struct.unpack_from(">III", buf, offset + 0x0C)
        scale = struct.unpack_from(">3f", buf, offset + 0x20)
        rot = struct.unpack_from(">3f", buf, offset + 0x2C)
        trans = struct.unpack_from(">3f", buf, offset + 0x38)
        wm_flat = struct.unpack_from(">12f", buf, offset + 0x70)
        ibm_flat = struct.unpack_from(">12f", buf, offset + 0xA0)
        world_mtx: Matrix3x4 = (wm_flat[0:4], wm_flat[4:8], wm_flat[8:12])
        inv_bind_mtx: Matrix3x4 = (ibm_flat[0:4], ibm_flat[4:8], ibm_flat[8:12])
        return cls(
            name=name,
            node_id=node_id,
            mtx_id=mtx_id,
            flags=flags,
            scale=scale,
            rotation=rot,
            translation=trans,
            world_matrix=world_mtx,
            inv_bind_matrix=inv_bind_mtx,
        )


@dataclass
class ResVtxArrayData:
    """
    Cabeçalho e elementos decodificados de `ResVtxPosData` (Sec 2),
    `ResVtxNrmData` (Sec 3) ou `ResVtxTexCoordData` (Sec 5).
      +0x00 (>I): byte_length
      +0x04 (>i): mdl0_offset
      +0x08 (>i): data_offset (relativo ao início desta estrutura)
      +0x0C (>i): name_offset
      +0x10 (>I): array_index
      +0x14 (>I): comp_cnt (Pos: 0=XY, 1=XYZ; Nrm: 0=XYZ, 1=NBT; UV: 0=S, 1=ST)
      +0x18 (>I): comp_type (0=U8, 1=S8, 2=U16, 3=S16, 4=F32)
      +0x1C (>B): divisor (fator de ponto fixo = 1 / 2^divisor)
      +0x1D (>B): stride (bytes por elemento)
      +0x1E (>H): num_elements
    """

    name: str
    index: int
    comp_cnt: int
    comp_type: int
    divisor: int
    stride: int
    num_elements: int
    values: List[Tuple[float, ...]]

    @classmethod
    def from_bytes(cls, name: str, buf: bytes, offset: int, n_components: int) -> ResVtxArrayData:
        (
            _,
            _,
            data_off,
            _,
            idx,
            comp_cnt,
            comp_type,
            divisor,
            stride,
            num_elems,
        ) = struct.unpack_from(">IiiiIIIBBH", buf, offset)
        fmt_char, type_size = GX_COMP_TYPES[comp_type]
        scale = 1.0 if comp_type == 4 else (1.0 / float(1 << divisor))
        elem_fmt = f">{n_components}{fmt_char}"
        data_start = offset + data_off
        values: List[Tuple[float, ...]] = []
        for i in range(num_elems):
            pos = data_start + i * stride
            raw_tuple = struct.unpack_from(elem_fmt, buf, pos)
            values.append(tuple(v * scale for v in raw_tuple))
        return cls(
            name=name,
            index=idx,
            comp_cnt=comp_cnt,
            comp_type=comp_type,
            divisor=divisor,
            stride=stride,
            num_elements=num_elems,
            values=values,
        )


@dataclass
class ResMatData:
    """
    Material `ResMatData` (`MDL0` seção 8).
      +0x0C (>I): index
      +0x18 (>I): cull_mode
      +0x2C (>I): num_tex_layers
      +0x30 (>i): tex_layer_offset (array de `ResTexPlttInfoOffset` de 0x34 bytes)
    """

    name: str
    index: int
    cull_mode: int
    texture_names: List[str]

    @classmethod
    def from_bytes(cls, name: str, buf: bytes, offset: int) -> ResMatData:
        idx = struct.unpack_from(">I", buf, offset + 0x0C)[0]
        cull_mode = struct.unpack_from(">I", buf, offset + 0x18)[0]
        num_layers, layer_off = struct.unpack_from(">Ii", buf, offset + 0x2C)
        tex_names: List[str] = []
        for i in range(num_layers):
            lpos = offset + layer_off + i * 0x34
            tname_rel = struct.unpack_from(">i", buf, lpos)[0]
            if tname_rel != 0:
                tname_pos = lpos + tname_rel
                tname_end = buf.find(b"\x00", tname_pos)
                tex_names.append(buf[tname_pos:tname_end].decode("ascii", errors="replace"))
        return cls(name=name, index=idx, cull_mode=cull_mode, texture_names=tex_names)

    @property
    def primary_diffuse_texture(self) -> Optional[str]:
        """Retorna a textura difusa principal (ignorando mapas auxiliares `_falloff`/`shading`)."""
        for tname in self.texture_names:
            if tname.lower() not in ("_falloff", "falloff", "shading"):
                return tname
        return self.texture_names[0] if self.texture_names else None


@dataclass
class ResShpData:
    """
    Polígono/Malha `ResShpData` (`MDL0` seção 10).
      +0x08 (>i): mtx_id (-1 se usa PMIDX por vértice, >=0 se vinculado a matriz fixa)
      +0x0C (>I): cp_vcd_lo (registrador GX CP_VCD_LO: PMIDX, T0..7MIDX, POS, NRM, COL0..1)
      +0x10 (>I): cp_vcd_hi (registrador GX CP_VCD_HI: TEX0..7)
      +0x18 (>IIi): dl_setup (buf_size, cmd_size, offset relativo a +0x18)
      +0x24 (>IIi): dl_primitives (buf_size, cmd_size, offset relativo a +0x24)
      +0x3C (>I): index
      +0x40 (>I): num_vertices
      +0x44 (>I): num_polygons
      +0x48 (>h): vtx_pos_idx
      +0x4A (>h): vtx_nrm_idx
      +0x4C (>2h): vtx_clr_idxs
      +0x50 (>8h): vtx_uv_idxs
    """

    name: str
    index: int
    mtx_id: int
    cp_vcd_lo: int
    cp_vcd_hi: int
    num_vertices: int
    num_polygons: int
    vtx_pos_idx: int
    vtx_nrm_idx: int
    vtx_uv_idxs: Tuple[int, ...]
    dl_bytes: bytes

    @classmethod
    def from_bytes(cls, name: str, buf: bytes, offset: int) -> ResShpData:
        mtx_id, cp_vcd_lo, cp_vcd_hi = struct.unpack_from(">iII", buf, offset + 0x08)
        _, dl2_cmd_size, dl2_rel_off = struct.unpack_from(">IIi", buf, offset + 0x24)
        idx, num_vtx, num_poly = struct.unpack_from(">III", buf, offset + 0x3C)
        vtx_pos_idx, vtx_nrm_idx = struct.unpack_from(">hh", buf, offset + 0x48)
        vtx_uv_idxs = struct.unpack_from(">8h", buf, offset + 0x50)
        dl_start = offset + 0x24 + dl2_rel_off
        dl_bytes = buf[dl_start : dl_start + dl2_cmd_size]
        return cls(
            name=name,
            index=idx,
            mtx_id=mtx_id,
            cp_vcd_lo=cp_vcd_lo,
            cp_vcd_hi=cp_vcd_hi,
            num_vertices=num_vtx,
            num_polygons=num_poly,
            vtx_pos_idx=vtx_pos_idx,
            vtx_nrm_idx=vtx_nrm_idx,
            vtx_uv_idxs=vtx_uv_idxs,
            dl_bytes=dl_bytes,
        )


@dataclass
class DrawCall:
    """Instrução `0x04` (Draw) do bytecode `DrawOpa` / `DrawXlu`."""

    mat_idx: int
    shp_idx: int
    bone_idx: int
    priority: int


@dataclass
class DecodedVertex:
    """Vértice 3D processado no espaço do modelo com posição, normal e UV."""

    position: Tuple[float, float, float]
    normal: Tuple[float, float, float]
    uv: Tuple[float, float]


@dataclass
class DecodedSubMesh:
    """Sub-malha decodificada associada a um polígono (`ResShpData`) e material (`ResMatData`)."""

    shape_name: str
    material_name: str
    texture_name: Optional[str]
    triangles: List[Tuple[DecodedVertex, DecodedVertex, DecodedVertex]]


class Mdl0Model:
    """
    Parser completo de um modelo 3D `MDL0` v11 dentro de um contêiner `BRRES`.
    """

    def __init__(self, buf: bytes, mdl_offset: int) -> None:
        magic = buf[mdl_offset : mdl_offset + 4]
        if magic != MDL0_MAGIC:
            raise ValueError(f"Assinatura MDL0 inválida em {mdl_offset:#x}: {magic!r}")
        self.buf = buf
        self.mdl_offset = mdl_offset
        self.byte_size, self.version, self.bres_rel_off = struct.unpack_from(">IIi", buf, mdl_offset + 0x04)
        self.section_offsets = struct.unpack_from(">14i", buf, mdl_offset + 0x10)
        name_rel = struct.unpack_from(">i", buf, mdl_offset + 0x48)[0]
        name_pos = mdl_offset + name_rel
        name_end = buf.find(b"\x00", name_pos)
        self.name = buf[name_pos:name_end].decode("ascii", errors="replace")

        # MDL0 Header (@ mdl_offset + 0x4C) e Tabela MtxId -> NodeId
        hdr_off = mdl_offset + 0x4C
        self.num_vertices, self.num_polygons = struct.unpack_from(">II", buf, hdr_off + 0x10)
        node_tbl_rel = struct.unpack_from(">I", buf, hdr_off + 0x24)[0]
        node_tbl_pos = hdr_off + node_tbl_rel
        node_tbl_count = struct.unpack_from(">I", buf, node_tbl_pos)[0]
        self.mtx_to_node: Tuple[int, ...] = struct.unpack_from(f">{node_tbl_count}i", buf, node_tbl_pos + 4)

        # Seção 1: Ossos (Bones)
        self.bones_by_id: Dict[int, ResBoneData] = {}
        self.bones_by_name: Dict[str, ResBoneData] = {}
        if self.section_offsets[1] != 0:
            for bname, boff in parse_res_dic(buf, mdl_offset + self.section_offsets[1]):
                bone = ResBoneData.from_bytes(bname, buf, boff)
                self.bones_by_id[bone.node_id] = bone
                self.bones_by_name[bname] = bone

        # Seção 2: Posições de Vértices (XYZ)
        self.vtx_pos_arrays: Dict[int, ResVtxArrayData] = {}
        if self.section_offsets[2] != 0:
            for vname, voff in parse_res_dic(buf, mdl_offset + self.section_offsets[2]):
                arr = ResVtxArrayData.from_bytes(vname, buf, voff, n_components=3)
                self.vtx_pos_arrays[arr.index] = arr

        # Seção 3: Normais de Vértices (XYZ)
        self.vtx_nrm_arrays: Dict[int, ResVtxArrayData] = {}
        if self.section_offsets[3] != 0:
            for nname, noff in parse_res_dic(buf, mdl_offset + self.section_offsets[3]):
                arr = ResVtxArrayData.from_bytes(nname, buf, noff, n_components=3)
                self.vtx_nrm_arrays[arr.index] = arr

        # Seção 5: Coordenadas de Textura (UV)
        self.vtx_uv_arrays: Dict[int, ResVtxArrayData] = {}
        if self.section_offsets[5] != 0:
            for uname, uoff in parse_res_dic(buf, mdl_offset + self.section_offsets[5]):
                arr = ResVtxArrayData.from_bytes(uname, buf, uoff, n_components=2)
                self.vtx_uv_arrays[arr.index] = arr

        # Seção 8: Materiais
        self.materials: Dict[int, ResMatData] = {}
        if self.section_offsets[8] != 0:
            for mname, moff in parse_res_dic(buf, mdl_offset + self.section_offsets[8]):
                mat = ResMatData.from_bytes(mname, buf, moff)
                self.materials[mat.index] = mat

        # Seção 10: Shapes (Polígonos e Display Lists GX)
        self.shapes: Dict[int, ResShpData] = {}
        if self.section_offsets[10] != 0:
            for sname, soff in parse_res_dic(buf, mdl_offset + self.section_offsets[10]):
                shp = ResShpData.from_bytes(sname, buf, soff)
                self.shapes[shp.index] = shp

        # Seção 0: ByteCode (DrawOpa / DrawXlu)
        self.draw_calls: List[DrawCall] = []
        if self.section_offsets[0] != 0:
            bcs = dict(parse_res_dic(buf, mdl_offset + self.section_offsets[0]))
            for bc_name in ("DrawOpa", "DrawXlu"):
                if bc_name in bcs:
                    self._parse_draw_bytecode(bcs[bc_name])

    def _parse_draw_bytecode(self, bc_offset: int) -> None:
        pos = bc_offset
        while pos < len(self.buf):
            op = self.buf[pos]
            if op in (0x00, 0x01):  # NOP / RET
                break
            if op == 0x04:  # DRAW (mat_idx: u16, shp_idx: u16, bone_idx: u16, prio: u8)
                mat_idx, shp_idx, bone_idx, prio = struct.unpack_from(">HHHB", self.buf, pos + 1)
                self.draw_calls.append(DrawCall(mat_idx, shp_idx, bone_idx, prio))
                pos += 8
            else:
                break

    def get_matrix_for_id(self, mtx_id: int) -> Matrix3x4:
        """
        Retorna a matriz 3x4 de transformação para `mtx_id`:
        - Se `mtx_to_node[mtx_id] >= 0` (osso rígido), os vértices estão no espaço local do osso
          e são transformados por `bone.world_matrix`.
        - Se `mtx_to_node[mtx_id] == -1` (envelope ponderado via `NodeMix` op `0x03`),
          os vértices já estão armazenados em espaço do modelo (bind pose = identidade).
        """
        if 0 <= mtx_id < len(self.mtx_to_node):
            node_id = self.mtx_to_node[mtx_id]
            if node_id >= 0 and node_id in self.bones_by_id:
                return self.bones_by_id[node_id].world_matrix
        return IDENTITY_MTX3X4

    def decode_submeshes(self, root_transform: Matrix3x4 = IDENTITY_MTX3X4) -> List[DecodedSubMesh]:
        """Decodifica todas as sub-malhas do modelo executando as Display Lists GX."""
        submeshes: List[DecodedSubMesh] = []
        for dc in self.draw_calls:
            shp = self.shapes[dc.shp_idx]
            mat = self.materials[dc.mat_idx]
            triangles = self._decode_shape_dl(shp, root_transform)
            submeshes.append(
                DecodedSubMesh(
                    shape_name=shp.name,
                    material_name=mat.name,
                    texture_name=mat.primary_diffuse_texture,
                    triangles=triangles,
                )
            )
        return submeshes

    def _decode_shape_dl(
        self, shp: ResShpData, root_transform: Matrix3x4
    ) -> List[Tuple[DecodedVertex, DecodedVertex, DecodedVertex]]:
        """
        Interpreta a Display List GX (`dl_bytes`) de `shp`, decodificando:
        - `0x20` (`GX_LOAD_INDX_A`): carrega matriz de posição no slot XF `PNMTX[slot]`
        - `0x28` (`GX_LOAD_INDX_B`), `0x30` (`GX_LOAD_INDX_C`), `0x38` (`GX_LOAD_INDX_D`)
        - `0x90` (`GX_TRIANGLES`), `0x98` (`GX_TRIANGLESTRIP`), `0xA0` (`GX_TRIANGLEFAN`), `0x80` (`GX_QUADS`)
        """
        pos_arr = self.vtx_pos_arrays[shp.vtx_pos_idx].values
        nrm_arr = self.vtx_nrm_arrays[shp.vtx_nrm_idx].values if shp.vtx_nrm_idx >= 0 else [(0.0, 1.0, 0.0)]
        uv_arrays: List[Optional[List[Tuple[float, ...]]]] = [
            self.vtx_uv_arrays[uidx].values if uidx >= 0 and uidx in self.vtx_uv_arrays else None
            for uidx in shp.vtx_uv_idxs
        ]

        # Decodifica CP_VCD_LO e CP_VCD_HI para determinar o layout exato de cada vértice no stream GX
        vcd_lo = shp.cp_vcd_lo
        vcd_hi = shp.cp_vcd_hi
        has_pmidx = bool(vcd_lo & 0x1)
        num_tex_mtx_idx = sum(1 for b in range(1, 9) if (vcd_lo >> b) & 0x1)
        pos_idx_fmt = (vcd_lo >> 9) & 0x3   # 2 = u8, 3 = u16
        nrm_idx_fmt = (vcd_lo >> 11) & 0x3  # 2 = u8, 3 = u16
        col0_idx_fmt = (vcd_lo >> 13) & 0x3
        col1_idx_fmt = (vcd_lo >> 15) & 0x3
        tex_idx_fmts = [(vcd_hi >> (i * 2)) & 0x3 for i in range(8)]

        pnmtx_table: Dict[int, int] = {}
        dl = shp.dl_bytes
        pos = 0
        dl_len = len(dl)
        triangles: List[Tuple[DecodedVertex, DecodedVertex, DecodedVertex]] = []

        def read_idx(fmt_code: int) -> int:
            nonlocal pos
            if fmt_code == 2:
                val = dl[pos]
                pos += 1
                return val
            if fmt_code == 3:
                val = struct.unpack_from(">H", dl, pos)[0]
                pos += 2
                return val
            return -1

        while pos < dl_len:
            op = dl[pos]
            if op == 0x00:  # GX_NOP (padding de alinhamento de 32 bytes no final da DL)
                pos += 1
                continue
            if op in (0x20, 0x28, 0x30, 0x38):  # GX_LOAD_INDX_A..D (5 bytes)
                mtx_id, xf_info = struct.unpack_from(">HH", dl, pos + 1)
                if op == 0x20:  # Pos Matrix Index
                    xf_addr = xf_info & 0x0FFF
                    slot = xf_addr // 12
                    pnmtx_table[slot * 3] = mtx_id
                pos += 5
                continue

            prim_type = op & 0xF8
            if prim_type in (0x80, 0x90, 0x98, 0xA0):
                vtx_count = struct.unpack_from(">H", dl, pos + 1)[0]
                pos += 3
                prim_verts: List[DecodedVertex] = []
                for _ in range(vtx_count):
                    pmidx = dl[pos] if has_pmidx else -1
                    if has_pmidx:
                        pos += 1
                    pos += num_tex_mtx_idx  # Pula T0MIDX..T7MIDX se presentes

                    pi = read_idx(pos_idx_fmt)
                    ni = read_idx(nrm_idx_fmt)
                    if col0_idx_fmt:
                        read_idx(col0_idx_fmt)
                    if col1_idx_fmt:
                        read_idx(col1_idx_fmt)

                    uv_indices: List[int] = []
                    for ti in range(8):
                        if tex_idx_fmts[ti]:
                            uv_indices.append(read_idx(tex_idx_fmts[ti]))
                        else:
                            uv_indices.append(-1)

                    # Determina a matriz de transformação do vértice
                    if has_pmidx and pmidx in pnmtx_table:
                        vtx_mtx = self.get_matrix_for_id(pnmtx_table[pmidx])
                    elif shp.mtx_id >= 0:
                        vtx_mtx = self.get_matrix_for_id(shp.mtx_id)
                    else:
                        vtx_mtx = IDENTITY_MTX3X4

                    raw_pos = pos_arr[pi]
                    raw_nrm = nrm_arr[ni] if 0 <= ni < len(nrm_arr) else (0.0, 1.0, 0.0)
                    model_pos = transform_point(vtx_mtx, (raw_pos[0], raw_pos[1], raw_pos[2]))
                    model_nrm = transform_normal(vtx_mtx, (raw_nrm[0], raw_nrm[1], raw_nrm[2]))
                    final_pos = transform_point(root_transform, model_pos)
                    final_nrm = transform_normal(root_transform, model_nrm)

                    # Seleciona o canal UV ativo (suporta multi-UV de expressões faciais como em EndoHead)
                    u_coord, v_coord = 0.0, 0.0
                    active_channels = [
                        (ch, uv_indices[ch])
                        for ch in range(8)
                        if uv_indices[ch] >= 0 and uv_arrays[ch] is not None
                    ]
                    if len(active_channels) == 1:
                        ch, uidx = active_channels[0]
                        u_coord, v_coord = uv_arrays[ch][uidx][0], uv_arrays[ch][uidx][1]  # type: ignore[index]
                    elif len(active_channels) >= 3:
                        # Em modelos de cabeça (ex: 0001_endo), UV#0 = boca (quando u0 > 0),
                        # UV#1 = olhos (quando u1 > 0) e UV#2 = mapa base completo da cabeça/cabelo
                        chosen_ch, chosen_uidx = active_channels[-1]
                        for ch, uidx in active_channels[:-1]:
                            if uidx > 0:
                                chosen_ch, chosen_uidx = ch, uidx
                                break
                        u_coord, v_coord = (
                            uv_arrays[chosen_ch][chosen_uidx][0],  # type: ignore[index]
                            uv_arrays[chosen_ch][chosen_uidx][1],  # type: ignore[index]
                        )
                    elif active_channels:
                        ch, uidx = active_channels[0]
                        u_coord, v_coord = uv_arrays[ch][uidx][0], uv_arrays[ch][uidx][1]  # type: ignore[index]

                    # Inverte V para o padrão Wavefront .obj (origem no canto inferior esquerdo)
                    prim_verts.append(
                        DecodedVertex(
                            position=final_pos,
                            normal=final_nrm,
                            uv=(u_coord, 1.0 - v_coord),
                        )
                    )

                # Converte primitiva GX em lista de triângulos (CCW)
                if prim_type == 0x90:  # GX_TRIANGLES
                    for i in range(0, len(prim_verts) - 2, 3):
                        triangles.append((prim_verts[i], prim_verts[i + 1], prim_verts[i + 2]))
                elif prim_type == 0x98:  # GX_TRIANGLESTRIP
                    for i in range(len(prim_verts) - 2):
                        if i % 2 == 0:
                            triangles.append((prim_verts[i], prim_verts[i + 1], prim_verts[i + 2]))
                        else:
                            triangles.append((prim_verts[i + 1], prim_verts[i], prim_verts[i + 2]))
                elif prim_type == 0xA0:  # GX_TRIANGLEFAN
                    for i in range(1, len(prim_verts) - 1):
                        triangles.append((prim_verts[0], prim_verts[i], prim_verts[i + 1]))
                elif prim_type == 0x80:  # GX_QUADS
                    for i in range(0, len(prim_verts) - 3, 4):
                        triangles.append((prim_verts[i], prim_verts[i + 1], prim_verts[i + 2]))
                        triangles.append((prim_verts[i], prim_verts[i + 2], prim_verts[i + 3]))
            else:
                raise ValueError(f"Opcode GX desconhecido {op:#x} em {shp.name} pos={pos:#x}")

        return triangles


# =============================================================================
# Decodificador de Texturas Wii GX (TEX0 v3 -> RGBA8 -> PNG)
# =============================================================================


@dataclass
class Tex0Texture:
    """
    Estrutura `TEX0` v3 (`BRRES`) e decodificador de blocos de pixels do Wii GX.
      +0x00 (4s): 'TEX0'
      +0x04 (>I): byte_size
      +0x08 (>I): version (3)
      +0x0C (>i): bres_offset
      +0x10 (>i): data_offset (início dos blocos de pixels em relação a TEX0)
      +0x14 (>i): name_offset
      +0x18 (>I): flags
      +0x1C (>H): width
      +0x1E (>H): height
      +0x20 (>I): format (GXTexFmt: 0=I4, 1=I8, 2=IA4, 3=IA8, 4=RGB565, 5=RGB5A3, 6=RGBA8, 0xE=CMPR)
      +0x24 (>I): mipmap_count
    """

    name: str
    width: int
    height: int
    fmt_code: int
    mipmap_count: int
    rgba_bytes: bytes

    @property
    def format_name(self) -> str:
        return GX_TEX_FORMAT_NAMES.get(self.fmt_code, f"UNKNOWN_{self.fmt_code:#x}")

    @classmethod
    def from_bytes(cls, buf: bytes, tex_offset: int) -> Tex0Texture:
        magic = buf[tex_offset : tex_offset + 4]
        if magic != TEX0_MAGIC:
            raise ValueError(f"Assinatura TEX0 inválida em {tex_offset:#x}: {magic!r}")
        data_off, name_off, _ = struct.unpack_from(">iiI", buf, tex_offset + 0x10)
        width, height, fmt_code, mip_cnt = struct.unpack_from(">HHII", buf, tex_offset + 0x1C)
        name_pos = tex_offset + name_off
        name_end = buf.find(b"\x00", name_pos)
        name = buf[name_pos:name_end].decode("ascii", errors="replace")
        raw_pixels = buf[tex_offset + data_off :]
        rgba = cls._decode_gx_pixels(raw_pixels, width, height, fmt_code)
        return cls(
            name=name,
            width=width,
            height=height,
            fmt_code=fmt_code,
            mipmap_count=mip_cnt,
            rgba_bytes=rgba,
        )

    @staticmethod
    def _rgb565_to_rgb(val: int) -> Tuple[int, int, int]:
        r = ((val >> 11) & 0x1F) * 255 // 31
        g = ((val >> 5) & 0x3F) * 255 // 63
        b = (val & 0x1F) * 255 // 31
        return (r, g, b)

    @classmethod
    def _decode_gx_pixels(cls, raw: bytes, width: int, height: int, fmt: int) -> bytes:
        """Converte blocos swizzled (tiled) do Wii GX para buffer linear RGBA8."""
        out = bytearray(width * height * 4)

        def set_px(x: int, y: int, r: int, g: int, b: int, a: int) -> None:
            if x < width and y < height:
                idx = (y * width + x) * 4
                out[idx] = r
                out[idx + 1] = g
                out[idx + 2] = b
                out[idx + 3] = a

        pos = 0
        if fmt == 0x0:  # I4 (blocos 8x8, 4 bpp)
            for ty in range(0, height, 8):
                for tx in range(0, width, 8):
                    for py in range(8):
                        for px in range(0, 8, 2):
                            byte = raw[pos]
                            pos += 1
                            i0 = (byte >> 4) * 17
                            i1 = (byte & 0xF) * 17
                            set_px(tx + px, ty + py, i0, i0, i0, 255)
                            set_px(tx + px + 1, ty + py, i1, i1, i1, 255)

        elif fmt == 0x1:  # I8 (blocos 8x4, 8 bpp)
            for ty in range(0, height, 4):
                for tx in range(0, width, 8):
                    for py in range(4):
                        for px in range(8):
                            val = raw[pos]
                            pos += 1
                            set_px(tx + px, ty + py, val, val, val, 255)

        elif fmt == 0x2:  # IA4 (blocos 8x4, 8 bpp: high nibble = Alpha, low nibble = Intensity)
            for ty in range(0, height, 4):
                for tx in range(0, width, 8):
                    for py in range(4):
                        for px in range(8):
                            byte = raw[pos]
                            pos += 1
                            alpha = ((byte >> 4) & 0xF) * 17
                            inten = (byte & 0xF) * 17
                            set_px(tx + px, ty + py, inten, inten, inten, alpha)

        elif fmt == 0x3:  # IA8 (blocos 4x4, 16 bpp)
            for ty in range(0, height, 4):
                for tx in range(0, width, 4):
                    for py in range(4):
                        for px in range(4):
                            alpha, inten = raw[pos], raw[pos + 1]
                            pos += 2
                            set_px(tx + px, ty + py, inten, inten, inten, alpha)

        elif fmt == 0x4:  # RGB565 (blocos 4x4, 16 bpp Big-Endian)
            for ty in range(0, height, 4):
                for tx in range(0, width, 4):
                    for py in range(4):
                        for px in range(4):
                            val = struct.unpack_from(">H", raw, pos)[0]
                            pos += 2
                            r, g, b = cls._rgb565_to_rgb(val)
                            set_px(tx + px, ty + py, r, g, b, 255)

        elif fmt == 0x5:  # RGB5A3 (blocos 4x4, 16 bpp Big-Endian)
            for ty in range(0, height, 4):
                for tx in range(0, width, 4):
                    for py in range(4):
                        for px in range(4):
                            val = struct.unpack_from(">H", raw, pos)[0]
                            pos += 2
                            if val & 0x8000:  # Sem transparência: RGB555
                                r = ((val >> 10) & 0x1F) * 255 // 31
                                g = ((val >> 5) & 0x1F) * 255 // 31
                                b = (val & 0x1F) * 255 // 31
                                a = 255
                            else:  # Com transparência: A3 R4 G4 B4
                                a = ((val >> 12) & 0x7) * 255 // 7
                                r = ((val >> 8) & 0xF) * 17
                                g = ((val >> 4) & 0xF) * 17
                                b = (val & 0xF) * 17
                            set_px(tx + px, ty + py, r, g, b, a)

        elif fmt == 0x6:  # RGBA8 (blocos 4x4, 64 bytes: 32B AR + 32B GB)
            for ty in range(0, height, 4):
                for tx in range(0, width, 4):
                    for py in range(4):
                        for px in range(4):
                            i = py * 4 + px
                            a = raw[pos + i * 2]
                            r = raw[pos + i * 2 + 1]
                            g = raw[pos + 32 + i * 2]
                            b = raw[pos + 32 + i * 2 + 1]
                            set_px(tx + px, ty + py, r, g, b, a)
                    pos += 64

        elif fmt == 0xE:  # CMPR (Wii DXT1: macro-blocos 8x8 de 4 sub-blocos 4x4 Big-Endian)
            for ty in range(0, height, 8):
                for tx in range(0, width, 8):
                    for sy in (0, 4):
                        for sx in (0, 4):
                            c0, c1, bits = struct.unpack_from(">HHI", raw, pos)
                            pos += 8
                            r0, g0, b0 = cls._rgb565_to_rgb(c0)
                            r1, g1, b1 = cls._rgb565_to_rgb(c1)
                            if c0 > c1:
                                palette = (
                                    (r0, g0, b0, 255),
                                    (r1, g1, b1, 255),
                                    ((2 * r0 + r1) // 3, (2 * g0 + g1) // 3, (2 * b0 + b1) // 3, 255),
                                    ((r0 + 2 * r1) // 3, (g0 + 2 * g1) // 3, (b0 + 2 * b1) // 3, 255),
                                )
                            else:
                                palette = (
                                    (r0, g0, b0, 255),
                                    (r1, g1, b1, 255),
                                    ((r0 + r1) // 2, (g0 + g1) // 2, (b0 + b1) // 2, 255),
                                    (0, 0, 0, 0),
                                )
                            for py in range(4):
                                for px in range(4):
                                    shift = 30 - 2 * (py * 4 + px)
                                    pidx = (bits >> shift) & 0x3
                                    pr, pg, pb, pa = palette[pidx]
                                    set_px(tx + sx + px, ty + sy + py, pr, pg, pb, pa)
        else:
            raise ValueError(f"Formato de textura GX não suportado: {fmt:#x}")

        return bytes(out)

    def save_png(self, out_path: Path) -> None:
        """Codifica e salva o buffer RGBA8 em arquivo PNG padrão (via zlib/struct)."""
        out_path.parent.mkdir(parents=True, exist_ok=True)

        def make_chunk(chunk_type: bytes, data: bytes) -> bytes:
            crc = zlib.crc32(chunk_type + data) & 0xFFFFFFFF
            return struct.pack(">I", len(data)) + chunk_type + data + struct.pack(">I", crc)

        ihdr = struct.pack(">IIBBBBB", self.width, self.height, 8, 6, 0, 0, 0)
        stride = self.width * 4
        raw_scanlines = bytearray()
        for y in range(self.height):
            raw_scanlines.append(0)  # Filter type 0 (None)
            raw_scanlines.extend(self.rgba_bytes[y * stride : (y + 1) * stride])
        idat = zlib.compress(bytes(raw_scanlines), level=9)

        png_bytes = (
            b"\x89PNG\r\n\x1a\n"
            + make_chunk(b"IHDR", ihdr)
            + make_chunk(b"IDAT", idat)
            + make_chunk(b"IEND", b"")
        )
        out_path.write_bytes(png_bytes)


# =============================================================================
# Leitor de Arquivo BRRES (`bres`) e Exportador Wavefront `.obj` / `.mtl`
# =============================================================================


class BrresArchive:
    """Extrai recursos `MDL0` e `TEX0` de um arquivo `BRRES` (`bres`)."""

    def __init__(self, buf: bytes) -> None:
        if buf[:4] != BRRES_MAGIC:
            raise ValueError(f"Magic BRRES inválido: {buf[:4]!r}")
        self.buf = buf
        self.models: List[Mdl0Model] = []
        self.textures: Dict[str, Tex0Texture] = {}
        self._scan_subfiles()

    def _scan_subfiles(self) -> None:
        root_off = struct.unpack_from(">H", self.buf, 0x0C)[0]
        folders = parse_res_dic(self.buf, root_off + 0x08)
        for folder_name, folder_dic_off in folders:
            for item_name, item_off in parse_res_dic(self.buf, folder_dic_off):
                sig = self.buf[item_off : item_off + 4]
                if sig == MDL0_MAGIC:
                    self.models.append(Mdl0Model(self.buf, item_off))
                elif sig == TEX0_MAGIC:
                    tex = Tex0Texture.from_bytes(self.buf, item_off)
                    self.textures[tex.name] = tex


def export_obj_and_mtl(
    obj_path: Path,
    mtl_path: Path,
    submeshes: List[DecodedSubMesh],
    tex_rel_dir: str = "textures",
) -> Tuple[int, int]:
    """
    Exporta uma lista de `DecodedSubMesh` para `.obj` + `.mtl`.
    Retorna `(total_vertices, total_triangles)`.
    """
    obj_path.parent.mkdir(parents=True, exist_ok=True)

    mtl_lines = [
        "# Gerado por scripts/extract_models.py (Inazuma Eleven Strikers 2013)",
        "",
    ]
    emitted_mats = set()
    for sm in submeshes:
        if sm.material_name not in emitted_mats:
            emitted_mats.add(sm.material_name)
            mtl_lines.extend(
                [
                    f"newmtl {sm.material_name}",
                    "Ka 1.0000 1.0000 1.0000",
                    "Kd 1.0000 1.0000 1.0000",
                    "Ks 0.0000 0.0000 0.0000",
                    "d 1.0",
                    "illum 1",
                ]
            )
            if sm.texture_name:
                mtl_lines.append(f"map_Kd {tex_rel_dir}/{sm.texture_name}.png")
            mtl_lines.append("")
    mtl_path.write_text("\n".join(mtl_lines), encoding="utf-8")

    obj_lines = [
        "# Gerado por scripts/extract_models.py (Inazuma Eleven Strikers 2013)",
        f"mtllib {mtl_path.name}",
        "",
    ]

    v_idx = 1
    total_tris = 0
    for sm in submeshes:
        obj_lines.append(f"g {sm.shape_name}_{sm.material_name}")
        obj_lines.append(f"usemtl {sm.material_name}")
        for tri in sm.triangles:
            for vtx in tri:
                px, py, pz = vtx.position
                nx, ny, nz = vtx.normal
                u, v = vtx.uv
                obj_lines.append(f"v {px:.6f} {py:.6f} {pz:.6f}")
                obj_lines.append(f"vt {u:.6f} {v:.6f}")
                obj_lines.append(f"vn {nx:.6f} {ny:.6f} {nz:.6f}")
            i0, i1, i2 = v_idx, v_idx + 1, v_idx + 2
            obj_lines.append(f"f {i0}/{i0}/{i0} {i1}/{i1}/{i1} {i2}/{i2}/{i2}")
            v_idx += 3
            total_tris += 1
        obj_lines.append("")

    obj_path.write_text("\n".join(obj_lines), encoding="utf-8")
    return (v_idx - 1, total_tris)


# =============================================================================
# Pipeline Principal de Extração
# =============================================================================


@dataclass
class ExtractedAssetReport:
    """Resumo de auditoria de cada modelo 3D e conjunto de texturas extraído."""

    asset_key: str
    description: str
    model_def: ModelDefEntry
    tex_set_def: TexSetDefEntry
    mdl_grp_index: int
    mdl_name: str
    bone_count: int
    submesh_count: int
    exported_vertices: int
    exported_triangles: int
    obj_file: str
    mtl_file: str
    textures: List[Dict[str, object]] = field(default_factory=list)


def load_idx75_tables(dat_path: Path) -> Tuple[List[ModelDefEntry], List[TexSetDefEntry]]:
    """Carrega a tabela de modelos (`sec[0]`) e conjuntos de texturas (`sec[1]`) de `dat.bin` (`idx=75`)."""
    dat = StrikersDatParser(dat_path)
    buf, hdr = dat.get_subfile(75)
    sec0 = hdr.sections[0]
    sec1 = hdr.sections[1]

    mdl_defs = [
        ModelDefEntry.from_bytes(buf, sec0.byte_offset + i * 0x5C, i)
        for i in range(sec0.entry_count)
    ]
    tex_sets = [
        TexSetDefEntry.from_bytes(buf, sec1.byte_offset + i * 0x28, i)
        for i in range(sec1.entry_count)
    ]
    return mdl_defs, tex_sets


def extract_asset(
    asset_key: str,
    description: str,
    model_def_id: int,
    grp: GrpContainer,
    mdl_defs: List[ModelDefEntry],
    tex_sets: List[TexSetDefEntry],
    out_models_dir: Path,
    out_textures_dir: Path,
) -> Tuple[ExtractedAssetReport, Mdl0Model, List[DecodedSubMesh]]:
    """Extrai um modelo individual (`MDL0`) e todas as suas texturas (`TEX0`) vinculadas."""
    mdef = mdl_defs[model_def_id]
    tset = tex_sets[mdef.tex_set_index]

    # 1. Descomprime e analisa o BRRES do modelo 3D
    mdl_grp_idx = mdef.mdl_file_id - 1
    mdl_brres = BrresArchive(grp.get_by_file_id(mdef.mdl_file_id))
    if not mdl_brres.models:
        raise ValueError(f"Nenhum MDL0 encontrado em grp[{mdl_grp_idx}]")
    mdl = mdl_brres.models[0]

    # 2. Descomprime e exporta todas as texturas TEX0 do conjunto
    tex_reports: List[Dict[str, object]] = []
    for tex_file_id in tset.tex_file_ids:
        tex_brres = BrresArchive(grp.get_by_file_id(tex_file_id))
        for tname, tex in tex_brres.textures.items():
            png_path = out_textures_dir / f"{tname}.png"
            tex.save_png(png_path)
            tex_reports.append(
                {
                    "name": tname,
                    "grp_file_id": tex_file_id,
                    "grp_index": tex_file_id - 1,
                    "width": tex.width,
                    "height": tex.height,
                    "format_code": f"{tex.fmt_code:#x}",
                    "format_name": tex.format_name,
                    "png_path": str(png_path),
                }
            )

    # 3. Decodifica as Display Lists GX e exporta .obj / .mtl
    submeshes = mdl.decode_submeshes()
    obj_path = out_models_dir / f"{asset_key}.obj"
    mtl_path = out_models_dir / f"{asset_key}.mtl"
    v_cnt, tri_cnt = export_obj_and_mtl(obj_path, mtl_path, submeshes)

    report = ExtractedAssetReport(
        asset_key=asset_key,
        description=description,
        model_def=mdef,
        tex_set_def=tset,
        mdl_grp_index=mdl_grp_idx,
        mdl_name=mdl.name,
        bone_count=len(mdl.bones_by_id),
        submesh_count=len(submeshes),
        exported_vertices=v_cnt,
        exported_triangles=tri_cnt,
        obj_file=str(obj_path),
        mtl_file=str(mtl_path),
        textures=tex_reports,
    )
    return report, mdl, submeshes


def main() -> None:
    dat_path = Path("extracted/files/dat.bin")
    grp_path = Path("extracted/files/grp.bin")
    out_models_dir = Path("output/models")
    out_textures_dir = out_models_dir / "textures"

    mdl_defs, tex_sets = load_idx75_tables(dat_path)
    grp = GrpContainer(grp_path)

    # 1. Bola de Futebol padrão (idx75[3889] -> ball0000_normal em grp[1116], texturas em grp[14])
    ball_report, _, _ = extract_asset(
        asset_key="ball",
        description="Bola de Futebol Padrão (ball0000_normal)",
        model_def_id=3889,
        grp=grp,
        mdl_defs=mdl_defs,
        tex_sets=tex_sets,
        out_models_dir=out_models_dir,
        out_textures_dir=out_textures_dir,
    )

    # 2. Cabeça de Endo Mamoru (cid=1, P_0001ENDO -> idx75[2481] -> 0001_endo em grp[3435])
    head_report, head_mdl, _ = extract_asset(
        asset_key="endo_head",
        description="Endo Mamoru - Cabeça (0001_endo)",
        model_def_id=2481,
        grp=grp,
        mdl_defs=mdl_defs,
        tex_sets=tex_sets,
        out_models_dir=out_models_dir,
        out_textures_dir=out_textures_dir,
    )

    # 3. Corpo de Goleiro da Raimon (idx75[400] -> RAI_GKH_CHIBI_00 em grp[1139])
    body_report, body_mdl, body_submeshes = extract_asset(
        asset_key="endo_body",
        description="Endo Mamoru - Corpo Uniforme GK Raimon (RAI_GKH_CHIBI_00)",
        model_def_id=400,
        grp=grp,
        mdl_defs=mdl_defs,
        tex_sets=tex_sets,
        out_models_dir=out_models_dir,
        out_textures_dir=out_textures_dir,
    )

    # 4. Personagem Completo Combinado (Corpo + Cabeça acoplada à matriz do osso `head`)
    head_bone_mtx = body_mdl.bones_by_name["head"].world_matrix
    transformed_head_submeshes = head_mdl.decode_submeshes(root_transform=head_bone_mtx)
    full_submeshes = body_submeshes + transformed_head_submeshes
    full_obj_path = out_models_dir / "endo_mamoru.obj"
    full_mtl_path = out_models_dir / "endo_mamoru.mtl"
    full_v_cnt, full_tri_cnt = export_obj_and_mtl(full_obj_path, full_mtl_path, full_submeshes)

    manifest = {
        "container": "extracted/files/grp.bin",
        "model_table_source": "extracted/files/dat.bin (idx=75 / 0x9C8B)",
        "total_grp_entries": grp.entry_count,
        "total_model_defs": len(mdl_defs),
        "total_tex_sets": len(tex_sets),
        "extracted_models": [
            asdict(ball_report),
            asdict(head_report),
            asdict(body_report),
            {
                "asset_key": "endo_mamoru",
                "description": "Endo Mamoru Completo (Corpo RAI_GKH_CHIBI_00 + Cabeça 0001_endo acoplada ao osso head)",
                "head_attach_bone": "head",
                "head_bone_translation": list(body_mdl.bones_by_name["head"].world_matrix[i][3] for i in range(3)),
                "submesh_count": len(full_submeshes),
                "exported_vertices": full_v_cnt,
                "exported_triangles": full_tri_cnt,
                "obj_file": str(full_obj_path),
                "mtl_file": str(full_mtl_path),
            },
        ],
    }

    manifest_path = out_models_dir / "extraction_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")

    print("=== Extração de Modelos 3D e Texturas Concluída ===")
    for item in manifest["extracted_models"]:
        print(
            f"  - {item['asset_key']:12s}: {item['exported_vertices']:5d} vértices, "
            f"{item['exported_triangles']:4d} triângulos -> {item['obj_file']}"
        )
    print(f"  - Texturas exportadas em: {out_textures_dir}")
    print(f"  - Manifesto salvo em:     {manifest_path}")


if __name__ == "__main__":
    main()
