# Especificação de Formato Binário: Modelos 3D (`MDL0` v11) e Texturas (`TEX0` v3)

Este documento descreve o layout binário completo dos modelos 3D, esqueletos, *Display Lists* de hardware Nintendo Wii GX e texturas swizzled extraídos de *Inazuma Eleven GO Strikers 2013* (`RVL-R5GJ-JPN`, Engine Shade / Level-5) a partir dos contêineres `extracted/files/grp.bin` e `extracted/files/dat.bin` (`idx = 75` / `0x9C8B`).

---

## 1. Arquitetura de Contêineres e Vinculação (`dat.bin` $\rightarrow$ `grp.bin`)

### 1.1. Contêiner Gráfico `grp.bin`
O arquivo `extracted/files/grp.bin` armazena **4.973 pacotes gráficos** (arquivos `BRRES` contendo malhas 3D, texturas e animações) comprimidos individualmente com o algoritmo **Shade LZSS/RLE** (`0xFCAA55A7`).

| Offset | Tipo | Endianness | Campo | Valor em `grp.bin` | Descrição |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `+0x00` | `uint32` | Little (`<I`) | `entry_count` | `4973` (`0x136D`) | Total de sub-arquivos empacotados |
| `+0x04` | `uint32` | Little (`<I`) | `offset_align` | `16384` (`0x4000`) | Alinhamento de setor em bytes |
| `+0x08` | `uint32` | Little (`<I`) | `size_mult` | `17` (`0x11`) | Multiplicador de tamanho |
| `+0x0C` | `uint32` | Little (`<I`) | `bit_shift` | `17` (`0x11`) | Deslocamento de bits do índice de setor |
| `+0x10` | `uint32` | Little (`<I`) | `bit_mask` | `0x1FFFF` | Máscara de bits de tamanho |
| `+0x14` | `uint32[4973]` | Little (`<I`) | `descriptors` | Array de 4.973 entradas | `byte_offset = (desc >> 17) * 0x4000` |

Cada bloco apontado por `byte_offset` inicia com o cabeçalho de compressão **Shade LZSS** de 12 bytes:
- `+0x00` (`4s`): Magic number `FC AA 55 A7`
- `+0x04` (`<I`): `decompressed_size` (tamanho exato do arquivo `BRRES` descomprimido)
- `+0x08` (`<I`): `compressed_size` (tamanho total do bloco comprimido incluindo cabeçalho)
- `+0x0C..`: Fluxo de comandos LZSS/RLE decodificado por `shade_decompress` (engenharia reversa de `0x80333C5C` em `main.dol`).

> [!IMPORTANT]
> **Indexação 1-Based (`file_id - 1`)**: Conforme comprovado pelo disassembly da função `MDLRegist__Fi` (`0x80025004`, instrução `addi r3, r3, -1` em `0x800251C0`), todos os IDs de arquivos de `grp.bin` armazenados na tabela `idx = 75` são **1-based**:
> $$\text{grp\_index} = \text{file\_id} - 1$$

---

### 1.2. Tabela de Definição de Modelos e Conjuntos de Texturas (`dat.bin`, `idx = 75` / `0x9C8B`)
Carregada na inicialização por `onm_load__Fv` (`0x80029C24`), a tabela `idx = 75` possui duas seções principais em **Big-Endian (`>`)**:

#### Seção 0 (`off = 0x64`, `count = 4133`, `stride = 0x5C` / 92 bytes): `ModelDefEntry`
| Offset | Tipo | Campo | Descrição |
| :--- | :--- | :--- | :--- |
| `+0x00` | `uint32` (`>I`) | `mdl_file_id` | ID 1-based do arquivo `BRRES` do modelo em `grp.bin` (`grp[mdl_file_id - 1]`) |
| `+0x04` | `uint32` (`>I`) | `tex_set_index` | Índice 0-based na Seção 1 (`TexSetDefEntry`) |
| `+0x08` | `uint32` (`>I`) | `anim_or_sub_id` | Índice auxiliar de animação/sombra |
| `+0x0C` | `int16[4]` (`>4h`) | `flags_params` | Flags de renderização e LOD |
| `+0x14` | `float32` (`>f`) | `bound_radius` | Raio do cilindro de colisão/visibilidade |
| `+0x18` | `float32` (`>f`) | `bound_height` | Altura do cilindro de colisão/visibilidade |
| `+0x1C` | `float32[3]` (`>3f`) | `bound_offset` | Deslocamento `(X, Y, Z)` do volume de colisão |
| `+0x28` | `float32` (`>f`) | `scale` | Escala global do modelo (`1.0` padrão) |
| `+0x2C..+0x5B` | `uint32[12]` | `reserved` | Parâmetros adicionais de shader/efeitos |

#### Seção 1 (`off = 0x5D184`, `count = 1706`, `stride = 0x28` / 40 bytes): `TexSetDefEntry`
| Offset | Tipo | Campo | Descrição |
| :--- | :--- | :--- | :--- |
| `+0x00..+0x0F` | `uint32[4]` (`>4I`) | `header_flags` | Metadados do conjunto de texturas |
| `+0x10..+0x27` | `uint32[6]` (`>6I`) | `tex_file_ids` | Até 6 IDs 1-based em `grp.bin` (`grp[id - 1]`) contendo os arquivos `BRRES` de texturas (`TEX0`) |

#### Vinculação com a Tabela de Personagens (`idx = 15` / `0x9C4F`)
Na função `CHRRegist__Fii` (`0x80024CF8`) e `GetTeamDefToBody` (`0x800BDD04`):
- O campo `+0x64` (`model_grp_id_1`) do registro do personagem em `idx = 15` é o `model_def_id` da **Cabeça** em `idx = 75` `sec[0]` (ex: Endo Mamoru `cid = 1` $\rightarrow$ `model_def_id = 2481` $\rightarrow$ `mdl_file_id = 3436` $\rightarrow$ `grp[3435]`).
- O uniforme do time seleciona o `model_def_id` do **Corpo** (ex: Goleiro Titular da Raimon $\rightarrow$ `model_def_id = 400` (`RAI_GKH_CHIBI_00`) $\rightarrow$ `mdl_file_id = 1140` $\rightarrow$ `grp[1139]`).
- A **Bola de Futebol** padrão utiliza `model_def_id = 3889` (`ball0000_normal` $\rightarrow$ `mdl_file_id = 1117` $\rightarrow$ `grp[1116]`, texturas em `tex_file_id = 15` $\rightarrow$ `grp[14]`).

---

## 2. Contêiner NintendoWare for Revolution `BRRES` (`bres`)

Cada bloco descomprimido de `grp.bin` é um arquivo `BRRES` em **Big-Endian (`>`)**:

| Offset | Tipo | Campo | Valor / Descrição |
| :--- | :--- | :--- | :--- |
| `+0x00` | `char[4]` | `magic` | `'bres'` (`0x62726573`) |
| `+0x04` | `uint16` (`>H`) | `bom` | `0xFEFF` (Big-Endian Byte Order Mark) |
| `+0x06` | `uint16` (`>H`) | `version` | Versão do contêiner |
| `+0x08` | `uint32` (`>I`) | `file_size` | Tamanho total do arquivo `BRRES` em bytes |
| `+0x0C` | `uint16` (`>H`) | `root_offset` | Offset da seção `root` (`0x0010`) |
| `+0x0E` | `uint16` (`>H`) | `num_sections` | Quantidade de seções no arquivo |

A seção `root` (`+0x10`) inicia com a assinatura `'root'` seguida por um dicionário `ResDic` (`+0x18`) que organiza subpastas como `3DModels(NW4R)` (contendo sub-arquivos `MDL0`) e `Textures(NW4R)` (contendo sub-arquivos `TEX0`).

---

## 3. Formato de Malha 3D `MDL0` (Versão 11)

### 3.1. Cabeçalho `MDL0` e Tabela de Seções
| Offset | Tipo | Campo | Descrição |
| :--- | :--- | :--- | :--- |
| `+0x00` | `char[4]` | `magic` | `'MDL0'` (`0x4D444C30`) |
| `+0x04` | `uint32` (`>I`) | `byte_size` | Tamanho total do bloco `MDL0` |
| `+0x08` | `uint32` (`>I`) | `version` | `11` (`0x0000000B`) |
| `+0x0C` | `int32` (`>i`) | `bres_offset` | Offset relativo para o início do `BRRES` |
| `+0x10..+0x47` | `int32[14]` (`>14i`) | `section_offsets` | Offsets relativos ao início do `MDL0` para os 14 dicionários `ResDic` |
| `+0x48` | `int32` (`>i`) | `name_offset` | Offset relativo para o nome do modelo |
| `+0x4C..+0x8B` | `MDL0Header` | `info` | Metadados globais (`num_vertices` em `+0x5C`, `num_polygons` em `+0x60`, offset da tabela `MtxId -> NodeId` em `+0x70`) |

#### Índice das 14 Seções (`section_offsets[0..13]`)
- **Seção 0 (`ByteCode`)**: Fluxos de instruções `NodeTree`, `NodeMix`, `DrawOpa` e `DrawXlu`.
- **Seção 1 (`Bones`)**: Árvore de ossos (`ResBoneData`).
- **Seção 2 (`VtxPos`)**: Buffers de coordenadas de vértices `(X, Y, Z)` (`ResVtxPosData`).
- **Seção 3 (`VtxNrm`)**: Buffers de vetores normais `(NX, NY, NZ)` (`ResVtxNrmData`).
- **Seção 4 (`VtxClr`)**: Buffers de cores de vértices (`ResVtxClrData`).
- **Seção 5 (`VtxTexCoord`)**: Buffers de coordenadas UV `(S, T)` (`ResVtxTexCoordData`).
- **Seção 8 (`Materials`)**: Definições de materiais e camadas de texturas (`ResMatData`).
- **Seção 9 (`Shaders`)**: Configuração de estágios TEV (`ResTevData`).
- **Seção 10 (`Shapes`)**: Polígonos e *Display Lists* GX (`ResShpData`).
- **Seção 11 (`TexPlttInfo`)**: Vínculos diretos entre nomes de texturas e materiais.

---

### 3.2. Seção 1: Ossos e Matrizes de Transformação (`ResBoneData`, `0xD0` bytes)
Cada osso possui `0xD0` (208) bytes em Big-Endian:

| Offset | Tipo | Campo | Descrição |
| :--- | :--- | :--- | :--- |
| `+0x00` | `uint32` (`>I`) | `length` | `0xD0` (208 bytes) |
| `+0x08` | `int32` (`>i`) | `name_offset` | Offset relativo para o nome ASCII do osso (ex: `head`, `waist`, `arm1L`) |
| `+0x0C` | `uint32` (`>I`) | `node_id` | Índice do osso na hierarquia (`0..N-1`) |
| `+0x10` | `uint32` (`>I`) | `mtx_id` | ID da matriz associada a este osso |
| `+0x14` | `uint32` (`>I`) | `flags` | Flags de transformação e visibilidade |
| `+0x20` | `float32[3]` (`>3f`) | `scale` | Escala local `(sx, sy, sz)` |
| `+0x2C` | `float32[3]` (`>3f`) | `rotation` | Rotação local em graus Euler `(rx, ry, rz)` |
| `+0x38` | `float32[3]` (`>3f`) | `translation` | Translação local `(tx, ty, tz)` |
| `+0x70` | `float32[12]` (`>12f`) | `world_matrix` | Matriz afim $3 \times 4$ na pose padrão (*Bind Pose* em espaço do modelo) |
| `+0xA0` | `float32[12]` (`>12f`) | `inv_bind_matrix` | Matriz afim $3 \times 4$ inversa da pose padrão (*Inverse Bind Matrix*) |

#### Regra de Espaço de Coordenadas (Rígido vs. *Weighted Skinning*)
A tabela `mtx_to_node` (localizada em `mdl0_offset + 0x4C + info.node_table_offset`) mapeia cada `mtx_id` para um `node_id`:
1. **`mtx_to_node[mtx_id] >= 0` (Vinculação Rígida a 1 Osso)**: Os vértices que referenciam este `mtx_id` estão armazenados no **espaço local do osso** e devem ser multiplicados pela matriz `bone.world_matrix` (`+0x70`).
2. **`mtx_to_node[mtx_id] == -1` (Envelope Ponderado em `NodeMix` op `0x03`)**: O bytecode `NodeMix` calcula `env_mtx = world_matrix * inv_bind_matrix` via opcode `0x05` (que na *bind pose* resulta na matriz **Identidade**) e combina os pesos via opcode `0x03`. Portanto, os vértices ponderados já estão armazenados diretamente no **espaço global do modelo** na *bind pose*.

---

### 3.3. Seções 2, 3 e 5: Arrays de Vértices, Normais e UVs (`ResVtx*Data`)
Todas as três seções compartilham o mesmo cabeçalho de 32 bytes:

| Offset | Tipo | Campo | Descrição |
| :--- | :--- | :--- | :--- |
| `+0x00` | `uint32` (`>I`) | `byte_length` | Tamanho total da estrutura |
| `+0x08` | `int32` (`>i`) | `data_offset` | Offset relativo para o início do buffer de dados brutos (`0x20` ou `0x40`) |
| `+0x10` | `uint32` (`>I`) | `array_index` | Índice do buffer referenciado por `ResShpData` |
| `+0x14` | `uint32` (`>I`) | `comp_cnt` | Componentes (`Pos`: `1`=XYZ; `Nrm`: `0`=XYZ; `UV`: `1`=ST) |
| `+0x18` | `uint32` (`>I`) | `comp_type` | `GXCompType`: `0`=`u8`, `1`=`s8`, `2`=`u16`, `3`=`s16`, `4`=`f32` |
| `+0x1C` | `uint8` (`>B`) | `divisor` | Expoente de ponto fixo: $\text{valor\_real} = \text{valor\_bruto} \times 2^{-\text{divisor}}$ |
| `+0x1D` | `uint8` (`>B`) | `stride` | Tamanho em bytes de cada elemento (ex: `6` para `3h`, `4` para `2H`) |
| `+0x1E` | `uint16` (`>H`) | `num_elements` | Quantidade de elementos no array |

---

### 3.4. Seção 10: Polígonos e *Display Lists* do Wii GX (`ResShpData`)
Cada `ResShpData` descreve uma sub-malha e encapsula os registradores do *Command Processor* (CP) e a *Display List* binária do hardware gráfico **Nintendo Wii GX**:

| Offset | Tipo | Campo | Descrição |
| :--- | :--- | :--- | :--- |
| `+0x08` | `int32` (`>i`) | `mtx_id` | `-1` se usa `PMIDX` por vértice; $\ge 0$ se vinculado a um único `mtx_id` |
| `+0x0C` | `uint32` (`>I`) | `cp_vcd_lo` | Registrador `GX_CP_VCD_LO`: bit `0` = `PMIDX` (1B), bits `1..8` = `T0..7MIDX` (1B cada), bits `9..10` = `POS` (`2`=`u8`, `3`=`u16`), bits `11..12` = `NRM` (`2`=`u8`, `3`=`u16`), bits `13..16` = `COL0..1` |
| `+0x10` | `uint32` (`>I`) | `cp_vcd_hi` | Registrador `GX_CP_VCD_HI`: 2 bits por canal UV `TEX0..TEX7` (`0`=ausente, `2`=índice `u8`, `3`=índice `u16`) |
| `+0x18` | `uint32, uint32, int32` | `dl_setup` | `(buf_size, cmd_size, rel_offset)` da Display List de configuração CP/VAT |
| `+0x24` | `uint32, uint32, int32` | `dl_primitives` | `(buf_size, cmd_size, rel_offset)` da Display List de primitivas geométricas |
| `+0x3C` | `uint32` (`>I`) | `shape_index` | Índice do polígono referenciado por `DrawOpa` |
| `+0x40` | `uint32` (`>I`) | `num_vertices` | Total de vértices emitidos na Display List |
| `+0x44` | `uint32` (`>I`) | `num_polygons` | Total de triângulos da sub-malha |
| `+0x48` | `int16` (`>h`) | `vtx_pos_idx` | Índice na Seção 2 (`VtxPos`) |
| `+0x4A` | `int16` (`>h`) | `vtx_nrm_idx` | Índice na Seção 3 (`VtxNrm`) |
| `+0x50` | `int16[8]` (`>8h`) | `vtx_uv_idxs` | Índices na Seção 5 (`VtxTexCoord`) para `TEX0..TEX7` (`-1` se não utilizado) |

#### Opcodes da *Display List* de Primitivas (`dl_primitives`)
- `0x00` (`GX_NOP`, 1 byte): Padding de alinhamento (32 bytes).
- `0x20` (`GX_LOAD_INDX_A`, 5 bytes `>BHH`): Carrega a matriz `mtx_id` na memória XF de posição (`slot = (xf_addr & 0xFFF) // 12`), associando `PMIDX = slot * 3` $\rightarrow$ `mtx_id`.
- `0x28`, `0x30`, `0x38` (`GX_LOAD_INDX_B/C/D`, 5 bytes): Carregam matrizes normais/texturas nos respectivos barramentos XF.
- `0x90` (`GX_TRIANGLES`), `0x98` (`GX_TRIANGLESTRIP`), `0xA0` (`GX_TRIANGLEFAN`), `0x80` (`GX_QUADS`): Seguidos por `uint16` (`>H`) `vertex_count` e `vertex_count` registros cujo layout de bytes é determinado exatamente por `cp_vcd_lo` e `cp_vcd_hi`.

---

## 4. Formato de Texturas `TEX0` (Versão 3) e Codificação Wii GX

### 4.1. Cabeçalho `TEX0` (`0x40` bytes)
| Offset | Tipo | Campo | Descrição |
| :--- | :--- | :--- | :--- |
| `+0x00` | `char[4]` | `magic` | `'TEX0'` (`0x54455830`) |
| `+0x04` | `uint32` (`>I`) | `byte_size` | Tamanho total do recurso `TEX0` |
| `+0x08` | `uint32` (`>I`) | `version` | `3` |
| `+0x10` | `int32` (`>i`) | `data_offset` | Offset relativo para o buffer de blocos de pixels (`0x40`) |
| `+0x14` | `int32` (`>i`) | `name_offset` | Offset relativo para o nome da textura |
| `+0x1C` | `uint16` (`>H`) | `width` | Largura em pixels |
| `+0x1E` | `uint16` (`>H`) | `height` | Altura em pixels |
| `+0x20` | `uint32` (`>I`) | `format` | Código `GXTexFmt` |
| `+0x24` | `uint32` (`>I`) | `mipmap_count` | Quantidade de níveis de mipmap |

### 4.2. Formatos de Pixels Swizzled (`GXTexFmt`) Mapeados
Todas as texturas do Wii armazenam pixels em blocos (*tiles*) de 32 ou 64 bytes em ordem **Big-Endian**:

| Código `GXTexFmt` | Nome | Dimensão do Bloco | Bits/Pixel | Descrição da Codificação |
| :--- | :--- | :--- | :--- | :--- |
| `0x0` | `I4` | $8 \times 8$ | 4 bpp | Intensidade monocromática de 4 bits (`I = nibble * 17`, `A = 255`) |
| `0x1` | `I8` | $8 \times 4$ | 8 bpp | Intensidade monocromática de 8 bits (`R = G = B = byte`, `A = 255`) — usado em `_falloff` |
| `0x2` | `IA4` | $8 \times 4$ | 8 bpp | `Alpha` nos 4 bits superiores (`(b >> 4) * 17`), `Intensity` nos 4 bits inferiores (`(b & 0xF) * 17`) — usado em `symb_00` e números `numb_001..099` |
| `0x3` | `IA8` | $4 \times 4$ | 16 bpp | 1 byte `Alpha` + 1 byte `Intensity` |
| `0x4` | `RGB565` | $4 \times 4$ | 16 bpp | `uint16` (`>H`): `R5` (bits 15..11), `G6` (bits 10..5), `B5` (bits 4..0) — usado em `BALL01` e `body_00` |
| `0x5` | `RGB5A3` | $4 \times 4$ | 16 bpp | `uint16` (`>H`): se bit 15 = `1`, `RGB555` opaco (`A = 255`); se bit 15 = `0`, `A3 R4 G4 B4` translúcido — usado em `face_0001` e `BALL_edge` |
| `0x6` | `RGBA8` | $4 \times 4$ | 32 bpp | 64 bytes por bloco: 32 bytes de pares `(A, R)` seguidos de 32 bytes de pares `(G, B)` |
| `0xE` | `CMPR` | $8 \times 8$ ($4 \times 4$ sub) | 4 bpp | Compressão DXT1 nativa do Wii: cada macro-bloco $8 \times 8$ contém 4 sub-blocos $4 \times 4$ de 8 bytes (`c0: >H`, `c1: >H`, `indices: >I` com bits `31..30` para o primeiro pixel) — usado em `body_01`, `foot_00`, `sock_00`, `hada_00`, `mark_00`, `shading` |

---

## 5. Resumo dos Assets Extraídos (`output/models/`)

| Asset | `idx=75` ID | `grp.bin` (Modelo) | `grp.bin` (Texturas) | Ossos | Vértices Exportados | Triângulos | Arquivo Exportado |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Bola (`ball0000_normal`)** | `3889` | `grp[1116]` (`id=1117`) | `grp[14]` (`id=15`) | 3 | 540 | 180 | `output/models/ball.obj` |
| **Endo Mamoru — Cabeça (`0001_endo`)** | `2481` | `grp[3435]` (`id=3436`) | `grp[4327]`, `grp[3942]` | 1 | 2.496 | 832 | `output/models/endo_head.obj` |
| **Endo Mamoru — Corpo GK Raimon (`RAI_GKH_CHIBI_00`)** | `400` | `grp[1139]` (`id=1140`) | `grp[71]`, `grp[65]`, `grp[3826]` | 23 | 9.054 | 3.018 | `output/models/endo_body.obj` |
| **Endo Mamoru — Completo (Corpo + Cabeça no osso `head`)** | `400 + 2481` | `grp[1139] + grp[3435]` | Conjunto completo | 24 | 11.550 | 3.850 | `output/models/endo_mamoru.obj` |
