#version 330 core
        out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D screenTexture;

void main() {
    vec3 mapped = texture(screenTexture, TexCoords).rgb;

    for (int i = 0; i < 3; ++i) {
        float x = mapped[i];
        if (x <= 0.0031308f) {
            mapped[i] = 12.92f * x;
        } else {
            mapped[i] = 1.055f * pow(x, 0.41666f) - 0.055f;
        }
    }

    FragColor = vec4(mapped, 1.0);
}