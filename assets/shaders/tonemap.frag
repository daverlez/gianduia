#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform int u_tonemapper; // 0: Linear, 1: ACES, 2: Khronos

vec3 ACESFilm(vec3 color) {
    // TODO
    return color;
}

vec3 KhronosPBRNeutral(vec3 color) {
    // TODO
    return color;
}

void main() {
    vec3 color = texture(screenTexture, TexCoords).rgb;

    if (u_tonemapper == 1) {
        color = ACESFilm(color);
    } else if (u_tonemapper == 2) {
        color = KhronosPBRNeutral(color);
    }

    FragColor = vec4(color, 1.0);
}