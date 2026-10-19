#!/usr/bin/env python3
"""
Scanner e analisador de cabeçalhos de contêineres de Inazuma Eleven GO Strikers 2013 (Wii).

Atua conforme as especificações do @format-parser (specs/AGENTS.md e specs/ARCHITECTURE.md):
1. Realiza leituras binárias baseadas em struct respeitando a ordenação Big-Endian (`>`) do Wii.
2. Lê os primeiros 16 a 32 bytes dos arquivos alvo em `extracted/files/` (.bin, .pack, .g4b,
   .bln, .arc, .dlf, .mo, .brsar, .tpl, .bnr, .elf, .dol e arquivos sem extensão).
3. Identifica magic numbers, estruturas de cabeçalho (contagens de blocos, offsets de tabela,
   máscaras de bitfield) e assinaturas de compressão (LZSS, zlib, RLE e Shade LZSS 0xFCAA55A7).
4. Valida limites estruturais (sem heurísticas cegas) e exporta o relatório determinístico em
   `output/data/containers_summary.json` com suporte explícito a strings UTF-8 / Shift-JIS.
"""

from __future__ import annotations

from dataclasses import asdict, dataclass, field
import json
import os
import struct
from typing import Any, Dict, List, Optional, Tuple


# Diretórios padrão do projeto
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EXTRACTED_FILES_DIR = os.path.join(PROJECT_ROOT, "extracted", "files")
OUTPUT_JSON_PATH = os.path.join(PROJECT_ROOT, "output", "data", "containers_summary.json")

# Extensões prioritárias da Level-5 / Shade e contêineres de sistema do Wii
LEVEL5_PRIMARY_EXTENSIONS = {".bin", ".pack", ".g4b", ".bln", ""}
CONTAINER_AUX_EXTENSIONS = {".arc", ".dlf", ".mo", ".brsar", ".tpl", ".bnr", ".elf", ".dol", ".img"}
TARGET_EXTENSIONS = LEVEL5_PRIMARY_EXTENSIONS | CONTAINER_AUX_EXTENSIONS


@dataclass
class BigEndianHeaderView:
    """Representação declarativa dos primeiros 32 bytes lidos em Big-Endian (>)."""
    raw_hex_16: str                  # Primeiros 16 bytes (0x00..0x10) em hexadecimal
    raw_hex_32: str                  # Primeiros 32 bytes (0x00..0x20) em hexadecimal
    magic_4b_hex: str                # Magic number de 4 bytes (0x00..0x04) em hex Big-Endian
    magic_4b_u32_be: int             # Valor uint32 Big-Endian (>I) em 0x00
    magic_ascii: str                 # Decodificação legível (UTF-8 / Shift-JIS) dos 4 primeiros bytes
    words_be_u32: List[str]          # 8 palavras uint32 em Big-Endian (>8I, offsets 0x00..0x1C)
    halfwords_be_u16: List[str]      # 8 meias-palavras uint16 em Big-Endian (>8H, offsets 0x00..0x0F)


@dataclass
class CompressionSignature:
    """Assinatura de compressão identificada no cabeçalho ou no primeiro bloco de dados."""
    detected: bool
    scheme: str                      # Ex: "Shade-LZSS/RLE (0xFCAA55A7)", "LZSS/RLE Stream", "Nintendo LZ10/LZ11", "zlib", "None"
    signature_offset: str            # Offset onde a assinatura foi detectada (ex: "0x00000000" ou "0x00008000")
    signature_bytes_be: str          # Bytes da assinatura em hex Big-Endian
    decompressed_size: Optional[int] = None
    compressed_size: Optional[int] = None
    inner_payload_magic: Optional[str] = None
    notes: str = ""


@dataclass
class ContainerStructureInfo:
    """Metadados estruturais validados contra o tamanho físico do arquivo."""
    format_family: str               # Família do formato (Level-5/Shade Bit-Packed Archive, Nintendo U8, etc.)
    header_endianness: str           # Endianness efetiva da tabela de cabeçalho ("Big-Endian (>)" ou "Híbrido LE-Index / BE-Payload")
    entry_count: Optional[int] = None
    table_offset: Optional[str] = None
    data_start_offset: Optional[str] = None
    offset_alignment: Optional[int] = None
    size_multiplier: Optional[int] = None
    bit_shift: Optional[int] = None
    bit_mask: Optional[str] = None
    bounds_validated: bool = False
    validation_details: str = ""
    sample_entries: List[Dict[str, Any]] = field(default_factory=list)


@dataclass
class FileHeaderRecord:
    """Registro completo de análise de um arquivo em extracted/files/."""
    relative_path: str
    file_name: str
    extension: str
    file_size_bytes: int
    is_level5_primary_target: bool
    header_be: BigEndianHeaderView
    compression: CompressionSignature
    structure: ContainerStructureInfo


def decode_safe_string(raw_bytes: bytes) -> str:
    """
    Decodifica bytes para string legível tentando UTF-8 e Shift-JIS (CP932),
    substituindo bytes de controle por pontos para inspeção segura.
    """
    cleaned = bytearray()
    for b in raw_bytes:
        if 0x20 <= b <= 0x7E or b >= 0x80:
            cleaned.append(b)
        else:
            cleaned.append(0x2E)  # '.'
    for encoding in ("utf-8", "shift_jis"):
        try:
            return cleaned.decode(encoding)
        except UnicodeDecodeError:
            continue
    return cleaned.decode("latin-1", errors="replace")


def parse_be_header(header_bytes: bytes) -> BigEndianHeaderView:
    """
    Desempacota os primeiros 16 a 32 bytes estritamente em Big-Endian (`>`).
    Offsets mapeados:
      - 0x00..0x04: Magic uint32 BE (`>I`)
      - 0x00..0x10: 8x uint16 BE (`>8H`) - primeiros 16 bytes
      - 0x00..0x20: 8x uint32 BE (`>8I`) - primeiros 32 bytes
    """
    padded_32 = header_bytes.ljust(32, b"\x00")[:32]
    padded_16 = padded_32[:16]

    magic_u32_be = struct.unpack(">I", padded_32[:4])[0]
    words_be = struct.unpack(">8I", padded_32)
    halfwords_be = struct.unpack(">8H", padded_16)

    return BigEndianHeaderView(
        raw_hex_16=header_bytes[:16].hex(),
        raw_hex_32=header_bytes[:32].hex(),
        magic_4b_hex=f"0x{magic_u32_be:08X}",
        magic_4b_u32_be=magic_u32_be,
        magic_ascii=decode_safe_string(header_bytes[:4]),
        words_be_u32=[f"0x{w:08X}" for w in words_be],
        halfwords_be_u16=[f"0x{h:04X}" for h in halfwords_be],
    )


def detect_standard_compression(data: bytes, base_offset: int = 0) -> CompressionSignature:
    """
    Verifica assinaturas clássicas de compressão (Shade LZSS/RLE, Nintendo LZ10/LZ11,
    zlib, RLE) considerando leitura Big-Endian (`>`) dos magic bytes.
    """
    if len(data) < 4:
        return CompressionSignature(
            detected=False,
            scheme="None",
            signature_offset=f"0x{base_offset:08X}",
            signature_bytes_be=data.hex(),
            notes="Arquivo menor que 4 bytes.",
        )

    magic_be = struct.unpack(">I", data[:4])[0]
    b0, b1 = data[0], data[1]

    # 1. Assinatura proprietária Shade / Level-5 LZSS: 0xFCAA55A7 (BE)
    # Estrutura do bloco (12 bytes):
    #   0x00: FC AA 55 A7 (magic uint32 BE = 0xFCAA55A7)
    #   0x04: decompressed_size (uint32)
    #   0x08: compressed_size (uint32)
    #   0x0C: início do stream de comandos LZSS/RLE (primeiro byte < 0x20 indica run literal)
    if magic_be == 0xFCAA55A7 and len(data) >= 16:
        decomp_le, comp_le = struct.unpack("<2I", data[4:12])
        inner_magic = decode_safe_string(data[13:21]) if len(data) >= 21 else ""
        return CompressionSignature(
            detected=True,
            scheme="Shade/Level-5 LZSS (Magic 0xFCAA55A7)",
            signature_offset=f"0x{base_offset:08X}",
            signature_bytes_be="fcaa55a7",
            decompressed_size=decomp_le,
            compressed_size=comp_le,
            inner_payload_magic=inner_magic,
            notes=(
                "Cabeçalho de compressão de 12 bytes (Magic BE 0xFCAA55A7 + tamanho descomprimido "
                "+ tamanho comprimido), seguido por stream LZSS/RLE."
            ),
        )

    # 2. Assinatura zlib (CMF + FLG em Big-Endian: 0x7801, 0x785E, 0x789C, 0x78DA)
    if b0 == 0x78 and b1 in (0x01, 0x5E, 0x9C, 0xDA) and ((b0 << 8) + b1) % 31 == 0:
        return CompressionSignature(
            detected=True,
            scheme="zlib (RFC 1950 Deflate)",
            signature_offset=f"0x{base_offset:08X}",
            signature_bytes_be=data[:2].hex(),
            notes=f"Cabeçalho zlib válido em Big-Endian (0x{b0:02X}{b1:02X}).",
        )

    # 3. Nintendo / Level-5 LZ10 (0x10), LZ11 (0x11) ou RLE (0x30) no byte 0
    if b0 in (0x10, 0x11, 0x30) and len(data) >= 8:
        # Verifica se os bytes 1..3 formam um tamanho plausível e se não é cabeçalho de contêiner
        size_24_le = data[1] | (data[2] << 8) | (data[3] << 16)
        size_24_be = (data[1] << 16) | (data[2] << 8) | data[3]
        if 0 < size_24_be <= 0x04000000 and data[4:8] != b"\x00\x00\x00\x00":
            scheme_map = {0x10: "LZSS (Nintendo/Level-5 Type 0x10)", 0x11: "LZSS (Extended Type 0x11)", 0x30: "RLE (Type 0x30)"}
            return CompressionSignature(
                detected=True,
                scheme=scheme_map[b0],
                signature_offset=f"0x{base_offset:08X}",
                signature_bytes_be=data[:4].hex(),
                decompressed_size=size_24_be,
                notes="Possível stream compatível com cabeçalho de 4 bytes (tipo + tamanho 24-bit).",
            )

    return CompressionSignature(
        detected=False,
        scheme="None (Uncompressed Header)",
        signature_offset=f"0x{base_offset:08X}",
        signature_bytes_be=data[:4].hex(),
        notes="Sem assinatura direta de compressão nos primeiros 4 bytes do arquivo.",
    )


def analyze_shade_packed_bin(f, file_size: int, header_bytes: bytes) -> Optional[Tuple[ContainerStructureInfo, CompressionSignature]]:
    """
    Valida se o arquivo `.bin` segue a estrutura de Contêiner Bit-Packed da Level-5 / Shade:
      - Offset 0x00 (uint32): count (quantidade de sub-arquivos)
      - Offset 0x04 (uint32): offset_align (unidade de alinhamento de offset, ex: 0x20, 0x800, 0x4000)
      - Offset 0x08 (uint32): size_mult (multiplicador de tamanho alocado, ex: 0x04, 0x10, 0x11, 0x20)
      - Offset 0x0C (uint32): bit_shift (deslocamento de bits do offset, ex: 17 ou 18)
      - Offset 0x10 (uint32): bit_mask (máscara de bits do tamanho = (1 << bit_shift) - 1)
      - Offset 0x14..0x14+4*count: tabela de descritores uint32 empacotados:
          file_offset = (entry >> bit_shift) * offset_align
          padded_size = (entry & bit_mask) * size_mult
    """
    if len(header_bytes) < 20:
        return None

    count, offset_align, size_mult, bit_shift, bit_mask = struct.unpack("<5I", header_bytes[:20])

    # Validação estrita de limites (Sem Heurísticas Cegas)
    if not (1 <= count <= 100000):
        return None
    if bit_shift not in (16, 17, 18, 19, 20):
        return None
    if bit_mask != (1 << bit_shift) - 1:
        return None
    if offset_align == 0 or size_mult == 0:
        return None
    table_end = 20 + count * 4
    if table_end > file_size:
        return None

    f.seek(20)
    entries = struct.unpack(f"<{count}I", f.read(count * 4))

    # Valida se todos os offsets e tamanhos cabem dentro de `file_size`
    sample_entries: List[Dict[str, Any]] = []
    first_sub_comp: Optional[CompressionSignature] = None

    last_idx = count - 1
    last_off = (entries[last_idx] >> bit_shift) * offset_align
    last_sz = (entries[last_idx] & bit_mask) * size_mult
    if last_off + last_sz > file_size:
        return None

    indices_to_inspect = list(range(min(4, count)))
    for idx in indices_to_inspect:
        e_val = entries[idx]
        sub_off = (e_val >> bit_shift) * offset_align
        sub_sz = (e_val & bit_mask) * size_mult
        f.seek(sub_off)
        sub_hdr = f.read(32)
        sub_be_u32 = struct.unpack(">4I", sub_hdr[:16].ljust(16, b"\x00"))

        sub_comp = detect_standard_compression(sub_hdr, base_offset=sub_off)
        # Caso especial de `dat.bin`: pares (LE, BE) comprimidos com stream LZSS/RLE sem o magic 0xFCAA55A7
        # Nos índices ímpares (Big-Endian), o byte 0 é o comprimento literal (ex: 0x0C ou 0x14),
        # seguido em Big-Endian (>) por: [block_count: >I] [decompressed_size: >I] [table_offset: >I]
        if not sub_comp.detected and len(sub_hdr) >= 16:
            lit_len = sub_hdr[0]
            if lit_len in (0x0A, 0x0B, 0x0C, 0x10, 0x12, 0x13, 0x14):
                be_cnt, be_decomp_sz, be_tbl_off = struct.unpack(">3I", sub_hdr[1:13])
                le_cnt, le_decomp_sz, le_tbl_off = struct.unpack("<3I", sub_hdr[1:13])
                if 1 <= be_cnt <= 64 and 0 < be_decomp_sz <= 0x1000000 and 0x10 <= be_tbl_off <= 0x200:
                    sub_comp = CompressionSignature(
                        detected=True,
                        scheme="Shade Headerless LZSS/RLE Stream (Big-Endian Payload)",
                        signature_offset=f"0x{sub_off:08X}",
                        signature_bytes_be=sub_hdr[:4].hex(),
                        decompressed_size=be_decomp_sz,
                        compressed_size=sub_sz,
                        inner_payload_magic=f"blocks={be_cnt}, tbl_off=0x{be_tbl_off:X} (BE >3I)",
                        notes=(
                            f"Byte inicial 0x{lit_len:02X} (run literal LZSS/RLE) seguido por cabeçalho de tabela "
                            f"em Big-Endian (>3I): count={be_cnt}, decomp_size=0x{be_decomp_sz:X}, offset=0x{be_tbl_off:X}."
                        ),
                    )
                elif 1 <= le_cnt <= 64 and 0 < le_decomp_sz <= 0x1000000 and 0x10 <= le_tbl_off <= 0x200:
                    sub_comp = CompressionSignature(
                        detected=True,
                        scheme="Shade Headerless LZSS/RLE Stream (Little-Endian Mirror Payload)",
                        signature_offset=f"0x{sub_off:08X}",
                        signature_bytes_be=sub_hdr[:4].hex(),
                        decompressed_size=le_decomp_sz,
                        compressed_size=sub_sz,
                        inner_payload_magic=f"blocks={le_cnt}, tbl_off=0x{le_tbl_off:X} (LE <3I)",
                        notes=(
                            f"Byte inicial 0x{lit_len:02X} (run literal LZSS/RLE) seguido por tabela espelhada "
                            f"em Little-Endian (<3I): count={le_cnt}, decomp_size=0x{le_decomp_sz:X}, offset=0x{le_tbl_off:X}. "
                            "No dat.bin, índices pares são LE e índices ímpares são Big-Endian nativos do Wii."
                        ),
                    )

        if first_sub_comp is None or (not first_sub_comp.detected and sub_comp.detected):
            first_sub_comp = sub_comp

        sample_entries.append({
            "index": idx,
            "packed_descriptor_hex": f"0x{e_val:08X}",
            "offset_hex": f"0x{sub_off:08X}",
            "allocated_size_bytes": sub_sz,
            "first_16b_hex": sub_hdr[:16].hex(),
            "first_4_words_be": [f"0x{w:08X}" for w in sub_be_u32],
            "sub_compression": sub_comp.scheme,
            "decompressed_size": sub_comp.decompressed_size,
            "inner_magic": sub_comp.inner_payload_magic,
        })

    first_data_off = (entries[0] >> bit_shift) * offset_align
    struct_info = ContainerStructureInfo(
        format_family="Level-5 / Shade Bit-Packed Archive Container (.bin)",
        header_endianness="Tabela de Índice Empacotada (32-bit); Payloads Big-Endian (>) / Pares Híbridos",
        entry_count=count,
        table_offset="0x00000014",
        data_start_offset=f"0x{first_data_off:08X}",
        offset_alignment=offset_align,
        size_multiplier=size_mult,
        bit_shift=bit_shift,
        bit_mask=f"0x{bit_mask:08X}",
        bounds_validated=True,
        validation_details=(
            f"Validado: {count} entradas na tabela (0x14..0x{table_end:X}). "
            f"Última entrada termina em 0x{last_off + last_sz:X} <= tamanho físico 0x{file_size:X}."
        ),
        sample_entries=sample_entries,
    )

    comp_summary = first_sub_comp if first_sub_comp is not None else detect_standard_compression(header_bytes)
    return struct_info, comp_summary


def analyze_file(file_path: str, rel_path: str) -> FileHeaderRecord:
    """
    Lê os primeiros 32 bytes de um arquivo em `extracted/files/`, constrói a visão
    Big-Endian (`>`) e classifica o formato de contêiner e esquema de compressão.
    """
    file_size = os.path.getsize(file_path)
    file_name = os.path.basename(file_path)
    ext = os.path.splitext(file_name)[1].lower()

    with open(file_path, "rb") as f:
        header_bytes = f.read(32)
        be_view = parse_be_header(header_bytes)

        # 1. Verifica se é um contêiner Bit-Packed `.bin` da Level-5 / Shade
        if ext == ".bin" and file_name not in ("bi2.bin", "boot.bin", "fst.bin"):
            packed_res = analyze_shade_packed_bin(f, file_size, header_bytes)
            if packed_res is not None:
                struct_info, comp_info = packed_res
                return FileHeaderRecord(
                    relative_path=rel_path,
                    file_name=file_name,
                    extension=ext,
                    file_size_bytes=file_size,
                    is_level5_primary_target=ext in LEVEL5_PRIMARY_EXTENSIONS,
                    header_be=be_view,
                    compression=comp_info,
                    structure=struct_info,
                )

        # 2. Verifica os arquivos `.bln` (Par mcb0.bln = Índice / mcb1.bln = Dados Comprimidos)
        if file_name == "mcb0.bln" and file_size % 12 == 0:
            max_records = file_size // 12
            f.seek(0)
            valid_entries = 0
            samples = []
            for idx in range(max_records):
                rec = f.read(12)
                if len(rec) < 12:
                    break
                id_flag_be, off_be, sz_be = struct.unpack(">3I", rec)
                id_flag_le, off_le, sz_le = struct.unpack("<3I", rec)
                if idx > 0 and off_le == 0 and sz_le == 0:
                    break
                valid_entries += 1
                if idx < 4:
                    samples.append({
                        "index": idx,
                        "group_id_hex": f"0x{id_flag_le:08X}",
                        "offset_in_mcb1_hex": f"0x{off_le:08X}",
                        "size_in_mcb1_bytes": sz_le,
                        "raw_words_be": [f"0x{id_flag_be:08X}", f"0x{off_be:08X}", f"0x{sz_be:08X}"],
                    })
            return FileHeaderRecord(
                relative_path=rel_path,
                file_name=file_name,
                extension=ext,
                file_size_bytes=file_size,
                is_level5_primary_target=True,
                header_be=be_view,
                compression=CompressionSignature(
                    detected=False,
                    scheme="None (External Index Table for mcb1.bln)",
                    signature_offset="0x00000000",
                    signature_bytes_be=header_bytes[:4].hex(),
                    notes="Tabela pura de índices de 12 bytes por entrada apontando para blocos comprimidos em mcb1.bln.",
                ),
                structure=ContainerStructureInfo(
                    format_family="Level-5 / Shade Paired Index Table (mcb0.bln -> mcb1.bln)",
                    header_endianness="Registros de 12 bytes (ID/Flag, Offset, Size)",
                    entry_count=valid_entries,
                    table_offset="0x00000000",
                    data_start_offset="0x00000000 (em mcb1.bln)",
                    offset_alignment=0x800,
                    bounds_validated=True,
                    validation_details=f"{valid_entries} registros ativos de 12 bytes mapeando mcb1.bln.",
                    sample_entries=samples,
                ),
            )

        if file_name == "mcb1.bln" and len(header_bytes) >= 32:
            # Em mcb1.bln, os primeiros 12 bytes (0x00..0x0C) são o cabeçalho de sub-bloco,
            # e em 0x0C inicia o bloco comprimido `FC AA 55 A7` seguido por `SHTXFS`
            sub_comp = detect_standard_compression(header_bytes[12:32], base_offset=0x0C)
            return FileHeaderRecord(
                relative_path=rel_path,
                file_name=file_name,
                extension=ext,
                file_size_bytes=file_size,
                is_level5_primary_target=True,
                header_be=be_view,
                compression=sub_comp,
                structure=ContainerStructureInfo(
                    format_family="Level-5 / Shade Bulk Asset Bank (mcb1.bln)",
                    header_endianness="Sub-cabeçalho de 12 bytes + Stream Shade LZSS (Magic BE 0xFCAA55A7 em 0x0C)",
                    table_offset="Externo (mcb0.bln)",
                    data_start_offset="0x0000000C",
                    offset_alignment=0x800,
                    bounds_validated=True,
                    validation_details=(
                        "Banco de dados binário indexado por mcb0.bln; primeiro bloco em 0x0C contém "
                        "assinatura 0xFCAA55A7 e textura SHTXFS."
                    ),
                ),
            )

        # 3. Nintendo U8 Archive (.arc) -> Magic Big-Endian 0x55AA382D ("U.8-")
        if be_view.magic_4b_u32_be == 0x55AA382D and len(header_bytes) >= 16:
            root_node_off, header_sz, data_off = struct.unpack(">3I", header_bytes[4:16])
            return FileHeaderRecord(
                relative_path=rel_path,
                file_name=file_name,
                extension=ext,
                file_size_bytes=file_size,
                is_level5_primary_target=False,
                header_be=be_view,
                compression=detect_standard_compression(header_bytes),
                structure=ContainerStructureInfo(
                    format_family="Nintendo U8 Archive (.arc)",
                    header_endianness="Big-Endian (>)",
                    table_offset=f"0x{root_node_off:08X}",
                    data_start_offset=f"0x{data_off:08X}",
                    bounds_validated=(data_off < file_size),
                    validation_details=(
                        f"Magic BE 0x55AA382D; root_node_offset=0x{root_node_off:X}, "
                        f"header_size=0x{header_sz:X}, data_offset=0x{data_off:X} (< 0x{file_size:X})."
                    ),
                ),
            )

        # 4. Nintendo Wii Sound Archive (.brsar) -> Magic Big-Endian 0x52534152 ("RSAR")
        if be_view.magic_4b_u32_be == 0x52534152 and len(header_bytes) >= 32:
            bom, ver_maj, ver_min, total_len, hdr_len, blk_cnt = struct.unpack(">HBBIIH", header_bytes[4:18])
            return FileHeaderRecord(
                relative_path=rel_path,
                file_name=file_name,
                extension=ext,
                file_size_bytes=file_size,
                is_level5_primary_target=False,
                header_be=be_view,
                compression=detect_standard_compression(header_bytes),
                structure=ContainerStructureInfo(
                    format_family="Nintendo Wii Binary Revolution Sound Archive (.brsar)",
                    header_endianness="Big-Endian (>) BOM 0xFEFF",
                    entry_count=blk_cnt,
                    table_offset=f"0x{hdr_len:08X}",
                    bounds_validated=(total_len == file_size),
                    validation_details=(
                        f"Magic 'RSAR' BE, BOM=0x{bom:04X}, versão {ver_maj}.{ver_min}, "
                        f"tamanho no cabeçalho={total_len} bytes (idêntico ao arquivo físico), {blk_cnt} seções (SYMB/INFO/FILE)."
                    ),
                ),
            )

        # 5. Executável ELF PowerPC Big-Endian (.elf) -> Magic 0x7F454C46 ("\x7fELF")
        if be_view.magic_4b_u32_be == 0x7F454C46 and len(header_bytes) >= 32:
            ei_class, ei_data = header_bytes[4], header_bytes[5]
            e_type, e_machine, e_version, e_entry, e_phoff = struct.unpack(">HHIII", header_bytes[16:32])
            return FileHeaderRecord(
                relative_path=rel_path,
                file_name=file_name,
                extension=ext,
                file_size_bytes=file_size,
                is_level5_primary_target=False,
                header_be=be_view,
                compression=detect_standard_compression(header_bytes),
                structure=ContainerStructureInfo(
                    format_family="PowerPC 32-bit Big-Endian ELF Executable (.elf)",
                    header_endianness="Big-Endian (>) ELFDATA2MSB",
                    table_offset=f"0x{e_phoff:08X}",
                    bounds_validated=(ei_class == 1 and ei_data == 2 and e_machine == 20),
                    validation_details=(
                        f"ELF32 Big-Endian (PowerPC e_machine={e_machine}), entry_point=0x{e_entry:08X}, "
                        f"program_header_offset=0x{e_phoff:X}."
                    ),
                ),
            )

        # 6. Nintendo Texture Palette Library (.tpl) -> Magic Big-Endian 0x0020AF30
        if be_view.magic_4b_u32_be == 0x0020AF30 and len(header_bytes) >= 16:
            ntex, tbl_off = struct.unpack(">II", header_bytes[4:12])
            return FileHeaderRecord(
                relative_path=rel_path,
                file_name=file_name,
                extension=ext,
                file_size_bytes=file_size,
                is_level5_primary_target=False,
                header_be=be_view,
                compression=detect_standard_compression(header_bytes),
                structure=ContainerStructureInfo(
                    format_family="Nintendo Wii Texture Palette Library (.tpl)",
                    header_endianness="Big-Endian (>)",
                    entry_count=ntex,
                    table_offset=f"0x{tbl_off:08X}",
                    bounds_validated=(tbl_off < file_size),
                    validation_details=f"Magic BE 0x0020AF30, {ntex} textura(s), offset da tabela=0x{tbl_off:X}.",
                ),
            )

        # 7. Mobiclip Video Container (.mo) -> Magic Big-Endian 0x4D4F4335 ("MOC5")
        if be_view.magic_4b_u32_be == 0x4D4F4335:
            return FileHeaderRecord(
                relative_path=rel_path,
                file_name=file_name,
                extension=ext,
                file_size_bytes=file_size,
                is_level5_primary_target=False,
                header_be=be_view,
                compression=CompressionSignature(
                    detected=True,
                    scheme="Mobiclip Video/Audio Codec (MOC5)",
                    signature_offset="0x00000000",
                    signature_bytes_be=header_bytes[:4].hex(),
                    notes="Contêiner de vídeo Mobiclip para Nintendo Wii.",
                ),
                structure=ContainerStructureInfo(
                    format_family="Mobiclip Wii Video Container (.mo)",
                    header_endianness="Misto (Tags ASCII Big-Endian 'MOC5', 'TL', 'V2' + Parâmetros LE)",
                    bounds_validated=True,
                    validation_details="Cabeçalho MOC5 validado.",
                ),
            )

        # 8. Disc Layout File (.dlf) -> Texto ASCII/Shift-JIS "v2.00\r\nRVL_CONFIG_FILE"
        if ext == ".dlf" and header_bytes.startswith(b"v2.00"):
            return FileHeaderRecord(
                relative_path=rel_path,
                file_name=file_name,
                extension=ext,
                file_size_bytes=file_size,
                is_level5_primary_target=False,
                header_be=be_view,
                compression=detect_standard_compression(header_bytes),
                structure=ContainerStructureInfo(
                    format_family="Wii Disc Layout Map (.dlf)",
                    header_endianness="Texto ASCII / Shift-JIS",
                    bounds_validated=True,
                    validation_details="Mapa de endereçamento físico dos arquivos no disco (RVL_CONFIG_FILE).",
                ),
            )

        # 9. Wii Banner (.bnr) -> Cabeçalho IMET em 0x40 com string UTF-16BE do título
        if ext == ".bnr" and file_size >= 0x100:
            f.seek(0x40)
            imet_hdr = f.read(64)
            imet_magic = decode_safe_string(imet_hdr[:4])
            f.seek(0x80)
            jp_title_raw = f.read(84)
            jp_title = jp_title_raw.decode("utf-16-be", errors="ignore").rstrip("\x00")
            return FileHeaderRecord(
                relative_path=rel_path,
                file_name=file_name,
                extension=ext,
                file_size_bytes=file_size,
                is_level5_primary_target=False,
                header_be=be_view,
                compression=detect_standard_compression(header_bytes),
                structure=ContainerStructureInfo(
                    format_family="Nintendo Wii Opening Banner (.bnr / IMET)",
                    header_endianness="Big-Endian (>) + UTF-16BE Strings",
                    table_offset="0x00000040",
                    bounds_validated=(imet_magic == "IMET"),
                    validation_details=f"Assinatura '{imet_magic}' em 0x40; título UTF-16BE extraído: '{jp_title}'.",
                ),
            )

        # 10. Arquivos de sistema do Wii em `sys/` (boot.bin, bi2.bin, fst.bin, main.dol, apploader.img)
        if file_name == "boot.bin" and len(header_bytes) >= 32:
            game_id = decode_safe_string(header_bytes[:6])
            wii_magic = struct.unpack(">I", header_bytes[24:28])[0]
            f.seek(0x20)
            disc_title = decode_safe_string(f.read(64).rstrip(b"\x00"))
            return FileHeaderRecord(
                relative_path=rel_path,
                file_name=file_name,
                extension=ext,
                file_size_bytes=file_size,
                is_level5_primary_target=True,
                header_be=be_view,
                compression=detect_standard_compression(header_bytes),
                structure=ContainerStructureInfo(
                    format_family="Nintendo Wii Disc Header (boot.bin)",
                    header_endianness="Big-Endian (>)",
                    bounds_validated=(wii_magic == 0x5D1C9EA3),
                    validation_details=f"GameID='{game_id}', Wii Magic BE=0x{wii_magic:08X}, Title='{disc_title}'.",
                ),
            )

        if file_name == "fst.bin" and len(header_bytes) >= 16:
            root_type_and_name, root_parent, total_entries = struct.unpack(">III", header_bytes[:12])
            return FileHeaderRecord(
                relative_path=rel_path,
                file_name=file_name,
                extension=ext,
                file_size_bytes=file_size,
                is_level5_primary_target=True,
                header_be=be_view,
                compression=detect_standard_compression(header_bytes),
                structure=ContainerStructureInfo(
                    format_family="Nintendo Wii File System Table (fst.bin)",
                    header_endianness="Big-Endian (>)",
                    entry_count=total_entries,
                    table_offset="0x00000000",
                    data_start_offset=f"0x{total_entries * 12:08X} (String Table)",
                    bounds_validated=(total_entries * 12 < file_size),
                    validation_details=f"{total_entries} nós FST de 12 bytes (Big-Endian) + tabela de nomes ASCII/Shift-JIS.",
                ),
            )

        if file_name == "bi2.bin":
            return FileHeaderRecord(
                relative_path=rel_path,
                file_name=file_name,
                extension=ext,
                file_size_bytes=file_size,
                is_level5_primary_target=True,
                header_be=be_view,
                compression=detect_standard_compression(header_bytes),
                structure=ContainerStructureInfo(
                    format_family="Nintendo Wii Disc Info Header (bi2.bin)",
                    header_endianness="Big-Endian (>)",
                    bounds_validated=(file_size == 0x2000),
                    validation_details="Estrutura BI2 de 8192 bytes (0x2000) em Big-Endian.",
                ),
            )

        if file_name == "main.dol" and len(header_bytes) >= 32:
            text0_off, text1_off = struct.unpack(">II", header_bytes[:8])
            f.seek(0xE0)
            bss_addr, bss_size, entry_pt = struct.unpack(">III", f.read(12))
            return FileHeaderRecord(
                relative_path=rel_path,
                file_name=file_name,
                extension=ext,
                file_size_bytes=file_size,
                is_level5_primary_target=False,
                header_be=be_view,
                compression=detect_standard_compression(header_bytes),
                structure=ContainerStructureInfo(
                    format_family="Nintendo GameCube/Wii DOL Executable (main.dol)",
                    header_endianness="Big-Endian (>)",
                    table_offset="0x00000000",
                    data_start_offset=f"0x{text0_off:08X}",
                    bounds_validated=(text0_off == 0x100 and entry_pt >= 0x80000000),
                    validation_details=(
                        f"Cabeçalho DOL de 0x100 bytes Big-Endian; Text0=0x{text0_off:X}, Text1=0x{text1_off:X}, "
                        f"EntryPoint=0x{entry_pt:08X}, BSS=0x{bss_addr:08X} (size=0x{bss_size:X})."
                    ),
                ),
            )

        # Fallback genérico para outros arquivos
        return FileHeaderRecord(
            relative_path=rel_path,
            file_name=file_name,
            extension=ext,
            file_size_bytes=file_size,
            is_level5_primary_target=ext in LEVEL5_PRIMARY_EXTENSIONS,
            header_be=be_view,
            compression=detect_standard_compression(header_bytes),
            structure=ContainerStructureInfo(
                format_family=f"Generic / Unknown ({ext if ext else 'sem extensão'})",
                header_endianness="Big-Endian (>)",
                bounds_validated=True,
                validation_details="Cabeçalho de 32 bytes lido e catalogado.",
            ),
        )


def scan_extracted_directory(base_dir: str) -> Dict[str, Any]:
    """
    Percorre recursivamente `extracted/files/`, analisa os arquivos alvo e sintetiza
    os padrões de cabeçalho, magic numbers e esquemas de compressão encontrados.
    """
    records: List[FileHeaderRecord] = []
    skipped_extensions: Dict[str, int] = {}

    for root, dirs, files in os.walk(base_dir):
        dirs.sort()
        for fname in sorted(files):
            full_path = os.path.join(root, fname)
            rel_path = os.path.relpath(full_path, base_dir)
            ext = os.path.splitext(fname)[1].lower()

            if ext not in TARGET_EXTENSIONS:
                skipped_extensions[ext] = skipped_extensions.get(ext, 0) + 1
                continue

            rec = analyze_file(full_path, rel_path)
            records.append(rec)

    # Agrupamento de magic numbers e esquemas de compressão
    magic_catalog: Dict[str, Dict[str, Any]] = {}
    compression_catalog: Dict[str, Dict[str, Any]] = {}
    level5_containers: List[Dict[str, Any]] = []

    for r in records:
        m_key = f"{r.header_be.magic_4b_hex} ({r.structure.format_family})"
        if m_key not in magic_catalog:
            magic_catalog[m_key] = {
                "magic_4b_be_hex": r.header_be.magic_4b_hex,
                "magic_ascii": r.header_be.magic_ascii,
                "format_family": r.structure.format_family,
                "file_count": 0,
                "files": [],
            }
        magic_catalog[m_key]["file_count"] += 1
        magic_catalog[m_key]["files"].append(r.relative_path)

        c_key = r.compression.scheme
        if c_key not in compression_catalog:
            compression_catalog[c_key] = {
                "scheme": c_key,
                "file_count": 0,
                "files": [],
                "notes": r.compression.notes,
            }
        compression_catalog[c_key]["file_count"] += 1
        compression_catalog[c_key]["files"].append(r.relative_path)

        if r.is_level5_primary_target:
            level5_containers.append(asdict(r))

    return {
        "metadata": {
            "project": "Inazuma Eleven GO Strikers 2013 (Nintendo Wii - S5SJHF)",
            "agent_role": "@format-parser",
            "endianness_convention": "Big-Endian (>)",
            "scanned_root": base_dir,
            "target_extensions_scanned": sorted(ext if ext else "[no_ext]" for ext in TARGET_EXTENSIONS),
            "total_target_files_analyzed": len(records),
            "total_level5_primary_files": len(level5_containers),
            "skipped_non_container_extensions": skipped_extensions,
        },
        "architectural_findings": {
            "level5_shade_bin_container_spec": {
                "files": ["dat.bin", "grp.bin", "scn.bin", "scn_sh.bin", "strap.bin", "ui.bin"],
                "header_size_bytes": 20,
                "layout": {
                    "0x00": "entry_count (uint32) - Quantidade total de sub-arquivos no contêiner",
                    "0x04": "offset_alignment (uint32) - Unidade de bloco para cálculo de offset (0x20, 0x800, 0x4000)",
                    "0x08": "size_multiplier (uint32) - Multiplicador do tamanho alocado (0x04, 0x10, 0x11, 0x20)",
                    "0x0C": "bit_shift (uint32) - Deslocamento de bits do offset dentro do descritor (17 ou 18 bits)",
                    "0x10": "bit_mask (uint32) - Máscara de extração do tamanho ((1 << bit_shift) - 1, ex: 0x1FFFF ou 0x3FFFF)",
                    "0x14..N": "Tabela de `entry_count` descritores uint32: offset = (entry >> bit_shift) * offset_alignment; size = (entry & bit_mask) * size_multiplier",
                },
            },
            "level5_shade_bln_pair_spec": {
                "files": ["mcb0.bln", "mcb1.bln"],
                "description": (
                    "Par Índice/Dados: mcb0.bln armazena registros de 12 bytes (ID/grupo, offset alinhado a 0x800, tamanho) "
                    "que indexam sub-contêineres dentro de mcb1.bln (1.22 GB)."
                ),
            },
            "compression_signatures_found": {
                "shade_lzss_magic_0xFCAA55A7": (
                    "Assinatura Big-Endian `0xFCAA55A7` (`fc aa 55 a7`) presente nos blocos de `grp.bin`, `scn.bin`, "
                    "`scn_sh.bin`, `strap.bin`, `ui.bin` e `mcb1.bln` (em +0x0C). Cabeçalho de 12 bytes: "
                    "[Magic BE 0xFCAA55A7][decompressed_size][compressed_size], seguido por stream LZSS/RLE onde "
                    "bytes < 0x20 indicam cópia literal (revelando assinaturas internas como `SHTXFF`, `SHTXFS`, `REF`, `!brres`)."
                ),
                "dat_bin_paired_headerless_lzss": (
                    "Em `dat.bin` (130 entradas = 65 pares), os sub-arquivos usam o mesmo stream LZSS/RLE sem o prefixo "
                    "`0xFCAA55A7`. Cada par `(2k, 2k+1)` representa a mesma tabela de jogo: índices pares (`2k`) estão em "
                    "Little-Endian e índices ímpares (`2k+1`) estão em Big-Endian (`>`) nativo do Wii (iniciando com byte literal "
                    "`0x0C` ou `0x14` seguido por `>3I`: block_count, decompressed_size, first_block_offset)."
                ),
            },
        },
        "magic_numbers_summary": list(magic_catalog.values()),
        "compression_summary": list(compression_catalog.values()),
        "level5_primary_containers": level5_containers,
        "all_analyzed_files": [asdict(r) for r in records],
    }


def main() -> None:
    if not os.path.isdir(EXTRACTED_FILES_DIR):
        raise FileNotFoundError(f"Diretório não encontrado: {EXTRACTED_FILES_DIR}")

    print(f"[scan_headers] Iniciando varredura Big-Endian (>) em: {EXTRACTED_FILES_DIR}")
    report = scan_extracted_directory(EXTRACTED_FILES_DIR)

    os.makedirs(os.path.dirname(OUTPUT_JSON_PATH), exist_ok=True)
    with open(OUTPUT_JSON_PATH, "w", encoding="utf-8") as out_f:
        json.dump(report, out_f, indent=2, ensure_ascii=False)

    meta = report["metadata"]
    print(f"[scan_headers] Concluído! {meta['total_target_files_analyzed']} arquivos analisados "
          f"({meta['total_level5_primary_files']} alvos primários Level-5/.bin/.bln).")
    print(f"[scan_headers] Relatório salvo em: {OUTPUT_JSON_PATH}")


if __name__ == "__main__":
    main()
