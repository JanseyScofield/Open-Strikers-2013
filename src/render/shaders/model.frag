#version 330 core

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
