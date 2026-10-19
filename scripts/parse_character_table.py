#!/usr/bin/env python3
"""
Decodificador modular das tabelas de personagens, atributos e técnicas (Hissatsu)
de Inazuma Eleven GO Strikers 2013 (Wii - RVL-R5GJ-JPN, Engine Shade/Level-5).

Tabelas decodificadas a partir de `extracted/files/dat.bin` (Big-Endian '>'):
- `idx = 1`  (`0x9C41`): Master String Table (Shift-JIS, 7.579 entradas)
- `idx = 9`  (`0x9C49`): Tabela de Atributos Base/Máx (`sec[0]`, 20 bytes) e Loadout de Hissatsu (`sec[2]`, 48 bytes)
- `idx = 15` (`0x9C4F`): Character Master Table (`sec[0]`, 412 registros × 328 bytes / `0x148`) e Uniformes/Times (`sec[1]`, 38 × 96 bytes)
- `idx = 19..73` (`0x9C53..0x9C89`): Tabelas de Elencos de Times (28 tabelas ímpares, até 16 jogadores × 68 bytes / `0x44` por variação)
- `idx = 79` (`0x9C8F`): Tabela de Nomes e Metadados Visuais de Hissatsu (`sec[5]`, 477 × 100 bytes / `0x64`)
- `idx = 81` (`0x9C91`): Tabela de Parâmetros de Jogo e Vínculos Co-op de Hissatsu (`sec[0]`, 477 × 140 bytes / `0x8C`)
"""

from __future__ import annotations

import json
import re
import struct
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Dict, List, Tuple


# =============================================================================
# Constantes Binárias e Mapeamentos de Domínio (Big-Endian '>')
# =============================================================================

SHADE_LZSS_MAGIC = b"\xfc\xaa\x55\xa7"
VIRTUAL_FILE_BASE = 0x9C40

# Mapeamento de Elementos (+0x0F4 em idx=15 e +0x08 em idx=81)
ELEMENT_MAP: Dict[int, str] = {
    0: "Wind (風 / Kaze)",
    1: "Wood (林 / Hayashi)",
    2: "Fire (火 / Hi)",
    3: "Mountain (山 / Yama)",
}

# Mapeamento de Posição (+0x05C em idx=15 e +0x04 em TeamPlayerRecord; 0 é normalizado para 34/GK em main.dol @ 0x80009EF4)
POSITION_MAP: Dict[int, str] = {
    0: "GK (Goalkeeper)",
    34: "GK (Goalkeeper)",
    35: "DF (Defender)",
    36: "MF (Midfielder)",
    37: "FW (Forward)",
    38: "Manager / Coach",
}

BODY_TYPE_MAP: Dict[int, str] = {
    0: "Standard (Normal)",
    1: "Small / Female",
    2: "Large / Heavy",
}

# Mapeamento de Categoria de Hissatsu (+0x00 uint32 em idx=79 sec[5])
HISSATSU_CATEGORY_MAP: Dict[int, str] = {
    0: "Shoot (シュート)",
    1: "Dribble (ドリブル)",
    2: "Block / Defense (ブロック)",
    3: "Catch / GK (キャッチ)",
}

# Atributo Secundário de Hissatsu (+0x0A uint16 em idx=81 sec[0])
HISSATSU_ATTRIBUTE_MAP: Dict[int, str] = {
    0: "None",
    1: "Long Shoot (ロング)",
    2: "Shoot Block (シュートブロック)",
    3: "Chain Shoot (チェイン)",
    4: "Catch / Punching (パンチング)",
}


# =============================================================================
# Descompressor Shade LZSS / RLE (Engenharia Reversa de `main.dol` @ 0x80333C5C)
# =============================================================================

def shade_decompress(src: bytes) -> bytes:
    """
    Descomprime um fluxo Shade LZSS/RLE (`0x80333C5C` em `main.dol`).
    Suporta tanto blocos precedidos pelo cabeçalho opcional de 12 bytes (`FC AA 55 A7`)
    quanto fluxos LZSS/RLE puros armazenados diretamente nos sub-arquivos de `dat.bin`.
    """
    pos = 12 if (len(src) >= 12 and src[:4] == SHADE_LZSS_MAGIC) else 0
    n = len(src)
    out = bytearray()

    while pos < n:
        cmd = src[pos]
        pos += 1
        if cmd == 0:
            break

        if cmd & 0x80:
            # Backreference LZSS: 4..7 bytes iniciais + extensões opcionais `011x xxxx`
            count = ((cmd >> 5) & 0x03) + 4
            if pos >= n:
                raise EOFError("Fim inesperado do buffer durante leitura de offset LZSS.")
            off = ((cmd & 0x1F) << 8) | src[pos]
            pos += 1
            if off == 0 or off > len(out):
                raise ValueError(f"Offset LZSS inválido: {off} (out_len={len(out)})")
            ref = len(out) - off
            for _ in range(count):
                out.append(out[ref])
                ref += 1
            while pos < n and (src[pos] & 0xE0) == 0x60:
                extra = src[pos] & 0x1F
                pos += 1
                for _ in range(extra):
                    out.append(out[ref])
                    ref += 1

        elif cmd & 0x40:
            # Repetição RLE de byte único
            if cmd & 0x10:
                if pos >= n:
                    raise EOFError("Fim inesperado do buffer durante leitura de contagem RLE estendida.")
                count = (((cmd & 0x0F) << 8) | src[pos]) + 4
                pos += 1
            else:
                count = (cmd & 0x0F) + 4
            if pos >= n:
                raise EOFError("Fim inesperado do buffer durante leitura de valor RLE.")
            val = src[pos]
            pos += 1
            out.extend([val] * count)

        else:
            # Sequência literal de bytes
            if cmd & 0x20:
                if pos >= n:
                    raise EOFError("Fim inesperado do buffer durante leitura de contagem literal estendida.")
                count = ((cmd & 0x1F) << 8) | src[pos]
                pos += 1
            else:
                count = cmd & 0x1F
            if pos + count > n:
                raise EOFError("Sequência literal excede o tamanho do bloco comprimido.")
            out.extend(src[pos : pos + count])
            pos += count

    return bytes(out)


# =============================================================================
# Modelos Declarativos (@dataclass) com Desempacotamento Big-Endian ('>')
# =============================================================================

@dataclass
class DatSubfileDescriptor:
    """Descritor de sub-arquivo no índice principal de `dat.bin` (`0x14 + idx * 4`)."""
    index: int
    virtual_file_id: str
    raw_descriptor: int
    byte_offset: int
    allocated_size: int
    decompressed_size: int
    endianness: str


@dataclass
class SectionDescriptor:
    """Descritor de seção de 8 bytes (`>II`) no cabeçalho de uma tabela descomprimida."""
    section_index: int
    byte_offset: int
    entry_count: int


@dataclass
class RelocatableTableHeader:
    """
    Cabeçalho padrão das tabelas Big-Endian (`>`) de `dat.bin`:
      - `+0x00` (`>I`): `section_count` (N)
      - `+0x04` (`>I`): `reloc_table_offset` (offset para tabela de relocação de ponteiros)
      - `+0x08` (`>I`): `first_section_offset` (`0x0C + N * 8`)
      - `+0x0C` (`>II` * N): Descritores `(section_offset, entry_count)`
    """
    HEADER_STRUCT = struct.Struct(">III")
    SECTION_STRUCT = struct.Struct(">II")

    section_count: int
    reloc_table_offset: int
    first_section_offset: int
    payload_size: int
    relocated_word_count: int
    sections: List[SectionDescriptor] = field(default_factory=list)

    @classmethod
    def from_bytes(cls, data: bytes, table_name: str = "") -> "RelocatableTableHeader":
        if len(data) < cls.HEADER_STRUCT.size:
            raise ValueError(f"[{table_name}] Buffer menor que 12 bytes ({len(data)} bytes).")
        if len(data) % 4 != 0:
            raise ValueError(f"[{table_name}] Tamanho descomprimido ({len(data)}) não alinhado a 4 bytes.")

        sec_count, reloc_off, first_sec_off = cls.HEADER_STRUCT.unpack_from(data, 0x00)
        expected_first_off = 0x0C + sec_count * 8
        if first_sec_off != expected_first_off:
            raise ValueError(
                f"[{table_name}] Offset da primeira seção inválido: lido={first_sec_off:#x}, "
                f"esperado={expected_first_off:#x} (section_count={sec_count})."
            )
        if reloc_off % 4 != 0 or reloc_off > len(data) - 4:
            raise ValueError(
                f"[{table_name}] Offset da tabela de relocação inválido ou desalinhado: "
                f"{reloc_off:#x} (len={len(data):#x})."
            )

        reloc_word_count = struct.unpack_from(">I", data, reloc_off)[0]
        sections: List[SectionDescriptor] = []
        for i in range(sec_count):
            s_off, s_cnt = cls.SECTION_STRUCT.unpack_from(data, 0x0C + i * 8)
            if s_off != 0 and (s_off % 4 != 0 or s_off > len(data)):
                raise ValueError(
                    f"[{table_name}] Seção {i} com offset desalinhado ou fora do buffer: {s_off:#x}"
                )
            sections.append(SectionDescriptor(section_index=i, byte_offset=s_off, entry_count=s_cnt))

        return cls(
            section_count=sec_count,
            reloc_table_offset=reloc_off,
            first_section_offset=first_sec_off,
            payload_size=len(data),
            relocated_word_count=reloc_word_count,
            sections=sections,
        )


@dataclass
class HissatsuTechnique:
    """
    Registro unificado de técnica (Hissatsu) combinando:
      - `idx = 79` (`0x9C8F`) `sec[5]` (stride `0x64` = 100 bytes, Big-Endian `>25I`):
          - `+0x00` (`>I`): `category_id` (`0`=Shoot, `1`=Dribble, `2`=Block, `3`=Catch)
          - `+0x04` (`>I`): `internal_name_offset` (offset para string Shift-JIS em `idx=79`)
          - `+0x08` (`>I`): `name_str_id` (índice em `idx=1` `sec[1]`)
          - `+0x0C` (`>I`): `evolved_str_id` (índice em `idx=1` `sec[1]`)
          - `+0x10` (`>I`): `ruby_str_id` (índice em `idx=1` `sec[1]`)
      - `idx = 81` (`0x9C91`) `sec[0]` (stride `0x8C` = 140 bytes, Big-Endian `>70H`):
          - `+0x00` (`>H`): `level_tier_raw` (`0`=Lv1, `1`=Lv2, `2`=Lv3, `3`=Co-op/Ultimate)
          - `+0x02` (`>H`): `base_power`
          - `+0x04` (`>H`): `max_power`
          - `+0x06` (`>H`): `tp_cost`
          - `+0x08` (`>H`): `element_id` (`0`=Wind, `1`=Wood, `2`=Fire, `3`=Mountain)
          - `+0x0A` (`>H`): `special_attribute_id` (`0`=None, `1`=Long, `2`=Block, `3`=Chain, `4`=Punch)
          - `+0x14` (`>H`): `participant_count_raw` (`0`=1P, `2`=2P Co-op, `3`=3P Co-op)
          - `+0x16` (`>H`): `kizuna_bond_required` (`0`, `500`, `700`, `900`)
          - `+0x1A..+0x2D` (`>10H`): `primary_user_char_ids`
          - `+0x2E..+0x41` (`>10H`): `partner_1_char_ids`
          - `+0x42..+0x4B` (`>5H`):  `partner_2_char_ids`
    """
    META_STRIDE = 100   # 0x64 bytes em idx=79 sec[5]
    PARAM_STRIDE = 140  # 0x8C bytes em idx=81 sec[0]

    hissatsu_id: int
    name_str_id: int
    name_byte_offset: int
    name: str
    internal_name_offset: int
    evolved_str_id: int
    evolved_name: str
    ruby_str_id: int
    ruby: str
    category_id: int
    category_name: str
    element_id: int
    element_name: str
    special_attribute_id: int
    special_attribute_name: str
    level_tier: int
    base_power: int
    max_power: int
    tp_cost: int
    participant_count: int
    kizuna_bond_required: int
    primary_user_char_ids: List[int]
    partner_1_char_ids: List[int]
    partner_2_char_ids: List[int]


@dataclass
class CharacterAttributes:
    """
    Bloco de 20 bytes (`0x14`) de atributos Base e Máximo em `idx = 9` (`0x9C49`) `sec[0]`.
    Decodificado em `main.dol` @ `0x80026C68`:
      - `+0x00..+0x01`: TP (Base, Max)
      - `+0x02..+0x03`: Kick (Base, Max)
      - `+0x04..+0x05`: Catch (Base, Max)
      - `+0x06..+0x07`: Body (Base, Max)
      - `+0x08..+0x09`: Control (Base, Max)
      - `+0x0A..+0x0B`: Guard (Base, Max)
      - `+0x0C..+0x0D`: Speed (Base, Max)
      - `+0x0E`: Keshin / Dark Emperors Stat Bonus (`0` ou `15`)
      - `+0x0F`: Reserved (`0`)
      - `+0x10`: Inazuma Gauge Fill Rate (`100`)
      - `+0x11..+0x13`: Padding (`0, 0, 0`)
    """
    STRUCT = struct.Struct(">20B")
    STRIDE = 20

    tp_base: int
    tp_max: int
    kick_base: int
    kick_max: int
    catch_base: int
    catch_max: int
    body_base: int
    body_max: int
    control_base: int
    control_max: int
    guard_base: int
    guard_max: int
    speed_base: int
    speed_max: int
    keshin_stat_bonus: int
    reserved_0f: int
    inazuma_gauge_rate: int
    padding: Tuple[int, int, int]

    @classmethod
    def from_bytes(cls, data: bytes, offset: int) -> "CharacterAttributes":
        if offset + cls.STRIDE > len(data):
            raise ValueError(f"Overflow ao ler CharacterAttributes em {offset:#x}.")
        vals = cls.STRUCT.unpack_from(data, offset)
        return cls(
            tp_base=vals[0],
            tp_max=vals[1],
            kick_base=vals[2],
            kick_max=vals[3],
            catch_base=vals[4],
            catch_max=vals[5],
            body_base=vals[6],
            body_max=vals[7],
            control_base=vals[8],
            control_max=vals[9],
            guard_base=vals[10],
            guard_max=vals[11],
            speed_base=vals[12],
            speed_max=vals[13],
            keshin_stat_bonus=vals[14],
            reserved_0f=vals[15],
            inazuma_gauge_rate=vals[16],
            padding=(vals[17], vals[18], vals[19]),
        )


@dataclass
class LinkedHissatsuSlot:
    """Referência resolvida a uma técnica vinculada a um personagem."""
    slot_index: int
    slot_type: str
    hissatsu_id: int
    name: str
    category: str
    element: str
    special_attribute: str
    level_tier: int
    base_power: int
    max_power: int
    tp_cost: int
    participant_count: int


@dataclass
class CharacterHissatsuLoadout:
    """
    Bloco de 48 bytes (`0x30` = `>24H`) de técnicas intrínsecas em `idx = 9` (`0x9C49`) `sec[2]`,
    enriquecido com as técnicas de elenco (`idx = 19..73`) e vínculos de `idx = 81`:
      - `u16[0..4]`   (`+0x00..+0x09`): Slots de Shoot (`Lv1, Lv2, Lv3, Alt, Extra`)
      - `u16[5..8]`   (`+0x0A..+0x11`): Slots de Dribble (`Lv1, Lv2, Lv3, Alt`)
      - `u16[9..12]`  (`+0x12..+0x19`): Slots de Block/Defense (`Lv1, Lv2, Lv3, Alt`)
      - `u16[13..16]` (`+0x1A..+0x21`): Slots de Catch/GK (`Lv1, Lv2, Lv3, Alt`)
      - `u16[17..23]` (`+0x22..+0x2F`): Slots Especiais / Co-op / Keshin (`u16[21]` em `+0x2A` = Keshin/Ultimate)
    """
    STRUCT = struct.Struct(">24H")
    STRIDE = 48

    shoot_slots: List[int]
    dribble_slots: List[int]
    block_slots: List[int]
    catch_slots: List[int]
    special_slots: List[int]
    intrinsic_techniques: List[LinkedHissatsuSlot]
    team_assigned_techniques: List[LinkedHissatsuSlot]
    compatible_techniques_from_waza_table: List[LinkedHissatsuSlot]


@dataclass
class TeamAppearance:
    """Registro de participação do personagem em uma tabela de elenco (`idx = 19..73`)."""
    team_file_idx: int
    team_virtual_id: str
    team_name: str
    preset_index: int
    jersey_number: int
    position_id: int
    position_name: str
    formation_slot: int
    stat_block_index: int
    assigned_hissatsu_ids: List[int]


@dataclass
class CharacterNames:
    """Strings e offsets de nomes resolvidos a partir de `idx = 1` (`0x9C41`)."""
    full_name_str_id: int
    full_name_byte_offset: int
    full_name_raw: str
    full_name: str
    furigana: str
    short_name_str_id: int
    short_name_byte_offset: int
    short_name: str
    hud_name_str_id: int
    hud_name_byte_offset: int
    hud_name: str
    bio_str_id: int
    bio_byte_offset: int
    bio_raw: str
    bio: str


@dataclass
class CharacterPhysicalParams:
    """Parâmetros físicos e de modelo 3D dentro do registro de 328 bytes (`idx = 15`)."""
    grade_or_category: int   # +0x34 (>I)
    model_scale_a: int       # +0x40 (>I)
    model_scale_b: int       # +0x44 (>I)
    head_size_param: int     # +0x48 (>I)
    body_type_id: int        # +0x4C (>I)
    body_type_name: str
    skin_variant: int        # +0x58 (>I)
    skin_color_rgba_1: str   # +0xE8 (>I hex RGBA)
    skin_color_rgba_2: str   # +0xEC (>I hex RGBA)
    age_or_grade_group: int  # +0xF8 (>I)
    body_build_group: int    # +0x100 (>I)


@dataclass
class CharacterAssetRefs:
    """Índices de assets gráficos (`grp.bin`) e de áudio (`idx = 103`) do personagem."""
    anim_skeleton_group: int    # +0x60 (>I)
    model_grp_id_1: int         # +0x64 (>I)
    model_grp_id_2: int         # +0x68 (>I)
    face_icon_grp_id: int       # +0x78 (>I)
    bust_up_grp_id_1: int       # +0x80 (>I)
    bust_up_grp_id_2: int       # +0x84 (>I)
    socks_model_code: str       # +0x88..+0x9F (24s ASCII)
    foot_model_code: str        # +0xA0..+0xBF (32s ASCII)
    voice_table_index: int      # +0x104 (>I)


@dataclass
class CharacterRecord:
    """
    Registro mestre de personagem de 328 bytes (`0x148`) em `idx = 15` (`0x9C4F`) `sec[0]`,
    enriquecido com os atributos e técnicas de `idx = 9` (`0x9C49`), `idx = 81` e elencos `idx = 19..73`.
    """
    STRIDE = 0x148  # 328 bytes exatos

    character_id: int
    record_byte_offset: int
    base_person_id: int
    model_code: str
    default_jersey_number: int
    element_id: int
    element_name: str
    position_id: int
    position_name: str
    team_affiliation_flag_1: int
    team_affiliation_flag_2: int
    names: CharacterNames
    physical_params: CharacterPhysicalParams
    asset_refs: CharacterAssetRefs
    attributes: CharacterAttributes
    hissatsu_loadout: CharacterHissatsuLoadout
    team_appearances: List[TeamAppearance]


# =============================================================================
# Utilitários de Decodificação Shift-JIS e Furigana
# =============================================================================

FURIGANA_PATTERN = re.compile(r"#R(?:#=)?([^\(\)#]+)\(([^\)]+)\)")
CTRL_TAG_PATTERN = re.compile(r"#R(?:[xX][0-9a-fA-F]+|m-?\d+|#=|#)")


def parse_furigana_markup(raw_text: str) -> Tuple[str, str]:
    """
    Converte a marcação de furigana da Shade/Level-5, ex.:
      `#Rx10#R#=円堂(えんどう)　#R守(まもる)`
    em:
      - `clean_name`: `'円堂 守'`
      - `furigana`:   `'えんどう まもる'`
    """
    no_ctrl = re.sub(r"#R(?:[xX][0-9a-fA-F]+|m-?\d+)(?:#=)?", "", raw_text)
    kanji_parts: List[str] = []
    reading_parts: List[str] = []
    pos = 0

    for match in FURIGANA_PATTERN.finditer(no_ctrl):
        prefix = no_ctrl[pos : match.start()]
        kanji_parts.append(prefix)
        reading_parts.append(prefix)
        kanji_parts.append(match.group(1))
        reading_parts.append(match.group(2))
        pos = match.end()

    tail = no_ctrl[pos:]
    kanji_parts.append(tail)
    reading_parts.append(tail)

    clean_kanji = CTRL_TAG_PATTERN.sub("", "".join(kanji_parts)).replace("\u3000", " ").strip()
    clean_reading = CTRL_TAG_PATTERN.sub("", "".join(reading_parts)).replace("\u3000", " ").strip()
    return clean_kanji, clean_reading


def read_ascii_fixed(buf: bytes, offset: int, length: int) -> str:
    """Lê uma string ASCII de tamanho fixo preenchida com `0x00`."""
    raw_slice = buf[offset : offset + length]
    nul_pos = raw_slice.find(b"\x00")
    if nul_pos != -1:
        raw_slice = raw_slice[:nul_pos]
    return raw_slice.decode("ascii", errors="strict")


def read_shift_jis_cstring(buf: bytes, offset: int) -> str:
    """Lê uma string terminada em nulo codificada em Shift-JIS (`cp932`)."""
    if offset <= 0 or offset >= len(buf):
        return ""
    end = buf.find(b"\x00", offset)
    if end == -1:
        end = len(buf)
    return buf[offset:end].decode("cp932", errors="strict")


# =============================================================================
# Parser Principal do Contêiner `dat.bin`
# =============================================================================

class StrikersDatParser:
    """Leitor e validador estrutural das tabelas de `extracted/files/dat.bin`."""

    CONTAINER_HEADER_STRUCT = struct.Struct("<5I")

    def __init__(self, dat_path: Path) -> None:
        self.dat_path = dat_path
        self.raw_bytes = dat_path.read_bytes()
        self.descriptors: List[DatSubfileDescriptor] = []
        self._decompressed_cache: Dict[int, bytes] = {}
        self._parse_container_index()

    def _parse_container_index(self) -> None:
        """
        Valida o cabeçalho de 20 bytes de `dat.bin` e lê os 130 descritores de sub-arquivos.
        """
        if len(self.raw_bytes) < 0x14:
            raise ValueError("Arquivo dat.bin truncado (< 20 bytes).")

        count, align, size_mult, shift, mask = self.CONTAINER_HEADER_STRUCT.unpack_from(
            self.raw_bytes, 0x00
        )
        if count != 130 or align != 0x800 or size_mult != 0x20 or shift != 17 or mask != 0x1FFFF:
            raise ValueError(
                f"Parâmetros de cabeçalho dat.bin inesperados: count={count}, "
                f"align={align:#x}, size_mult={size_mult:#x}, shift={shift}, mask={mask:#x}"
            )

        raw_entries = struct.unpack_from(f"<{count}I", self.raw_bytes, 0x14)
        for idx, val in enumerate(raw_entries):
            offset = (val >> shift) * align
            alloc_size = (val & mask) * size_mult
            if offset % align != 0:
                raise ValueError(f"Sub-arquivo {idx} desalinhado de {align:#x}: {offset:#x}")
            if offset + alloc_size > len(self.raw_bytes):
                raise ValueError(
                    f"Sub-arquivo {idx} excede os limites de dat.bin: "
                    f"offset={offset:#x}, size={alloc_size:#x}, total={len(self.raw_bytes):#x}"
                )
            if alloc_size == 0:
                raise ValueError(f"Sub-arquivo {idx} possui tamanho alocado zero.")

            endian = "Big-Endian (>)" if (idx % 2 == 1) else "Little-Endian (<)"
            self.descriptors.append(
                DatSubfileDescriptor(
                    index=idx,
                    virtual_file_id=f"0x{VIRTUAL_FILE_BASE + idx:04X}",
                    raw_descriptor=val,
                    byte_offset=offset,
                    allocated_size=alloc_size,
                    decompressed_size=0,
                    endianness=endian,
                )
            )

    def get_subfile(self, idx: int) -> Tuple[bytes, RelocatableTableHeader]:
        """
        Descomprime um sub-arquivo Big-Endian (índice ímpar) e valida seu cabeçalho e integridade
        comparando-o com seu espelho Little-Endian (`idx - 1`).
        """
        if idx % 2 == 0:
            raise ValueError(f"O índice {idx} é Little-Endian; use o índice ímpar Big-Endian ({idx + 1}).")

        if idx not in self._decompressed_cache:
            desc_be = self.descriptors[idx]
            desc_le = self.descriptors[idx - 1]

            raw_be = self.raw_bytes[desc_be.byte_offset : desc_be.byte_offset + desc_be.allocated_size]
            raw_le = self.raw_bytes[desc_le.byte_offset : desc_le.byte_offset + desc_le.allocated_size]

            dec_be = shade_decompress(raw_be)
            dec_le = shade_decompress(raw_le)

            if len(dec_be) != len(dec_le):
                raise ValueError(
                    f"Falha de integridade no par ({idx-1}, {idx}): "
                    f"len(LE)={len(dec_le)} != len(BE)={len(dec_be)}"
                )

            desc_be.decompressed_size = len(dec_be)
            desc_le.decompressed_size = len(dec_le)
            self._decompressed_cache[idx] = dec_be

        payload = self._decompressed_cache[idx]
        header = RelocatableTableHeader.from_bytes(
            payload, table_name=f"idx={idx} ({VIRTUAL_FILE_BASE + idx:#06x})"
        )
        return payload, header

    def parse_master_strings(self) -> Tuple[List[int], List[str]]:
        """
        Decodifica `idx = 1` (`0x9C41`): Master String Table em Shift-JIS (`cp932`).
        `sec[1]` (`off = 0x42C`, `count = 7579`) contém 7.579 offsets de 32 bits Big-Endian (`>I`).
        """
        buf, hdr = self.get_subfile(1)
        sec1 = hdr.sections[1]
        offsets = list(struct.unpack_from(f">{sec1.entry_count}I", buf, sec1.byte_offset))
        strings: List[str] = []
        for sid, off in enumerate(offsets):
            if off >= len(buf):
                raise ValueError(f"Offset de string fora dos limites em str[{sid}]: {off:#x}")
            strings.append(read_shift_jis_cstring(buf, off))
        return offsets, strings

    def parse_hissatsu_tables(
        self, str_offsets: List[int], strings: List[str]
    ) -> Dict[int, HissatsuTechnique]:
        """
        Decodifica todas as 477 técnicas (Hissatsu) cruzando:
          - `idx = 79` (`0x9C8F`) `sec[5]` (477 × 100 bytes / `0x64`)
          - `idx = 81` (`0x9C91`) `sec[0]` (477 × 140 bytes / `0x8C`)
        """
        buf79, hdr79 = self.get_subfile(79)
        buf81, hdr81 = self.get_subfile(81)

        sec79 = hdr79.sections[5]
        sec81 = hdr81.sections[0]

        if sec79.entry_count != sec81.entry_count:
            raise ValueError(
                f"Contagem divergente de Hissatsu entre idx=79 ({sec79.entry_count}) "
                f"e idx=81 ({sec81.entry_count})."
            )

        expected_79_end = sec79.byte_offset + sec79.entry_count * HissatsuTechnique.META_STRIDE
        expected_81_end = sec81.byte_offset + sec81.entry_count * HissatsuTechnique.PARAM_STRIDE
        if expected_79_end > len(buf79) or expected_81_end > len(buf81):
            raise ValueError("Overflow na leitura das seções de Hissatsu (idx=79 / idx=81).")

        hissatsu_map: Dict[int, HissatsuTechnique] = {}
        for wid in range(sec79.entry_count):
            off79 = sec79.byte_offset + wid * HissatsuTechnique.META_STRIDE
            off81 = sec81.byte_offset + wid * HissatsuTechnique.PARAM_STRIDE

            meta_u32 = struct.unpack_from(">25I", buf79, off79)
            param_u16 = struct.unpack_from(">70H", buf81, off81)

            category_id = meta_u32[0]
            internal_name_off = meta_u32[1]
            name_sid = meta_u32[2]
            evol_sid = meta_u32[3]
            ruby_sid = meta_u32[4]

            raw_tier = param_u16[0]
            base_power = param_u16[1]
            max_power = param_u16[2]
            tp_cost = param_u16[3]
            element_id = param_u16[4]
            spec_attr_id = param_u16[5]
            raw_participants = param_u16[10]
            kizuna_req = param_u16[11]

            primary_users = [cid for cid in param_u16[13:23] if 0 < cid <= 411]
            partner_1_ids = [cid for cid in param_u16[23:33] if 0 < cid <= 411]
            partner_2_ids = [cid for cid in param_u16[33:38] if 0 < cid <= 411]

            name_text = strings[name_sid] if 0 < name_sid < len(strings) else ""
            name_off = str_offsets[name_sid] if 0 < name_sid < len(str_offsets) else 0
            evol_text = strings[evol_sid] if 0 < evol_sid < len(strings) else ""
            ruby_text = strings[ruby_sid] if 0 < ruby_sid < len(strings) else ""

            hissatsu_map[wid] = HissatsuTechnique(
                hissatsu_id=wid,
                name_str_id=name_sid,
                name_byte_offset=name_off,
                name=name_text,
                internal_name_offset=internal_name_off,
                evolved_str_id=evol_sid,
                evolved_name=evol_text,
                ruby_str_id=ruby_sid,
                ruby=ruby_text,
                category_id=category_id,
                category_name=HISSATSU_CATEGORY_MAP.get(category_id, f"Unknown ({category_id})"),
                element_id=element_id,
                element_name=ELEMENT_MAP.get(element_id, f"Unknown ({element_id})"),
                special_attribute_id=spec_attr_id,
                special_attribute_name=HISSATSU_ATTRIBUTE_MAP.get(
                    spec_attr_id, f"Unknown ({spec_attr_id})"
                ),
                level_tier=raw_tier + 1 if wid > 0 else 0,
                base_power=base_power,
                max_power=max_power,
                tp_cost=tp_cost,
                participant_count=raw_participants if raw_participants > 0 else (1 if wid > 0 else 0),
                kizuna_bond_required=kizuna_req,
                primary_user_char_ids=primary_users,
                partner_1_char_ids=partner_1_ids,
                partner_2_char_ids=partner_2_ids,
            )

        return hissatsu_map

    def parse_team_rosters(self) -> Dict[int, List[TeamAppearance]]:
        """
        Percorre todas as 28 tabelas de elencos de times (`idx = 19, 21, ..., 73` / `0x9C53..0x9C89`).
        Em cada tabela (`main.dol` @ `0x80009D58..0x8000A17C`):
          - `sec[0]` contém `N` ponteiros (`>I`) para descritores de variação de time de 24 bytes (`0x18`).
          - Cada descritor de 24 bytes (`>IIHHHHII`) aponta em `+0x00` para o nome do time (Shift-JIS)
            e em `+0x10` para um array de até 16 registros de jogadores de 68 bytes (`0x44`):
              - `+0x00` (`>H`): `character_id`
              - `+0x02` (`>H`): `jersey_number`
              - `+0x04` (`>I`): `position_id` (`34`=GK, `35`=DF, `36`=MF, `37`=FW)
              - `+0x08` (`>I`): `stat_block_index` (índice em `sec[1]`)
              - `+0x0C` (`>I`): `formation_slot` (`1..16`)
              - `+0x20..+0x41` (`>17H`): 17 slots de técnicas Hissatsu atribuídas no time
        """
        appearances_by_char: Dict[int, List[TeamAppearance]] = {}
        player_struct = struct.Struct(">HHIIIIIII17HH")
        if player_struct.size != 0x44:
            raise AssertionError("Tamanho incorreto para TeamPlayerRecord (esperado 0x44 = 68 bytes).")

        for idx in range(19, 75, 2):
            buf, hdr = self.get_subfile(idx)
            sec0 = hdr.sections[0]
            variant_ptrs = struct.unpack_from(f">{sec0.entry_count}I", buf, sec0.byte_offset)

            for preset_idx, v_ptr in enumerate(variant_ptrs):
                if v_ptr == 0:
                    continue
                if v_ptr + 0x18 > len(buf):
                    raise ValueError(f"Descritor de time fora dos limites em idx={idx}: {v_ptr:#x}")

                str_off, _u4, _emblem, _coach, _uniform, _form, players_off, tac_off = (
                    struct.unpack_from(">IIHHHHII", buf, v_ptr)
                )
                team_name = read_shift_jis_cstring(buf, str_off)

                end_bound = tac_off if (0 < tac_off <= len(buf)) else len(buf)
                if players_off >= end_bound:
                    raise ValueError(
                        f"Offset de jogadores inválido em idx={idx}: {players_off:#x} (end={end_bound:#x})"
                    )

                for slot in range(16):
                    poff = players_off + slot * 0x44
                    if poff + 0x44 > end_bound:
                        break
                    unpacked = player_struct.unpack_from(buf, poff)
                    cid = unpacked[0]
                    if cid == 0:
                        break
                    jersey_num = unpacked[1]
                    pos_id = unpacked[2]
                    stat_idx = unpacked[3]
                    form_slot = unpacked[4]
                    waza_ids = [w for w in unpacked[9:26] if w != 0]

                    norm_pos_id = 34 if pos_id == 0 else pos_id
                    app = TeamAppearance(
                        team_file_idx=idx,
                        team_virtual_id=f"0x{VIRTUAL_FILE_BASE + idx:04X}",
                        team_name=team_name,
                        preset_index=preset_idx,
                        jersey_number=jersey_num,
                        position_id=norm_pos_id,
                        position_name=POSITION_MAP.get(norm_pos_id, f"Unknown ({pos_id})"),
                        formation_slot=form_slot,
                        stat_block_index=stat_idx,
                        assigned_hissatsu_ids=waza_ids,
                    )
                    appearances_by_char.setdefault(cid, []).append(app)

        return appearances_by_char

    def parse_characters(
        self,
        str_offsets: List[int],
        strings: List[str],
        hissatsu_map: Dict[int, HissatsuTechnique],
        team_appearances: Dict[int, List[TeamAppearance]],
    ) -> List[CharacterRecord]:
        """
        Decodifica todos os 412 registros da Character Master Table (`idx = 15`, `0x9C4F`, `sec[0]`)
        e vincula:
          - Atributos Base/Máx de 20 bytes (`idx = 9`, `0x9C49`, `sec[0]`)
          - Loadout de Hissatsu de 48 bytes (`idx = 9`, `0x9C49`, `sec[2]`)
          - Técnicas adicionais atribuídas nos elencos de times (`idx = 19..73`)
          - Técnicas compatíveis / Co-op listadas em `idx = 81` (`0x9C91`)
        """
        buf15, hdr15 = self.get_subfile(15)
        buf9, hdr9 = self.get_subfile(9)

        sec15_chars = hdr15.sections[0]
        sec9_stats = hdr9.sections[0]
        sec9_wazas = hdr9.sections[2]

        if sec15_chars.entry_count != 412 or sec9_stats.entry_count != 412:
            raise ValueError(
                f"Contagem de personagens inesperada: idx15={sec15_chars.entry_count}, "
                f"idx9_stats={sec9_stats.entry_count}"
            )
        if sec9_wazas.entry_count != 411:
            raise ValueError(f"Contagem de loadouts de Hissatsu inesperada: {sec9_wazas.entry_count}")

        # Validação estrita de tamanho das seções (esperado vs lido)
        expected_15_bytes = sec15_chars.entry_count * CharacterRecord.STRIDE
        actual_15_bytes = hdr15.reloc_table_offset - sec15_chars.byte_offset
        if actual_15_bytes != expected_15_bytes:
            raise ValueError(
                f"Falha de integridade em idx=15 sec[0]: tamanho esperado={expected_15_bytes}, "
                f"lido={actual_15_bytes}"
            )

        expected_9_stats_bytes = sec9_stats.entry_count * CharacterAttributes.STRIDE
        actual_9_stats_bytes = hdr9.sections[1].byte_offset - sec9_stats.byte_offset
        if actual_9_stats_bytes != expected_9_stats_bytes:
            raise ValueError(
                f"Falha de integridade em idx=9 sec[0]: tamanho esperado={expected_9_stats_bytes}, "
                f"lido={actual_9_stats_bytes}"
            )

        expected_9_wazas_bytes = sec9_wazas.entry_count * CharacterHissatsuLoadout.STRIDE
        actual_9_wazas_bytes = hdr9.sections[3].byte_offset - sec9_wazas.byte_offset
        if actual_9_wazas_bytes != expected_9_wazas_bytes:
            raise ValueError(
                f"Falha de integridade em idx=9 sec[2]: tamanho esperado={expected_9_wazas_bytes}, "
                f"lido={actual_9_wazas_bytes}"
            )

        def build_linked_slot(slot_idx: int, slot_type: str, wid: int) -> LinkedHissatsuSlot:
            tech = hissatsu_map.get(wid)
            if tech is None:
                raise ValueError(f"Hissatsu ID {wid} (slot {slot_idx}) inexistente em idx=79/81.")
            return LinkedHissatsuSlot(
                slot_index=slot_idx,
                slot_type=slot_type,
                hissatsu_id=wid,
                name=tech.name,
                category=tech.category_name,
                element=tech.element_name,
                special_attribute=tech.special_attribute_name,
                level_tier=tech.level_tier,
                base_power=tech.base_power,
                max_power=tech.max_power,
                tp_cost=tech.tp_cost,
                participant_count=tech.participant_count,
            )

        # Pré-indexa técnicas compatíveis por cid a partir de idx=81 (primary_user_char_ids)
        compatible_by_char: Dict[int, List[int]] = {}
        for wid, tech in hissatsu_map.items():
            if wid == 0:
                continue
            for u_cid in tech.primary_user_char_ids:
                wlist = compatible_by_char.setdefault(u_cid, [])
                if wid not in wlist:
                    wlist.append(wid)

        characters: List[CharacterRecord] = []

        # O índice 0 em idx=15 sec[0] é o registro nulo/dummy; 1..411 são os 411 personagens válidos
        for cid in range(1, sec15_chars.entry_count):
            rec_off = sec15_chars.byte_offset + cid * CharacterRecord.STRIDE
            if rec_off % 4 != 0:
                raise ValueError(f"Registro de personagem {cid} desalinhado: {rec_off:#x}")

            # Leitura Big-Endian ('>') dos campos do registro de 328 bytes (0x148)
            char_id_val, _res04, hud_sid, short_sid, full_sid = struct.unpack_from(
                ">5I", buf15, rec_off + 0x00
            )
            if char_id_val != cid:
                raise ValueError(
                    f"ID de personagem inconsistente no índice {cid}: lido={char_id_val}"
                )

            model_code = read_ascii_fixed(buf15, rec_off + 0x14, 0x20)
            (
                grade_cat,
                bio_sid,
                _unk3c,
                scale_a,
                scale_b,
                head_size,
                body_type_id,
                affil_1,
                affil_2,
                skin_var,
                raw_pos_id,
                anim_skel,
                model_grp_1,
                model_grp_2,
            ) = struct.unpack_from(">14I", buf15, rec_off + 0x34)

            default_jersey, face_grp, _unk7c, bust_grp_1, bust_grp_2 = struct.unpack_from(
                ">5I", buf15, rec_off + 0x74
            )
            socks_model_code = read_ascii_fixed(buf15, rec_off + 0x88, 0x18)
            foot_model_code = read_ascii_fixed(buf15, rec_off + 0xA0, 0x20)
            rgba1, rgba2, base_person_id, element_id, age_group, _unkfc, build_group, voice_idx = (
                struct.unpack_from(">8I", buf15, rec_off + 0xE8)
            )

            # Normalização de posição conforme main.dol @ 0x80009EF4 (0 -> 34 / GK)
            norm_pos_id = 34 if raw_pos_id == 0 else raw_pos_id

            # Resolução de strings e furigana em Shift-JIS (idx=1)
            full_raw = strings[full_sid] if 0 < full_sid < len(strings) else ""
            full_clean, furigana = parse_furigana_markup(full_raw)
            short_clean = (
                strings[short_sid].replace("\u3000", " ").strip()
                if 0 < short_sid < len(strings)
                else ""
            )
            hud_clean = (
                strings[hud_sid].replace("\u3000", " ").strip()
                if 0 < hud_sid < len(strings)
                else ""
            )
            bio_raw = strings[bio_sid] if 0 < bio_sid < len(strings) else ""
            bio_clean, _ = parse_furigana_markup(bio_raw)

            names = CharacterNames(
                full_name_str_id=full_sid,
                full_name_byte_offset=str_offsets[full_sid] if 0 < full_sid < len(str_offsets) else 0,
                full_name_raw=full_raw,
                full_name=full_clean,
                furigana=furigana,
                short_name_str_id=short_sid,
                short_name_byte_offset=str_offsets[short_sid] if 0 < short_sid < len(str_offsets) else 0,
                short_name=short_clean,
                hud_name_str_id=hud_sid,
                hud_name_byte_offset=str_offsets[hud_sid] if 0 < hud_sid < len(str_offsets) else 0,
                hud_name=hud_clean,
                bio_str_id=bio_sid,
                bio_byte_offset=str_offsets[bio_sid] if 0 < bio_sid < len(str_offsets) else 0,
                bio_raw=bio_raw,
                bio=bio_clean,
            )

            physical = CharacterPhysicalParams(
                grade_or_category=grade_cat,
                model_scale_a=scale_a,
                model_scale_b=scale_b,
                head_size_param=head_size,
                body_type_id=body_type_id,
                body_type_name=BODY_TYPE_MAP.get(body_type_id, f"Unknown ({body_type_id})"),
                skin_variant=skin_var,
                skin_color_rgba_1=f"0x{rgba1:08X}",
                skin_color_rgba_2=f"0x{rgba2:08X}",
                age_or_grade_group=age_group,
                body_build_group=build_group,
            )

            assets = CharacterAssetRefs(
                anim_skeleton_group=anim_skel,
                model_grp_id_1=model_grp_1,
                model_grp_id_2=model_grp_2,
                face_icon_grp_id=face_grp,
                bust_up_grp_id_1=bust_grp_1,
                bust_up_grp_id_2=bust_grp_2,
                socks_model_code=socks_model_code,
                foot_model_code=foot_model_code,
                voice_table_index=voice_idx,
            )

            # Atributos Base/Máx (idx=9 sec[0])
            attr_off = sec9_stats.byte_offset + cid * CharacterAttributes.STRIDE
            attributes = CharacterAttributes.from_bytes(buf9, attr_off)

            # Loadout de Hissatsu (idx=9 sec[2], indexado por cid - 1)
            waza_off = sec9_wazas.byte_offset + (cid - 1) * CharacterHissatsuLoadout.STRIDE
            raw_slots = list(CharacterHissatsuLoadout.STRUCT.unpack_from(buf9, waza_off))

            shoot_slots = raw_slots[0:5]
            dribble_slots = raw_slots[5:9]
            block_slots = raw_slots[9:13]
            catch_slots = raw_slots[13:17]
            special_slots = raw_slots[17:24]

            intrinsic_list: List[LinkedHissatsuSlot] = []
            for s_idx, wid in enumerate(raw_slots):
                if wid == 0:
                    continue
                if s_idx < 5:
                    stype = f"Shoot Slot {s_idx + 1}"
                elif s_idx < 9:
                    stype = f"Dribble Slot {s_idx - 4}"
                elif s_idx < 13:
                    stype = f"Block Slot {s_idx - 8}"
                elif s_idx < 17:
                    stype = f"Catch Slot {s_idx - 12}"
                elif s_idx == 21:
                    stype = "Ultimate / Keshin Slot"
                else:
                    stype = f"Special / Alt Slot {s_idx - 16}"
                intrinsic_list.append(build_linked_slot(s_idx, stype, wid))

            # Coleta técnicas atribuídas nos elencos de times (idx=19..73)
            char_apps = team_appearances.get(cid, [])
            seen_team_wazas: Dict[int, LinkedHissatsuSlot] = {}
            for app in char_apps:
                for wid in app.assigned_hissatsu_ids:
                    if wid not in seen_team_wazas:
                        seen_team_wazas[wid] = build_linked_slot(
                            slot_idx=-1,
                            slot_type=f"Team Roster ({app.team_name})",
                            wid=wid,
                        )

            # Coleta técnicas compatíveis / Co-op listadas em idx=81 (sec[0])
            compatible_list = [
                build_linked_slot(slot_idx=-1, slot_type="Compatible / Co-op Primary User", wid=wid)
                for wid in compatible_by_char.get(cid, [])
            ]

            loadout = CharacterHissatsuLoadout(
                shoot_slots=shoot_slots,
                dribble_slots=dribble_slots,
                block_slots=block_slots,
                catch_slots=catch_slots,
                special_slots=special_slots,
                intrinsic_techniques=intrinsic_list,
                team_assigned_techniques=list(seen_team_wazas.values()),
                compatible_techniques_from_waza_table=compatible_list,
            )

            characters.append(
                CharacterRecord(
                    character_id=cid,
                    record_byte_offset=rec_off,
                    base_person_id=base_person_id,
                    model_code=model_code,
                    default_jersey_number=default_jersey,
                    element_id=element_id,
                    element_name=ELEMENT_MAP.get(element_id, f"Unknown ({element_id})"),
                    position_id=norm_pos_id,
                    position_name=POSITION_MAP.get(norm_pos_id, f"Unknown ({raw_pos_id})"),
                    team_affiliation_flag_1=affil_1,
                    team_affiliation_flag_2=affil_2,
                    names=names,
                    physical_params=physical,
                    asset_refs=assets,
                    attributes=attributes,
                    hissatsu_loadout=loadout,
                    team_appearances=char_apps,
                )
            )

        return characters


# =============================================================================
# Ponto de Entrada e Exportação JSON
# =============================================================================

def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent
    dat_path = repo_root / "extracted" / "files" / "dat.bin"
    out_path = repo_root / "output" / "data" / "characters_parsed.json"

    if not dat_path.is_file():
        print(f"[ERRO] Contêiner não encontrado: {dat_path}", file=sys.stderr)
        return 1

    parser = StrikersDatParser(dat_path)

    # 1. Tabela Mestre de Strings (idx = 1 / 0x9C41)
    str_offsets, strings = parser.parse_master_strings()

    # 2. Tabelas de Técnicas Hissatsu (idx = 79 / 0x9C8F & idx = 81 / 0x9C91)
    hissatsu_map = parser.parse_hissatsu_tables(str_offsets, strings)

    # 3. Tabelas de Elencos de Times (idx = 19..73 / 0x9C53..0x9C89)
    team_appearances = parser.parse_team_rosters()

    # 4. Tabela Mestre de Personagens (idx = 15 / 0x9C4F) + Atributos/Loadouts (idx = 9 / 0x9C49)
    characters = parser.parse_characters(str_offsets, strings, hissatsu_map, team_appearances)

    # Resumo de cabeçalhos de todas as tabelas utilizadas
    key_tables_info = {}
    for idx, _label in [
        (1, "Master Shift-JIS String Table (0x9C41)"),
        (9, "Character Attributes & Hissatsu Loadout Table (0x9C49)"),
        (15, "Character Master & Team Uniform Table (0x9C4F)"),
        (79, "Hissatsu Metadata & String Reference Table (0x9C8F)"),
        (81, "Hissatsu Gameplay Parameters Table (0x9C91)"),
    ]:
        _, hdr = parser.get_subfile(idx)
        desc = parser.descriptors[idx]
        key_tables_info[f"idx_{idx}"] = {
            "subfile_index": idx,
            "virtual_file_id": desc.virtual_file_id,
            "container_offset": f"0x{desc.byte_offset:08X}",
            "allocated_compressed_size": desc.allocated_size,
            "decompressed_size": desc.decompressed_size,
            "endianness": desc.endianness,
            "header": asdict(hdr),
        }

    # Filtra entradas válidas de Hissatsu (1..476)
    hissatsu_list = [asdict(hissatsu_map[wid]) for wid in sorted(hissatsu_map) if wid > 0]

    output_payload = {
        "metadata": {
            "game_title": "Inazuma Eleven GO Strikers 2013",
            "game_id": "RVL-R5GJ-JPN",
            "source_container": "extracted/files/dat.bin",
            "compression": "Shade LZSS/RLE (0x80333C5C in main.dol)",
            "native_endianness": "Big-Endian (>)",
            "string_encoding": "Shift-JIS (CP932)",
            "total_characters_parsed": len(characters),
            "total_hissatsu_parsed": len(hissatsu_list),
            "total_master_strings": len(strings),
            "total_team_roster_tables": 28,
        },
        "tables_layout": key_tables_info,
        "characters": [asdict(c) for c in characters],
        "hissatsu_techniques": hissatsu_list,
    }

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(output_payload, f, ensure_ascii=False, indent=2)

    print(f"[OK] Decodificação concluída com sucesso:")
    print(f"  - Personagens decodificados : {len(characters)} (esperado: 411)")
    print(f"  - Técnicas Hissatsu         : {len(hissatsu_list)} (esperado: 476)")
    print(f"  - Strings Shift-JIS (idx=1) : {len(strings)}")
    print(f"  - Arquivo exportado         : {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
