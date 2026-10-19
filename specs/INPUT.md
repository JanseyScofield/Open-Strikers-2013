## 1. Visão Geral
Substituir as chamadas de biblioteca do Wii (WPAD/PAD) por uma camada portável usando SDL2, permitindo entrada via Teclado e Gamepad (XInput/DualShock/Generic HID).

## 2. Ações de Jogo Requeridas (Strikers 2013)
O motor de jogo espera internamente uma máscara de bits de ações lógicas por jogador:
- **Movimento:** Eixo Direcional (Analógico / D-Pad / WASD).
- **Ações Principais:** Passe/Troca, Chute/Dividida, Drible/Especial, Corrida/Dash.
- **Modificadores:** Troca de Tática, Seleção de Nível de Técnica (Hissatsu).
- **Interface:** Pause/Menu, Confirmar, Cancelar.

## 3. Contrato de Mapeamento (JSON Schema)
As configurações devem ser lidas a partir de um arquivo em `config/bindings.json`:
- Mapeamento individual para até 4 jogadores locais.
- Suporte a remapeamento dinâmico de teclas e botões de controle via interface ou arquivo de texto.

## 4. Requisitos de Implementação (C++20)
- O subsistema de input em `src/input/` deve expor uma interface pura:
  `virtual LogicalInputState Poll(int player_index) = 0;`
- O loop principal da lógica de jogo não deve ter referências a códigos de teclas da SDL2 ou registradores do Wii.