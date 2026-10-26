# Especificação Formal das Fórmulas de Confronto (`BATTLE_FORMULAS.md`)

**Alvo Analisado:** `extracted/files/sys/main.dol` (PowerPC 750CL / Broadway 32-bit Big-Endian) & `extracted/files/dat.bin`  
**Jogo:** *Inazuma Eleven GO Strikers 2013* (`RVL-R5GJ-JPN`, Engine Shade / Level-5)  
**Agente Responsável:** `@static-analyst`  
** Artefato de Símbolos Descompilados:** [`output/symbols/battle_routines_decompiled.json`](file:///home/jansey/strikers-ai-core/output/symbols/battle_routines_decompiled.json)

---

## 1. Mapa de Memória, Estruturas Runtime e Tabelas de Balanceamento

### 1.1. Mapeamento de Seções de `main.dol` e Registradores Globais (EABI)

| Seção DOL | Offset no Arquivo (`main.dol`) | Endereço Virtual (RAM) | Tamanho | Descrição |
| :--- | :--- | :--- | :--- | :--- |
| `.text0` | `0x000100 .. 0x002820` | `0x80004000 .. 0x80006720` | `0x002720` | Inicialização de hardware/registradores (`__init_registers`) |
| `.text1` | `0x002820 .. 0x461DA0` | `0x80020B00 .. 0x80480080` | `0x45F580` | Código principal do jogo (`vaddr = file_offset + 0x8001E2E0`) |
| `.rodata` (`data4`/`data5`) | `0x47C280 .. 0x508500` | `0x80480180 .. 0x8050C400` | `0x08C280` | Constantes, tabelas de jump e strings Shift-JIS (`matchmgr.cpp`, `action.cpp`) |
| `.sdata` (`data6`) | `0x508500 .. 0x509760` | `0x8050C400 .. 0x8050D660` | `0x001260` | Small Data (`r13 = _SDA_BASE_ = 0x80514400`, init em `0x80004294`) |
| `.sdata2` (`data7`) | `0x509760 .. 0x50DCA0` | `0x8050E7E0 .. 0x80512D20` | `0x004540` | Small Const Data (`r2 = _SDA2_BASE_ = 0x805167E0`, init em `0x8000428C`) |

---

### 1.2. Mapeamento Exato dos 6 Atributos (`dat.bin` $\to$ Roster $\to$ Match Stat Struct)

A inspeção cruzada da rotina de cópia de elenco (`0x800B9B08`, file `+0x09B828`), do construtor de atributos de partida (`FUN_80092708` @ `0x80092708`, file `+0x074428`) e do HUD de depuração nativo da Shade (`FUN_800999b0` @ `0x800999B0`, file `+0x07B6D0`, com rótulos Shift-JIS em `0x80499906..0x8049996F` de `matchmgr.cpp`) estabelece de forma definitiva a ordem dos **6 atributos primários**:

| Índice (`stat_idx`) | Rótulo Debug (`matchmgr.cpp`) | Atributo | Offset em `idx=9` (`sec[0]` / `0x80530E74`) | Offset no Roster (`0xB0` bytes, `0x800BBF0C`) | Offset no Match Stat (`0x4C` bytes, `0x807AC5A4`) | Deslocamento de Base (`0x807B2408`) |
| :---: | :--- | :--- | :---: | :---: | :---: | :---: |
| **`0`** | `#Q2#W#=キック　 = %d` (`0x80499906`) | **Kick** (`キック`) | `+0x0A` (base) / `+0x0B` (max) | `+0x70` (`uint8`) | **`+0x00`** (`int32`) | `-0x5E64` (`0xA19C`) |
| **`1`** | `#Q2#W#=キャッチ = %d` (`0x8049991B`) | **Catch** (`キャッチ`) | `+0x0C` (base) / `+0x0D` (max) | `+0x71` (`uint8`) | **`+0x04`** (`int32`) | `-0x5E60` (`0xA1A0`) |
| **`2`** | `#Q2#W#=ボディ　 = %d` (`0x80499930`) | **Body** (`ボディ`) | `+0x0E` (base) / `+0x0F` (max) | `+0x72` (`uint8`) | **`+0x08`** (`int32`) | `-0x5E5C` (`0xA1A4`) |
| **`3`** | `#Q2#W#=ガード　 = %d` (`0x80499945`) | **Guard** (`ガード`) | `+0x10` (base) / `+0x11` (max) | `+0x73` (`uint8`) | **`+0x0C`** (`int32`) | `-0x5E58` (`0xA1A8`) |
| **`4`** | `#Q2#W#=コントロ = %d` (`0x8049995A`) | **Control** (`コントロール`) | `+0x12` (base) / `+0x13` (max) | `+0x74` (`uint8`) | **`+0x10`** (`int32`) | `-0x5E54` (`0xA1AC`) |
| **`5`** | `#Q2#W#=スピード = %d` (`0x8049996F`) | **Speed** (`スピード`) | `+0x14` (base) / `+0x15` (max) | `+0x75` (`uint8`) | **`+0x14`** (`int32`) | `-0x5E50` (`0xA1B0`) |

> **Nota de Correção de Schema:** Na documentação preliminar da Fase 1 (`CHARACTERS_SCHEMA.md`), os offsets `+0x10/+0x11` e `+0x12/+0x13` de `idx=9` haviam sido nomeados como `Control` e `Guard`, respectivamente. A descompilação de `FUN_800999b0` (`0x800999B0`) e das rotinas de confronto confirma que **`+0x10/+0x11` (`stat_idx = 3`, `0xA1A8`) é `Guard` (`ガード`)** e **`+0x12/+0x13` (`stat_idx = 4`, `0xA1AC`) é `Control` (`コントロール`)**.

---

### 1.3. Estrutura `MatchPlayerStat` (`0x807AC5A4`, 22 slots $\times$ `0x4C` = 76 bytes)

Inicializada em `FUN_80092708` (`0x80092708`, file `+0x074428`) para os 22 jogadores da partida (`slot = 0..21`, obtido de `*(uint8_t*)(actor + 0x82)`):

| Offset | Desloc. (`rBase + slot*0x4C`) | Tipo | Nome do Campo | Inicialização em `FUN_80092708` / `FUN_800929ac` |
| :---: | :---: | :---: | :--- | :--- |
| `+0x00` | `0xA19C` | `int32_t` | `kick` | `roster[+0x70]` (`uint8`) |
| `+0x04` | `0xA1A0` | `int32_t` | `catch_stat` | `roster[+0x71]` (`uint8`) |
| `+0x08` | `0xA1A4` | `int32_t` | `body` | `roster[+0x72]` (`uint8`) |
| `+0x0C` | `0xA1A8` | `int32_t` | `guard` | `roster[+0x73]` (`uint8`) |
| `+0x10` | `0xA1AC` | `int32_t` | `control` | `roster[+0x74]` (`uint8`) |
| `+0x14` | `0xA1B0` | `int32_t` | `speed` | `roster[+0x75]` (`uint8`) |
| `+0x18` | `0xA1B4` | `int32_t` | `aggression_foul_rate` | `roster[+0x76]` (`uint8`, taxa base de falta no carrinho em `0x80075968`) |
| `+0x1C` | `0xA1B8` | `int32_t` | `tp_recovery_aux` | `roster[+0x77]` (`uint8`) |
| `+0x20` | `0xA1BC` | `int32_t` | `charge_aux` | `roster[+0x78]` (`uint8`) |
| `+0x24` | `0xA1C0` | `float` | `speed_DashMult` | Multiplicador de corrida (`FUN_800929ac`, baseado em `speed`) |
| `+0x28` | `0xA1C4` | `float` | `speed_DribbleMult` | Multiplicador de condução com bola (`FUN_800929ac`, baseado em `speed`) |
| `+0x40` | `0xA1DC` | `int32_t` | `rng_trait_bias` | `+1` se `(roster[+0x79] & 0x01)`, `+1` se `(& 0x10)`, `-1` se `(& 0x20)` |
| `+0x44` | `0xA1E0` | `int32_t` | `current_tp` | `roster[+0x6C]` (`uint16`) |
| `+0x48` | `0xA1E4` | `uint8_t` | `passive_flag_bit1` | `1` se `(roster[+0x79] & 0x02) != 0`, senão `0` |

---

### 1.4. Estrutura `MatchActor` (Ator em Campo, ponteiro `actor` em `r3`/`r4`)

| Offset | Tipo | Nome do Campo | Descrição |
| :---: | :---: | :--- | :--- |
| `+0x08` | `float[3]` | `position` | Posição 3D `(x, y, z)` no gramado (metros/unidades de mundo) |
| `+0x20` | `float` | `facing_angle_rad` | Ângulo de orientação horizontal em radianos |
| `+0x2E` | `int8_t` | `team_side` | Lado do time (`0` = Casa/Esquerda, `1` = Visitante/Direita) |
| `+0x44` | `int16_t` | `character_id` | ID do personagem (`1..411`, indexa `idx=9` e `idx=15`) |
| `+0x4E` | `uint8_t` | `element_id` | Elemento do jogador (`0` = Vento, `1` = Floresta, `2` = Fogo, `3` = Terra) |
| `+0x80` | `uint8_t` | `team_idx` | Índice de controle da equipe (`0` ou `1`) |
| `+0x82` | `uint8_t` | `match_slot` | Slot na partida (`0..21`, indexa `MatchPlayerStat` em `0x807AC5A4`) |
| `+0x84` | `uint8_t` | `waza_evol_level` | Nível de evolução das técnicas do jogador (`0..3`, copiado de `roster[+0x0C]`) |
| `+0x85` | `uint8_t` | `passive_flag_bit3` | `1` se `(roster[+0x79] & 0x08) != 0` |
| `+0x98` | `float` | `gk_charge_timer` | Tempo de carga (`0.0 .. 0.9+` s) para defesa normal de goleiro (`FUN_8009ee18`) |

---

### 1.5. Tabela Global de Balanceamento de Partida (`dat.bin` `idx=7` / `0x9C47` `sec[0]`)

Carregada em `0x8002AA6C..0x8002AA80` (file `+0x00C78C..+0x00C7A0`) e armazenada no ponteiro global `iRam8051d5ec` (`0x8051D5EC`):

| Offset em `sec[0]` | Valor (`int32` BE) | Símbolo / Papel na Mecânica |
| :---: | :---: | :--- |
| `+0x0010` | **`4700`** | Coordenada X da linha do gol em centímetros (`4700 / 100.0f = 47.0f` m) |
| `+0x2034` | **`200`** | Distância máxima de decaimento de chute $d_{\max} = 200.0\text{ m}$ |
| `+0x20D4` | **`20`** | Bônus de atributo efetivo do Keyman (`+20`) aplicado em `FUN_80092b2c` |
| `+0x212C` | **`25`** | Distância inicial sem penalidade para chute normal: $d_{\min,\text{norm}} = 25.0\text{ m}$ |
| `+0x2130` | **`150`** | Penalidade máxima de distância para chute normal em $d_{\max}$: $D_{\max,\text{norm}} = 150$ |
| `+0x2134` | **`50`** | Distância inicial sem penalidade para **Long Shoot** (`attr == 1`): $d_{\min,\text{long}} = 50.0\text{ m}$ |
| `+0x2138` | **`100`** | Penalidade máxima de distância para **Long Shoot** em $d_{\max}$: $D_{\max,\text{long}} = 100$ |
| `+0x213C` | **`25`** | Percentual herdado do poder do chute anterior em **Chain Shoot** (`25%`) |
| `+0x2140` | **`10`** | Bônus fixo adicional por elo de **Chain Shoot** (`+10 * chain_count`) |
| `+0x26AC .. +0x26F0` | Matriz $3 \times 3 \times 2$ | Probabilidades (`%`) de redução de nível efetivo da defesa por diferença de Nv (Seção 4.3) |

---

## 2. Fator de Aleatoriedade / PRNG (`0x80330D14` e `0x8009F3E0`)

### 2.1. Gerador Pseudo-Aleatório Base (`FUN_80330d14` @ `0x80330D14`, file `+0x312A34`)

O motor utiliza um registrador de deslocamento com realimentação generalizada (**GFSR / Lagged Fibonacci XOR $(521, 32)$**) operando sobre um buffer de estado `uint32_t state[521]` em `0x80900A00` e índice `int32_t idx` em `0x8050E408`:

$$\text{idx} \leftarrow (\text{idx} + 1) \bmod 521$$
$$j \leftarrow (\text{idx} - 32) \bmod 521$$
$$\text{state}[\text{idx}] \leftarrow \text{state}[\text{idx}] \oplus \text{state}[j]$$
$$\text{RNG}(a, b) = \min(a, b) + \left( \text{state}[\text{idx}] \bmod (\lvert b - a \rvert + 1) \right)$$

> O wrapper `FUN_8008ef20(min, max, file, line)` (`0x8008EF20`, file `+0x070C40`) invoca diretamente `FUN_80330d14(min, max)`.

### 2.2. Sorteio de Variância de Confronto (`FUN_8009f3e0` @ `0x8009F3E0`, file `+0x081100`)

Aplicado ao poder de Chute (`FUN_8009e36c`), Shoot Counter (`FUN_8009eac8`), Shoot Block (`FUN_8009ec70`) e Defesa de Goleiro (`FUN_8009ee18`). Realiza um **sorteio em dois estágios** dependente das flags passivas do jogador (`roster[+0x79]`) e do estado de Keshin (`FUN_80093254`):

1. **Limite Inferior ($R_{\min}$):**
   $$R_{\min} = \begin{cases} \text{RNG}(0, 10) & \text{se } (\text{roster}[+0x79] \mathbin{\&} \text{0x10}) \neq 0 \\ 0 & \text{se } (\text{roster}[+0x79] \mathbin{\&} \text{0x20}) \neq 0 \\ \text{RNG}(0, 5) & \text{caso contrário (padrão)} \end{cases}$$

2. **Limite Superior ($R_{\max}$):**
   $$R_{\max} = \begin{cases} 19 \text{ (0x13)} & \text{se } \text{ignore\_keshin} == 0 \land \text{IsKeshinActive}(\text{actor}) \neq 0 \\ 10 \text{ (0x0A)} & \text{caso contrário} \end{cases}$$

3. **Resultado Final ($R_{\text{battle}}$):**
   $$R_{\text{battle}} = \text{RNG}(R_{\min}, R_{\max}) \in [0, 19]$$
   *(Para um jogador padrão sem Keshin ativo, $R_{\min} \sim U\{0..5\}$ e $R_{\text{battle}} \sim U\{R_{\min}..10\}$, com valor esperado $\mathbb{E}[R_{\text{battle}}] = 6.25$).*

---

## 3. Atributos Efetivos, Modificadores Elementais e Nível de Técnica

### 3.1. Cálculo de Atributo Efetivo (`FUN_80092b2c` @ `0x80092B2C`, file `+0x07484C`)

Dado o valor base do atributo na partida (`stat_val`) e o seu índice (`stat_idx` $\in \{0..5\}$), a função verifica o tipo de bônus tático de **Keyman** da equipe (`FUN_8007c4c4` @ `0x8007C4C4`, que lê `team_state[+0x6C]`):

$$\text{Stat}_{\text{eff}}(\text{actor}, \text{stat\_idx}) = \text{stat\_val} + \begin{cases} 20 & \text{se } \text{stat\_idx} \in \{0, 4\} \text{ (Kick/Control)} \land \text{KeymanType} == 1 \\ 20 & \text{se } \text{stat\_idx} \in \{1, 2, 3\} \text{ (Catch/Body/Guard)} \land \text{KeymanType} == 2 \\ 20 & \text{se } \text{stat\_idx} == 5 \text{ (Speed)} \land \text{KeymanType} == 0 \\ 0 & \text{caso contrário} \end{cases}$$

*(Onde a constante `20` provém de `*(int32_t*)(iRam8051d5ec + 0x20D4)`).*

---

### 3.2. Multiplicador de Velocidade de Locomoção (`FUN_800929ac` @ `0x800929AC`, file `+0x0746CC`)

Calcula os multiplicadores escalares `speed_DashMult` (`+0x24`, `0xA1C0`) e `speed_DribbleMult` (`+0x28`, `0xA1C4`) a partir de $S = \text{Stat}_{\text{eff}}(\text{actor}, 5)$ (`Speed`), usando interpolação linear em 6 faixas com constantes em `.sdata` (`0x8050C4F8` = `0.088f`, `0x8050C4FC` = `0.095f`):

$$\text{Mult}(S, k) = 1.0 + \text{tier}(S) \cdot (1.0 + k) + \frac{S - S_{\text{base}}}{20.0} \cdot k$$

onde $k = 0.095$ para corrida sem bola (`+0x24`) e $k = 0.088$ para condução com bola (`+0x28`), e:
- $S < 20$: $\text{tier} = -2, \; S_{\text{base}} = 0$
- $20 \le S < 40$: $\text{tier} = -1, \; S_{\text{base}} = 20$
- $40 \le S < 60$: $\text{tier} = 0, \; S_{\text{base}} = 40$ *(neutro em $S = 40 \implies 1.0$)*
- $60 \le S < 80$: $\text{tier} = +1, \; S_{\text{base}} = 60$
- $80 \le S < 100$: $\text{tier} = +2, \; S_{\text{base}} = 80$
- $S \ge 100$: $\text{tier} = +3, \; \text{fração} = 0$

---

### 3.3. Matriz de Afinidade Elemental (`FUN_8009f4c0` @ `0x8009F4C0`, file `+0x0811E0`)

Os elementos são codificados como:
- `0` = **Vento** (`風` / Wind)
- `1` = **Floresta** (`林` / Wood)
- `2` = **Fogo** (`火` / Fire)
- `3` = **Terra / Montanha** (`山` / Mountain)
- `4` = **Neutro / Sem Elemento** (`無`)
- `5` = **Herdar Elemento do Usuário** *(Se `waza.element_id == 5`, o código substitui por `*(uint8_t*)(actor + 0x4E)` antes de chamar `FUN_8009f4c0`)*.

O ciclo de vantagem elemental implementado em `FUN_8009f4c0(elem_A, elem_B)` é:

$$\text{ Floresta (1)} \succ \text{Vento (0)} \succ \text{Terra (3)} \succ \text{Fogo (2)} \succ \text{Floresta (1)}$$

$$E(A, B) = \text{FUN\_8009f4c0}(A, B) = \begin{cases} +1 & \text{se } (A,B) \in \{(0,3), (1,0), (2,1), (3,2)\} \text{ (Vantagem para } A\text{)} \\ -1 & \text{se } (A,B) \in \{(0,1), (1,2), (2,3), (3,0)\} \text{ (Desvantagem para } A\text{)} \\ 0 & \text{caso contrário (Mesmo elemento, oposto diagonal ou neutro)} \end{cases}$$

**Dupla Verificação Elemental nos Confrontos:**
Tanto nas disputas de finalização quanto nos duelos de Hissatsu 1v1, o jogo avalia **dois pares elementais independentes**:
1. **Elemento da Técnica vs. Elemento da Técnica** ($E(W_A, W_B)$)
2. **Elemento do Jogador vs. Elemento do Jogador** ($E(P_A, P_B)$)

---

### 3.4. Poder Base vs. Evoluído e Nível de Técnica (`idx=81` `sec[0]`, `0x8051EBD8`)

Cada registro de técnica em `iRam8051ebd8 + waza_id * 0x8C` (`140` bytes) possui:
- `+0x00` (`int16`): `tp_cost`
- `+0x02` (`int16`): `base_power` (`psVar[1]`)
- `+0x04` (`int16`): `max_power` (`psVar[2]`, poder evoluído: Shin / Kai / G2..G5 / V2..V4 / Z)
- `+0x08` (`uint8`): `element_id` (`0..3`, `4` neutro, `5` herda do jogador)
- `+0x0A` (`uint16`): `special_attribute_id` (`0` = Normal, `1` = Long Shoot, `2` = Shoot Block, `3` = Chain Shoot, `4` = Punching)
- `+0x0C` (`uint16`): `level_tier_raw` (`0` = Normal/Sem Hissatsu, `1` = Nv 1, `2` = Nv 2, `3` = Nv 3)
- `+0x86` (`uint8`): `flags_0x86` (Bits `0..1`: desativa bônus de carga de chute se `(flags_0x86 & 3) != 0`)

**Seleção de Poder da Técnica (`FUN_800b8c04` @ `0x800B8C04`, file `+0x09A924`):**
A função verifica se `*(uint8_t*)(actor + 0x84) >= 3` (nível máximo de evolução da técnica alcançado pelo jogador):

$$W_{\text{power}}(\text{actor}, \text{waza}) = \begin{cases} \text{waza.max\_power} & \text{se } \text{FUN\_800b8c04}(\text{actor}, \text{waza\_id}) \neq 0 \land \text{waza.max\_power} > 0 \\ \text{waza.base\_power} & \text{caso contrário} \end{cases}$$

---

## 4. Disputa de Finalização: Chute vs. Defesa (`0x8009E36C .. 0x8009F074`)

### 4.1. Fórmula do Poder de Chute ($P_{\text{chute}}$ — `FUN_8009e36c` @ `0x8009E36C`, file `+0x08008C`)

O poder de chute inicial $P_{\text{chute}}$ (armazenado globalmente em `iRam807ac578` @ `0x807AC578`) é dado por:

$$P_{\text{chute}} = W_{\text{shoot}} + \left\lfloor \frac{\text{Stat}_{\text{eff}}(\text{shooter}, 0 \text{ [Kick]})}{2} \right\rfloor + B_{\text{charge/chain}} + B_{\text{burning}} + R_{\text{battle}} - D_{\text{dist}}$$

Onde cada termo é calculado exatamente na seguinte ordem em `FUN_8009e36c`:

1. **Poder da Técnica ($W_{\text{shoot}}$):**
   - Se chute normal (`sRam807ace56 == 0`): $W_{\text{shoot}} = 0$, elemento da técnica = `4` (Neutro), nível da técnica = `0`.
   - Se Hissatsu de Chute (`sRam807ace56 != 0`): $W_{\text{shoot}} = W_{\text{power}}(\text{shooter}, \text{waza})$.

2. **Bônus de Carga ou Chain Shoot ($B_{\text{charge/chain}}$):**
   - **Caso A — Primeiro Chute (`chain_count == 0`):**
     Seja $t_{\text{charge}} = \text{FUN\_800afab8}(\text{shooter})$ o tempo de carga do chute em segundos. Se `(waza.flags_0x86 & 3) == 0`:
     $$B_{\text{charge}} = \begin{cases} 0 & \text{se } t_{\text{charge}} \le 0.0 \\ \left\lfloor \frac{t_{\text{charge}}}{0.9} \times 20.0 \right\rfloor & \text{se } 0.0 < t_{\text{charge}} < 0.9 \\ 20 & \text{se } t_{\text{charge}} \ge 0.9 \end{cases}$$
     *(Constantes em `.sdata2`: `0.9f` @ `0x8050F178`, `20.0f` @ `0x8050F174`).*
   - **Caso B — Chain Shoot (`chain_count > 0`, quando um segundo atacante encadeia outro chute na trajetória):**
     Seja $P_{\text{prev}} = \max(0, \text{iRam807ac578})$ o poder residual do chute anterior. Usando `sec0[+0x213C] = 25` e `sec0[+0x2140] = 10`:
     $$B_{\text{chain}} = \left\lfloor \frac{P_{\text{prev}} \times 25}{100} \right\rfloor + (10 \times \text{chain\_count})$$

3. **Contribuição do Atributo Kick:**
   $$\text{KickTerm} = \left\lfloor \frac{\text{Stat}_{\text{eff}}(\text{shooter}, 0)}{2} \right\rfloor$$

4. **Bônus de Burning Phase / Inazuma Gauge (`0x8009EA64`):**
   - Na inicialização do chute (`FUN_8009e944`), se `FUN_8007c444(shooter) != 0` (Burning Phase ativa), soma-se:
     $$B_{\text{burning}} = +30 \text{ (0x1E)}$$

5. **Variância Aleatória ($R_{\text{battle}}$):**
   - Se a distância euclidiana entre o chutador e o goleiro adversário for $\le 25.0\text{ m}$ (`0x8050F180`), a flag de chute à queima-roupa `uRam807ac598 = 1` é ativada.
   - $R_{\text{battle}} = \text{FUN\_8009f3e0}(\text{shooter}, \text{uRam807ac598}) \in [0, 19]$.

6. **Decaimento Linear por Distância ao Gol ($D_{\text{dist}}$):**
   Seja $d = \min\!\left(200.0, \sqrt{(x_{\text{shooter}} - x_{\text{goal}})^2 + y_{\text{shooter}}^2 + z_{\text{shooter}}^2}\right)$ a distância ao centro do gol adversário ($x_{\text{goal}} = \pm 47.0\text{ m}$ conforme `sec0[+0x0010] / 100.0f`, e limite $200.0\text{ m}$ em `0x8050F17C`).
   - Para **Chute Normal / Hissatsu Padrão** (`waza.special_attribute_id != 1`):
     $$d_{\min} = 25.0\text{ m} \text{ (`+0x212C`)}, \quad D_{\max} = 150 \text{ (`+0x2130`)}$$
   - Para **Long Shoot** (`waza.special_attribute_id == 1`):
     $$d_{\min} = 50.0\text{ m} \text{ (`+0x2134`)}, \quad D_{\max} = 100 \text{ (`+0x2138`)}$$
   - Fórmula de decaimento (`0x8009E64C..0x8009E698`):
     $$D_{\text{dist}} = \begin{cases} 0 & \text{se } d \le d_{\min} \\ \left\lfloor \frac{d - d_{\min}}{200.0 - d_{\min}} \times D_{\max} \right\rfloor & \text{se } d > d_{\min} \end{cases}$$

---

### 4.2. Fórmulas do Poder de Defesa ($P_{\text{defesa}}$)

Existem **três sub-rotinas distintas** de oposição a um chute em andamento, cada uma utilizando um atributo primário específico do defensor e comparando $P_{\text{defesa}}$ contra $P_{\text{chute}}$ (`iRam807ac578`):

#### A) Contra-Chute / Shoot Counter (`FUN_8009eac8` @ `0x8009EAC8`, file `+0x0807E8`)
Acionado quando um jogador intercepta o chute com uma **Hissatsu de Chute** dotada de propriedade de contra-chute. Utiliza **Kick (`stat_idx = 0`, `0xA19C`)**:

$$P_{\text{counter}} = W_{\text{power}}(\text{def}, \text{waza}) + 30 \cdot \mathbb{I}_{\text{burning}} + \left\lfloor \frac{\text{Stat}_{\text{eff}}(\text{def}, 0 \text{ [Kick]})}{2} \right\rfloor + 10 \cdot E(W_{\text{def}}, W_{\text{shoot}}) + 10 \cdot E(P_{\text{def}}, P_{\text{shooter}}) + R_{\text{battle}}(\text{def}, 0)$$

#### B) Bloqueio de Chute / Shoot Block (`FUN_8009ec70` @ `0x8009EC70`, file `+0x080990`)
Acionado quando um defensor usa uma **Hissatsu de Bloqueio** com atributo `Shoot Block` (`special_attribute_id == 2`) na trajetória do chute. Utiliza **Guard (`stat_idx = 3`, `0xA1A8`)**:

$$P_{\text{block\_shoot}} = W_{\text{power}}(\text{def}, \text{waza}) + 30 \cdot \mathbb{I}_{\text{burning}} + \left\lfloor \frac{\text{Stat}_{\text{eff}}(\text{def}, 3 \text{ [Guard]})}{2} \right\rfloor + 10 \cdot E(W_{\text{def}}, W_{\text{shoot}}) + 10 \cdot E(P_{\text{def}}, P_{\text{shooter}}) + R_{\text{battle}}(\text{def}, 0)$$

#### C) Defesa de Goleiro: Normal, Burning Catch e Hissatsu (`FUN_8009ee18` @ `0x8009EE18`, file `+0x080B38`)
Acionado na disputa final no gol. Utiliza **Catch (`stat_idx = 1`, `0xA1A0`)**:

$$P_{\text{defesa\_gk}} = B_{\text{gk\_base}} + \left\lfloor \frac{\text{Stat}_{\text{eff}}(\text{gk}, 1 \text{ [Catch]})}{2} \right\rfloor + 10 \cdot E(W_{\text{gk}}, W_{\text{shoot}}) + 10 \cdot E(P_{\text{gk}}, P_{\text{shooter}}) + R_{\text{battle}}(\text{gk}, \text{uRam807ac598})$$

onde o termo base $B_{\text{gk\_base}}$ depende de o goleiro usar técnica (`sRam807ace56 != 0`) ou defesa normal (`sRam807ace56 == 0`):
- **Com Hissatsu de Goleiro (`sRam807ace56 != 0`):**
  $$B_{\text{gk\_base}} = W_{\text{power}}(\text{gk}, \text{waza}) + 30 \cdot \mathbb{I}_{\text{burning}}$$
- **Defesa Normal / Carregada (`sRam807ace56 == 0`):**
  Seja $t_{\text{gk}} = \text{*(float*)(gk + 0x98)}$ o tempo de carga do goleiro (`0.0 .. 0.9` s).
  - Se `iRam807ac59c != 0` (override de Burning Catch / estado especial): $B_{\text{gk\_base}} = \text{iRam807ac59c}$.
  - Caso contrário:
    $$B_{\text{gk\_base}} = \begin{cases} 0 & \text{se } t_{\text{gk}} \le 0.0 \\ \left\lfloor \frac{t_{\text{gk}}}{0.9} \times 50.0 \right\rfloor & \text{se } 0.0 < t_{\text{gk}} < 0.9 \\ 50 & \text{se } t_{\text{gk}} \ge 0.9 \end{cases}$$

---

### 4.3. Resolução da Finalização, Critério de Desempate e Atenuação de Chute

Em todas as três rotinas (`FUN_8009eac8`, `FUN_8009ec70`, `FUN_8009ee18`), a comparação entre $P_{\text{defesa}}$ e $P_{\text{chute}}$ (`iRam807ac578`) segue a regra exata:

1. **Critério de Vitória / Desempate Pró-Defensor ($P_{\text{defesa}} \ge P_{\text{chute}}$):**
   $$\text{Se } P_{\text{defesa}} \ge P_{\text{chute}} \implies \text{uRam807ace5b} = 1 \quad (\textbf{Defesa / Bloqueio Bem-Sucedido!})$$
   - **Nota sobre Empate ($P_{\text{defesa}} == P_{\text{chute}}$):** O branch em assembly é `blt` (`if (P_chute <= P_defesa)`), portanto **todo empate exato favorece o defensor/goleiro**.
   - Se a técnica do goleiro tiver `special_attribute_id == 4` (**Punching / Espalmada**), a bola é rebatida para escanteio/rebote em vez de encaixada.

2. **Quebra de Bloqueio / Gol ($P_{\text{defesa}} < P_{\text{chute}}$):**
   $$\text{Se } P_{\text{defesa}} < P_{\text{chute}} \implies \text{uRam807ace5b} = 0 \quad (\textbf{Defesa Rompida / Gol!})$$
   Quando um bloqueio de linha (`FUN_8009ec70`), contra-chute (`FUN_8009eac8`) ou defesa é rompido, o poder residual do chute é reduzido em **um terço** do poder da defesa vencida (implementado via multiplicação inteira por `0x55555556` `mulhw`):
   $$P_{\text{chute}}^{\text{novo}} = \max\!\left(0, \; P_{\text{chute}} - \left\lfloor \frac{P_{\text{defesa}}}{3} \right\rfloor\right)$$

3. **Ajuste de Nível de Técnica Chute vs. Goleiro (`0x8009E6B4..0x8009E810`):**
   Quando o chute (`Lv_shoot` $\in \{1, 2, 3\}$) enfrenta uma Hissatsu de goleiro (`Lv_gk` $\in \{1, 2, 3\}$), `FUN_8009e36c` consulta a matriz $3 \times 3$ em `iRam8051d5ec + 0x26AC` com um sorteio $r \sim \text{RNG}(0, 100)$ para determinar o nível efetivo `iRam807ac594` (`0, 1, 2`) da animação/efeito de impacto:

   | Nv Chute \ Nv GK | GK Nv 1 (`prob -1`, `prob -2`) | GK Nv 2 (`prob -1`, `prob -2`) | GK Nv 3 (`prob -1`, `prob -2`) |
   | :---: | :---: | :---: | :---: |
   | **Chute Nv 1** | `(0%, 0%)` | `(100%, 0%)` | `(0%, 100%)` |
   | **Chute Nv 2** | `(0%, 20%)` | `(10%, 20%)` | `(10%, 10%)` |
   | **Chute Nv 3** | `(30%, 10%)` | `(0%, 30%)` | `(0%, 0%)` |

---

## 5. Disputa de Linha (1v1 Terrestre: Drible vs. Bloqueio e Desarmes)

O motor possui **três subsistemas matemáticos separados** para disputas de linha:
1. **Command Battle de Hissatsu 1v1** (`FUN_8009f078` @ `0x8009F078`): quando atacante e/ou marcador ativam técnicas de Drible vs. Bloqueio.
2. **Duelo Terrestre Normal de Investida / Tackle (`A` / Carga de Corpo)** (`FUN_80075754` @ `0x80075754`): quando o marcador dá o bote normal sobre o condutor da bola.
3. **Carrinho (Sliding Tackle `B`), Falta e Interceptação de Passe/Chute** (`FUN_8007590C` @ `0x8007590C` e `FUN_80075A10` @ `0x80075A10`).

---

### 5.1. Duelo 1v1 de Hissatsu: Drible vs. Bloqueio (`FUN_8009f078` @ `0x8009F078`, file `+0x080D98`)

Quando ocorre um confronto de comando entre o atacante (`atk`, técnica $W_{\text{atk}}$ de categoria `1 = Dribble`) e o defensor (`def`, técnica $W_{\text{def}}$ de categoria `2 = Block`):

1. **Casos Triviais (Assimetria de Uso de Hissatsu):**
   - Se $W_{\text{atk}} == 0 \land W_{\text{def}} == 0 \implies \text{Resultado} = 0$ (**Empate**).
   - Se $W_{\text{atk}} \neq 0 \land W_{\text{def}} == 0 \implies \text{Resultado} = 1$ (**Atacante vence automaticamente**).
   - Se $W_{\text{atk}} == 0 \land W_{\text{def}} \neq 0 \implies \text{Resultado} = 2$ (**Defensor vence automaticamente**).

2. **Caso Hissatsu vs. Hissatsu ($W_{\text{atk}} \neq 0 \land W_{\text{def}} \neq 0$):**
   O jogo **não usa RNG** no duelo Hissatsu 1v1! Ele calcula uma **Pontuação Discreta Total ($S_{\text{duel}}$)** somando **5 critérios determinísticos**:

   Seja a função degrau de limiar 20 definida em `0x8009F1D8..0x8009F244` como:
   $$\text{step}_{20}(\Delta) = \begin{cases} +2 & \text{se } \Delta \ge 21 \\ +1 & \text{se } 1 \le \Delta \le 20 \\ 0 & \text{se } \Delta == 0 \\ -1 & \text{se } -19 \le \Delta \le -1 \\ -2 & \text{se } \Delta \le -20 \end{cases}$$

   Os 5 componentes são:
   - **1. Afinidade Elemental das Técnicas:**
     $$S_{\text{waza\_elem}} = E(\text{elem}(W_{\text{atk}}), \text{elem}(W_{\text{def}})) \in \{-1, 0, +1\}$$
   - **2. Afinidade Elemental dos Jogadores:**
     $$S_{\text{char\_elem}} = E(\text{elem}(P_{\text{atk}}), \text{elem}(P_{\text{def}})) \in \{-1, 0, +1\}$$
   - **3. Diferença de Nível da Técnica (`level_tier_raw` $\in \{1, 2, 3\}$):**
     $$S_{\text{level}} = \text{Lv}(W_{\text{atk}}) - \text{Lv}(W_{\text{def}}) \in \{-2, -1, 0, +1, +2\}$$
   - **4. Diferença de Poder da Técnica ($W_{\text{power}}$):**
     $$S_{\text{power}} = \text{step}_{20}\!\left(W_{\text{power}}(\text{atk}, W_{\text{atk}}) - W_{\text{power}}(\text{def}, W_{\text{def}})\right) \in \{-2, -1, 0, +1, +2\}$$
   - **5. Diferença de Atributo (`Body` do Atacante vs. `Guard` do Defensor):**
     $$S_{\text{stat}} = \text{step}_{20}\!\left(\text{Stat}_{\text{eff}}(\text{atk}, 2 \text{ [Body]}) - \text{Stat}_{\text{eff}}(\text{def}, 3 \text{ [Guard]})\right) \in \{-2, -1, 0, +1, +2\}$$

   **Soma e Veredito (`0x8009F248..0x8009F29C`):**
   $$S_{\text{duel}} = S_{\text{waza\_elem}} + S_{\text{char\_elem}} + S_{\text{level}} + S_{\text{power}} + S_{\text{stat}} \in [-8, +8]$$

   $$\text{Resultado} = \begin{cases} 1 \text{ (Atacante vence o drible)} & \text{se } S_{\text{duel}} \ge +1 \\ 2 \text{ (Defensor vence o bloqueio/desarme)} & \text{se } S_{\text{duel}} \le -1 \\ 0 \text{ (Empate — ambos sofrem knockback)} & \text{se } S_{\text{duel}} == 0 \end{cases}$$

---

### 5.2. Duelo 1v1 Terrestre Normal: Drible vs. Bote/Tackle (`FUN_80075754` @ `0x80075754`, file `+0x057474`)

Acionado em `action.cpp` (linha `12335`, chamada em `0x8006BE88`) durante o bote físico terrestre entre o atacante condutor (`r30 = atk`) e o marcador (`r31 = def`). Combina **Body**, **Guard**, **Speed** e **Elemento**:

1. **Poder de Drible Terrestre ($P_{\text{drible}}$):**
   $$P_{\text{drible}} = \text{Stat}_{\text{eff}}(\text{atk}, 2 \text{ [Body]}) + \left\lfloor \frac{\text{Stat}_{\text{eff}}(\text{atk}, 5 \text{ [Speed]})}{2} \right\rfloor + 20 \cdot \mathbb{I}_{E(P_{\text{atk}}, P_{\text{def}}) == +1}$$

2. **Poder de Bloqueio / Bote Terrestre ($P_{\text{bloqueio}}$):**
   $$P_{\text{bloqueio}} = \text{Stat}_{\text{eff}}(\text{def}, 3 \text{ [Guard]}) + \left\lfloor \frac{\text{Stat}_{\text{eff}}(\text{def}, 5 \text{ [Speed]})}{2} \right\rfloor + 20 \cdot \mathbb{I}_{E(P_{\text{atk}}, P_{\text{def}}) == -1}$$

3. **Diferença Líquida ($\Delta = P_{\text{drible}} - P_{\text{bloqueio}}$) e Tabela de Probabilidade de Sucesso (`0x80075860..0x800758E8`):**
   A diferença $\Delta = P_{\text{drible}} - P_{\text{bloqueio}}$ é convertida em um limiar percentual $T(\Delta) \in \{45, 50, 55, 60, 65, 70, 75, 80\}$:

   | Faixa de $\Delta = P_{\text{drible}} - P_{\text{bloqueio}}$ | Limiar $T(\Delta)$ (`%`) |
   | :---: | :---: |
   | $\Delta \ge 56$ | **`80%`** |
   | $46 \le \Delta \le 55$ | **`75%`** |
   | $36 \le \Delta \le 45$ | **`70%`** |
   | $26 \le \Delta \le 35$ | **`65%`** |
   | $16 \le \Delta \le 25$ | **`60%`** |
   | $6 \le \Delta \le 15$ | **`55%`** |
   | $-5 \le \Delta \le 5$ | **`50%`** |
   | $\Delta \le -6$ | **`45%`** |

4. **Sorteio Final (`0x80075840` & `0x800758EC..0x800758F4`):**
   Sorteia-se $r = \text{RNG}(0, 100)$:
   $$\text{Atacante mantém a posse / vence o bote} \iff r < T(\Delta)$$
   $$\text{Defensor realiza o desarme limpo} \iff r \ge T(\Delta)$$

---

### 5.3. Tratamento de Faltas no Carrinho (Sliding Tackle — `FUN_8007590C` @ `0x8007590C`, file `+0x05762C`)

Acionado em `action.cpp` (linha `12404`, chamada em `0x8006A8B0`) quando um defensor atinge o portador da bola com um carrinho (`Sliding Tackle`):

1. **Verificação de Regra de Partida (`0x80075934`):**
   - Se o bit de faltas desativadas estiver ativo em `*(uint16_t*)(0x807BCF00) & 0x08`, retorna `0` (**Sem falta / Desarme limpo**).
2. **Cálculo do Ângulo de Ataque ($\theta$) e Taxa Base de Falta (`aggression_foul_rate`):**
   - Lê $F_{\text{base}} = \text{stat.aggression\_foul\_rate}$ (`+0x18` / `0xA1B4`, vindo de `roster[+0x76]`).
   - Calcula a diferença angular absoluta $\theta \in [0, \pi]$ (`FUN_80330290` / `FUN_8033045C`) entre o vetor direção do carrinho e a orientação do portador da bola (`+0x20`).
3. **Probabilidade de Falta por Setor Angular (`0x800759A0..0x800759DC`):**
   - **Por trás ($\theta > \frac{3\pi}{4} \approx 2.3562\text{ rad} = 135^\circ$, `0x8050EC08`):**
     $$P_{\text{falta}} = F_{\text{base}} + 15\%$$
   - **Pela lateral ($\frac{\pi}{2} < \theta \le \frac{3\pi}{4}$, onde $\frac{\pi}{2} \approx 1.5708\text{ rad} = 90^\circ$, `0x8050EBAC`):**
     $$P_{\text{falta}} = \left\lfloor \frac{F_{\text{base}}}{2} \right\rfloor + 7\%$$
   - **Pela frente ($\theta \le \frac{\pi}{2} = 90^\circ$):**
     $$P_{\text{falta}} = 0\% \quad (\textbf{Desarme 100\% limpo pela frente!})$$
4. **Sorteio de Falta (`0x8007599C`):**
   - Sorteia-se $r = \text{RNG}(0, 99)$:
     $$\text{Falta marcada pelo árbitro} \iff r < P_{\text{falta}}$$

---

### 5.4. Domínio de Bola / Recepção (`Control` — `0x8006BF90` & `0x800729F0`)

O atributo **Control (`コントロール`, `stat_idx = 4`, `0xA1AC`)** governa a precisão do domínio de bola (trap) em passes fortes ou bolas aéreas (`action.cpp`, linha `12385`):
- Sorteia-se $r = \text{RNG}(0, 100)$ e compara-se com $C = \text{Stat}_{\text{eff}}(\text{actor}, 4 \text{ [Control]})$:
  - Em `0x8006BFDC` (Recepção aérea): se $C \ge r$, a bola amortece a `1.0f` m do jogador; se $C < r$, espirra a `1.7f` m.
  - Em `0x80072A58` (Corte / Drible curto): se $C \ge r$, raio de controle `1.6f` m; se $C < r$, raio `3.0f` m.

---

### 5.5. Interceptação de Chute Sem Hissatsu no Meio do Caminho (`FUN_80075A10` @ `0x80075A10`, file `+0x057730`)

Quando um chute normal passa próximo a um jogador de linha ou goleiro sem ativar Command Battle (`action.cpp`, linhas `12498..12530`, chamada em `0x8005FCD0`):
1. **Poder de Chute Físico ($P_{\text{atk}}$):**
   $$P_{\text{atk}} = \left\lfloor \text{Stat}_{\text{eff}}(\text{shooter}, 0 \text{ [Kick]}) \times M_{\text{charge}} \right\rfloor - D_{\text{dist\_sq}} + 20 \cdot \mathbb{I}_{E(P_{\text{shooter}}, P_{\text{def}}) == +1} + B_{\text{state}}$$
   onde $M_{\text{charge}} \in \{1.3, 1.6, 1.9\}$ conforme o estágio de carga (`0, 1, 2`), e $D_{\text{dist\_sq}} \in [0, 180]$ decai linearmente com a distância quadrática $d^2 \in [400.0, 40000.0]$.
2. **Poder de Rebatida/Trava do Defensor ($P_{\text{def}}$):**
   $$P_{\text{def}} = \text{Stat}_{\text{eff}}(\text{def}, 3 \text{ [Guard]}) + \text{Stat}_{\text{eff}}(\text{def}, 1 \text{ [Catch]}) + 20 \cdot \mathbb{I}_{E(P_{\text{shooter}}, P_{\text{def}}) == -1} - 40 \cdot \mathbb{I}_{\text{IsKeshinActive}(\text{def})}$$
3. **Resultado (`uRam807ac5a0` / `0xA198`):**
   Determina se a bola é travada (`0`), desviada (`1`) ou passa rasante derrubando o defensor (`2`).

---

## 6. Pseudocódigo Limpo (Pronto para Implementação em C++20 pelo `@reconstructor`)

```cpp
#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace strikers::battle {

enum class Element : uint8_t {
    Wind     = 0, // 風 (Vento)
    Wood     = 1, // 林 (Floresta)
    Fire     = 2, // 火 (Fogo)
    Mountain = 3, // 山 (Terra/Montanha)
    Neutral  = 4, // 無 (Neutro)
    Inherit  = 5  // Herda o elemento do jogador (+0x4E)
};

enum class StatIndex : int32_t {
    Kick    = 0, // +0x00 (0xA19C) - キック
    Catch   = 1, // +0x04 (0xA1A0) - キャッチ
    Body    = 2, // +0x08 (0xA1A4) - ボディ
    Guard   = 3, // +0x0C (0xA1A8) - ガード
    Control = 4, // +0x10 (0xA1AC) - コントロール
    Speed   = 5  // +0x14 (0xA1B0) - スピード
};

struct WazaParam {
    int16_t  tp_cost;              // +0x00
    int16_t  base_power;           // +0x02
    int16_t  max_power;            // +0x04 (Poder evoluído)
    Element  element;              // +0x08
    uint16_t special_attribute_id; // +0x0A (1=Long, 2=ShootBlock, 3=Chain, 4=Punching)
    uint16_t level_tier_raw;       // +0x0C (1=Lv1, 2=Lv2, 3=Lv3)
    uint8_t  flags_0x86;           // +0x86
};

struct PlayerBattleView {
    int32_t kick;
    int32_t catch_stat;
    int32_t body;
    int32_t guard;
    int32_t control;
    int32_t speed;
    int32_t aggression_foul_rate; // +0x18 (0xA1B4)
    Element element;              // actor +0x4E
    uint8_t waza_evolution_tier;  // actor +0x84 (0..3)
    uint8_t passive_flags;        // roster +0x79
    int32_t team_keyman_buff;     // -1=None, 0=Speed, 1=Kick/Control, 2=Catch/Body/Guard
    bool    team_burning_active;  // FUN_8007c444
    bool    keshin_active;        // FUN_80093254
};

// ============================================================================
// 1. PRNG (FUN_80330d14 @ 0x80330D14) & Sorteio de Variância (FUN_8009f3e0 @ 0x8009F3E0)
// ============================================================================
class Gfsr521Rng {
    uint32_t state_[521]{};
    int32_t  idx_{0};
public:
    int32_t nextInt(int32_t a, int32_t b) noexcept {
        int32_t lo = std::min(a, b);
        int32_t hi = std::max(a, b);
        idx_ = (idx_ + 1) % 521;
        int32_t j = (idx_ - 32 + 521) % 521;
        state_[idx_] ^= state_[j];
        uint32_t span = static_cast<uint32_t>(hi - lo + 1);
        return span ? (lo + static_cast<int32_t>(state_[idx_] % span)) : lo;
    }
};

inline int32_t rollBattleVariance(Gfsr521Rng& rng, const PlayerBattleView& p, bool close_range) noexcept {
    int32_t min_roll = 0;
    if (p.passive_flags & 0x10) {
        min_roll = rng.nextInt(0, 10);
    } else if (p.passive_flags & 0x20) {
        min_roll = 0;
    } else {
        min_roll = rng.nextInt(0, 5);
    }
    int32_t max_roll = (!close_range && p.keshin_active) ? 19 : 10;
    return rng.nextInt(min_roll, max_roll);
}

// ============================================================================
// 2. Afinidade Elemental (FUN_8009f4c0 @ 0x8009F4C0)
// ============================================================================
constexpr int32_t compareElements(Element a, Element b) noexcept {
    if (a == Element::Wind)     return (b == Element::Mountain) ? +1 : ((b == Element::Wood)     ? -1 : 0);
    if (a == Element::Wood)     return (b == Element::Wind)     ? +1 : ((b == Element::Fire)     ? -1 : 0);
    if (a == Element::Fire)     return (b == Element::Wood)     ? +1 : ((b == Element::Mountain) ? -1 : 0);
    if (a == Element::Mountain) return (b == Element::Fire)     ? +1 : ((b == Element::Wind)     ? -1 : 0);
    return 0;
}

constexpr Element resolveWazaElement(const WazaParam* waza, Element user_elem) noexcept {
    if (!waza) return Element::Neutral;
    return (waza->element == Element::Inherit) ? user_elem : waza->element;
}

// ============================================================================
// 3. Atributo Efetivo (FUN_80092b2c @ 0x80092B2C) e Poder de Hissatsu (FUN_800b8c04)
// ============================================================================
constexpr int32_t getEffectiveStat(const PlayerBattleView& p, StatIndex idx) noexcept {
    int32_t base = 0;
    switch (idx) {
        case StatIndex::Kick:    base = p.kick; break;
        case StatIndex::Catch:   base = p.catch_stat; break;
        case StatIndex::Body:    base = p.body; break;
        case StatIndex::Guard:   base = p.guard; break;
        case StatIndex::Control: base = p.control; break;
        case StatIndex::Speed:   base = p.speed; break;
    }
    int32_t bonus = 0;
    if ((idx == StatIndex::Kick || idx == StatIndex::Control) && p.team_keyman_buff == 1) bonus = 20;
    else if ((idx == StatIndex::Catch || idx == StatIndex::Body || idx == StatIndex::Guard) && p.team_keyman_buff == 2) bonus = 20;
    else if (idx == StatIndex::Speed && p.team_keyman_buff == 0) bonus = 20;
    return base + bonus;
}

constexpr int32_t getEffectiveWazaPower(const PlayerBattleView& p, const WazaParam* waza) noexcept {
    if (!waza) return 0;
    if (p.waza_evolution_tier >= 3 && waza->max_power > 0) {
        return waza->max_power;
    }
    return waza->base_power;
}

// ============================================================================
// 4. Poder de Chute (FUN_8009e36c @ 0x8009E36C)
// ============================================================================
inline int32_t computeShootPower(
    Gfsr521Rng& rng,
    const PlayerBattleView& shooter,
    const WazaParam* shoot_waza,
    float charge_time_sec,
    float dist_to_goal_m,
    float dist_to_gk_m,
    int32_t chain_count = 0,
    int32_t prev_shoot_power = 0
) noexcept {
    const int32_t waza_power = getEffectiveWazaPower(shooter, shoot_waza);
    const int32_t kick_half  = getEffectiveStat(shooter, StatIndex::Kick) / 2;

    int32_t charge_or_chain_bonus = 0;
    if (chain_count == 0) {
        bool disable_charge = shoot_waza && ((shoot_waza->flags_0x86 & 0x03) != 0);
        if (!disable_charge && charge_time_sec > 0.0f) {
            charge_or_chain_bonus = (charge_time_sec < 0.9f)
                ? static_cast<int32_t>((charge_time_sec / 0.9f) * 20.0f)
                : 20;
        }
    } else {
        int32_t clamped_prev = std::max(0, prev_shoot_power);
        charge_or_chain_bonus = (clamped_prev * 25) / 100 + (10 * chain_count);
    }

    const int32_t burning_bonus = shooter.team_burning_active ? 30 : 0;
    const bool close_range = (dist_to_gk_m <= 25.0f);
    const int32_t rng_bonus = rollBattleVariance(rng, shooter, close_range);

    // Decaimento por distância ao gol (dat.bin idx=7 sec[0] +0x212C..+0x2138)
    const float clamped_dist = std::min(200.0f, std::max(0.0f, dist_to_goal_m));
    const bool is_long_shoot = shoot_waza && (shoot_waza->special_attribute_id == 1);
    const float min_dist = is_long_shoot ? 50.0f : 25.0f;
    const float max_pen  = is_long_shoot ? 100.0f : 150.0f;

    int32_t dist_penalty = 0;
    if (clamped_dist > min_dist) {
        dist_penalty = static_cast<int32_t>(((clamped_dist - min_dist) / (200.0f - min_dist)) * max_pen);
    }

    return waza_power + kick_half + charge_or_chain_bonus + burning_bonus + rng_bonus - dist_penalty;
}

// ============================================================================
// 5. Poder de Defesa & Resolução contra Chute (0x8009EAC8, 0x8009EC70, 0x8009EE18)
// ============================================================================
struct ShotDefenseResult {
    int32_t defense_power;
    int32_t remaining_shoot_power;
    bool    stopped; // true se P_defesa >= P_chute
};

inline ShotDefenseResult resolveShotDefense(
    Gfsr521Rng& rng,
    const PlayerBattleView& defender,
    const WazaParam* def_waza,
    StatIndex defense_stat_type, // Kick (Counter), Guard (ShootBlock) ou Catch (GK)
    float gk_normal_charge_sec,
    int32_t current_shoot_power,
    Element shoot_waza_elem,
    Element shooter_player_elem,
    bool close_range_flag
) noexcept {
    int32_t base_power = 0;
    if (def_waza != nullptr) {
        base_power = getEffectiveWazaPower(defender, def_waza) + (defender.team_burning_active ? 30 : 0);
    } else if (defense_stat_type == StatIndex::Catch && gk_normal_charge_sec > 0.0f) {
        base_power = (gk_normal_charge_sec < 0.9f)
            ? static_cast<int32_t>((gk_normal_charge_sec / 0.9f) * 50.0f)
            : 50;
    }

    const int32_t stat_half = getEffectiveStat(defender, defense_stat_type) / 2;
    const Element def_waza_elem = resolveWazaElement(def_waza, defender.element);
    const int32_t elem_waza_bonus   = 10 * compareElements(def_waza_elem, shoot_waza_elem);
    const int32_t elem_player_bonus = 10 * compareElements(defender.element, shooter_player_elem);
    const int32_t rng_bonus = rollBattleVariance(rng, defender, (defense_stat_type == StatIndex::Catch) ? close_range_flag : false);

    const int32_t p_defesa = base_power + stat_half + elem_waza_bonus + elem_player_bonus + rng_bonus;

    if (p_defesa >= current_shoot_power) {
        return {p_defesa, 0, true};
    }
    int32_t reduced_shoot = std::max(0, current_shoot_power - (p_defesa / 3));
    return {p_defesa, reduced_shoot, false};
}

// ============================================================================
// 6. Duelo de Linha 1v1: Hissatsu Command Battle (FUN_8009f078 @ 0x8009F078)
// ============================================================================
constexpr int32_t stepThreshold20(int32_t diff) noexcept {
    if (diff >= 21)  return +2;
    if (diff >= 1)   return +1;
    if (diff == 0)   return  0;
    if (diff >= -19) return -1;
    return -2;
}

// Retorna: 1 = Atacante vence, 2 = Defensor vence, 0 = Empate
constexpr int32_t resolveHissatsuCommandBattle(
    const PlayerBattleView& attacker,
    const WazaParam* dribble_waza,
    const PlayerBattleView& defender,
    const WazaParam* block_waza
) noexcept {
    if (!dribble_waza && !block_waza) return 0;
    if (dribble_waza && !block_waza)  return 1;
    if (!dribble_waza && block_waza)  return 2;

    const Element atk_waza_elem = resolveWazaElement(dribble_waza, attacker.element);
    const Element def_waza_elem = resolveWazaElement(block_waza, defender.element);

    const int32_t score_waza_elem = compareElements(atk_waza_elem, def_waza_elem);
    const int32_t score_char_elem = compareElements(attacker.element, defender.element);
    const int32_t score_level     = static_cast<int32_t>(dribble_waza->level_tier_raw) - static_cast<int32_t>(block_waza->level_tier_raw);
    const int32_t score_power     = stepThreshold20(getEffectiveWazaPower(attacker, dribble_waza) - getEffectiveWazaPower(defender, block_waza));
    const int32_t score_stat      = stepThreshold20(getEffectiveStat(attacker, StatIndex::Body) - getEffectiveStat(defender, StatIndex::Guard));

    const int32_t total_score = score_waza_elem + score_char_elem + score_level + score_power + score_stat;
    if (total_score >= 1)  return 1;
    if (total_score <= -1) return 2;
    return 0;
}

// ============================================================================
// 7. Duelo de Linha 1v1 Terrestre Normal (FUN_80075754 @ 0x80075754)
//    e Carrinho / Falta (FUN_8007590C @ 0x8007590C)
// ============================================================================
inline bool resolveNormalTackle1v1(
    Gfsr521Rng& rng,
    const PlayerBattleView& attacker,
    const PlayerBattleView& defender
) noexcept {
    int32_t p_drible   = getEffectiveStat(attacker, StatIndex::Body)  + (getEffectiveStat(attacker, StatIndex::Speed) / 2);
    int32_t p_bloqueio = getEffectiveStat(defender, StatIndex::Guard) + (getEffectiveStat(defender, StatIndex::Speed) / 2);

    const int32_t elem_cmp = compareElements(attacker.element, defender.element);
    if (elem_cmp == +1)      p_drible   += 20;
    else if (elem_cmp == -1) p_bloqueio += 20;

    const int32_t diff = p_drible - p_bloqueio;
    int32_t win_threshold = 50;
    if      (diff >= 56) win_threshold = 80;
    else if (diff >= 46) win_threshold = 75;
    else if (diff >= 36) win_threshold = 70;
    else if (diff >= 26) win_threshold = 65;
    else if (diff >= 16) win_threshold = 60;
    else if (diff >= 6)  win_threshold = 55;
    else if (diff >= -5) win_threshold = 50;
    else                 win_threshold = 45;

    return rng.nextInt(0, 100) < win_threshold; // true = atacante escapa; false = desarme do defensor
}

inline bool checkSlidingTackleFoul(
    Gfsr521Rng& rng,
    const PlayerBattleView& tackler,
    float angle_diff_rad, // [0, pi]: 0 = frontal, pi = por trás
    bool fouls_enabled = true
) noexcept {
    if (!fouls_enabled) return false;
    int32_t foul_prob = 0;
    if (angle_diff_rad > 2.3561945f) {        // > 135 graus (por trás)
        foul_prob = tackler.aggression_foul_rate + 15;
    } else if (angle_diff_rad > 1.5707964f) { // > 90 graus (lateral)
        foul_prob = (tackler.aggression_foul_rate / 2) + 7;
    } else {                                  // <= 90 graus (frontal: sempre limpo)
        foul_prob = 0;
    }
    return rng.nextInt(0, 99) < foul_prob;
}

} // namespace strikers::battle
```

---

## 7. Tabela de Referência de Funções Descompiladas (`main.dol`)

| Endereço Virtual | File Offset (`main.dol`) | Símbolo Ghidra | Papel na Engine (`matchmgr.cpp` / `action.cpp`) |
| :---: | :---: | :--- | :--- |
| `0x80330D14` | `+0x312A34` | `FUN_80330d14` | PRNG central GFSR $(521, 32)$ com estado em `0x80900A00` |
| `0x8008EF20` | `+0x070C40` | `FUN_8008ef20` | Wrapper de RNG de partida (`GET_RANDOM(min, max, file, line)`) |
| `0x80092708` | `+0x074428` | `FUN_80092708` | Inicializa `MatchPlayerStat[22]` (`0x807AC5A4`, stride `0x4C`) a partir do Roster (`0xB0`) |
| `0x800929AC` | `+0x0746CC` | `FUN_800929ac` | Calcula multiplicadores de corrida (`+0x24`) e condução (`+0x28`) via `Speed` (`+0x14`) |
| `0x80092B2C` | `+0x07484C` | `FUN_80092b2c` | Calcula atributo efetivo (`+20` conforme Keyman buff `FUN_8007c4c4`) |
| `0x800999B0` | `+0x07B6D0` | `FUN_800999b0` | Debug HUD de `matchmgr.cpp` (confirma rótulos de `0xA19C..0xA1B0`) |
| `0x8009E36C` | `+0x08008C` | `FUN_8009e36c` | Cálculo completo do Poder de Chute ($P_{\text{chute}}$ em `iRam807ac578`) |
| `0x8009EAC8` | `+0x0807E8` | `FUN_8009eac8` | Resolução de **Shoot Counter** (`Kick` + Hissatsu vs. $P_{\text{chute}}$) |
| `0x8009EC70` | `+0x080990` | `FUN_8009ec70` | Resolução de **Shoot Block** (`Guard` + Hissatsu vs. $P_{\text{chute}}$) |
| `0x8009EE18` | `+0x080B38` | `FUN_8009ee18` | Resolução de **Defesa de Goleiro** (`Catch` + Hissatsu/Carga vs. $P_{\text{chute}}$) |
| `0x8009F078` | `+0x080D98` | `FUN_8009f078` | Resolução de **Command Battle 1v1** (Drible `Body` vs. Bloqueio `Guard`) |
| `0x8009F3E0` | `+0x081100` | `FUN_8009f3e0` | Sorteio de variância aleatória em dois estágios (`[0..5..10..19]`) |
| `0x8009F4C0` | `+0x0811E0` | `FUN_8009f4c0` | Comparador do ciclo elemental (`Vento > Terra > Fogo > Floresta > Vento`) |
| `0x800B8C04` | `+0x09A924` | `FUN_800b8c04` | Verifica evolução máxima da Hissatsu (`actor[+0x84] >= 3` $\implies$ `max_power`) |
| `0x80075754` | `+0x057474` | `FUN_80075754` | Duelo terrestre 1v1 (`Body + Speed/2` vs `Guard + Speed/2`) |
| `0x8007590C` | `+0x05762C` | `FUN_8007590C` | Probabilidade de falta no carrinho (`Sliding Tackle` por setor angular) |
| `0x80075A10` | `+0x057730` | `FUN_80075A10` | Interceptação/trava física de chute em trânsito (`Kick` vs `Guard + Catch`) |
| `0x8006BF90` | `+0x04DCBC` | `FUN_8006bf90` | Teste de domínio de bola aéreo (`Control >= RNG(0, 100)`) |
| `0x800729F0` | `+0x054710` | `FUN_800729f0` | Teste de domínio/corte em condução (`Control >= RNG(0, 100)`) |
