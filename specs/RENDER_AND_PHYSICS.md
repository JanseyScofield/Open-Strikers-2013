# Especificação: Renderização e Cinemática (Sandbox Gráfica)

## 1. Janela e Contexto
- Gerenciamento via SDL2 com perfil de contexto OpenGL (Core ou Compatibility para debug primitives).
- Resolução padrão: 1280x720 (proporção 16:9), VSync ativado (60 FPS travados).

## 2. Sistema de Coordenadas do Campo
- Dimensões do gramado: 105.0m (Comprimento, Eixo X) x 68.0m (Largura, Eixo Z). Altura no Eixo Y.
- Centro do campo em (0.0, 0.0, 0.0).
- Renderização de linhas de depuração (Debug Renderer):
  - Retângulo exterior do campo.
  - Círculo central (raio 9.15m) e linha de meio-campo.
  - Áreas dos goleiros e traves em cada extremidade.

## 3. Cinemática de Jogador e Bola
- Atleta representado no espaço 3D por uma primitiva com orientação visual (frente/olhar).
- Velocidade escalar base governada pelos atributos do personagem (`Speed`).
- Aceleração e desaceleração suaves com atrito de solo.
- Limite de colisão: o jogador não pode ultrapassar as quatro linhas do campo.
- Polling contínuo acoplado ao `InputManager` já implementado.
