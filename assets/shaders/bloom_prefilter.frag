#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D srcTexture;
uniform float u_threshold;

void main() {
    vec3 color = texture(srcTexture, TexCoords).rgb;
    float brightness = max(color.r, max(color.g, color.b));

    if(brightness > u_threshold) {
        FragColor = vec4(color, 1.0);
    } else {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}