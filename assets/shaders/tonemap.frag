#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform int u_tonemapper;
uniform float u_exposure;

uniform sampler2D bloomTexture;
uniform bool u_enableBloom;
uniform float u_bloomIntensity;


// Credits for the following implementations: https://github.com/dmnsgn/glsl-tone-map


/************************************
 *  Reinhard
 ************************************/

vec3 Reinhard(vec3 color) {
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float toneMappedLuma = luma / (1.0 + luma);

    return color * (toneMappedLuma / max(luma, 0.00001));
}


/************************************
 *  Uncharted 2
 ************************************/

vec3 Uncharted2Curve(vec3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 Uncharted2(vec3 color) {
    float exposureBias = 2.0;
    vec3 curr = Uncharted2Curve(exposureBias * color);
    vec3 whiteScale = 1.0 / Uncharted2Curve(vec3(11.2));

    return curr * whiteScale;
}


/************************************
 *  ACES
 ************************************/

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
const mat3 ACESInputMat = mat3(
    0.59719, 0.07600, 0.02840,
    0.35458, 0.90834, 0.13383,
    0.04823, 0.01566, 0.83777
);

// ODT_SAT => XYZ => D60_2_D65 => sRGB
const mat3 ACESOutputMat = mat3(
    1.60475, -0.10208, -0.00327,
    -0.53108,  1.10813, -0.07276,
    -0.07367, -0.00605,  1.07602
);

vec3 RRTAndODTFit(vec3 v) {
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.432951) + 0.238081;
    return a / b;
}

vec3 ACESFilm(vec3 color) {
    color = ACESInputMat * color;
    color = RRTAndODTFit(color);
    color = ACESOutputMat * color;
    return clamp(color, 0.0, 1.0);
}


/************************************
 *  Khronos PBR Neutral
 ************************************/

vec3 KhronosPBRNeutral(vec3 color) {
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;

    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) return color;

    const float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(color, vec3(newPeak), g);
}


/************************************
 *  AgX
 ************************************/

const mat3 LINEAR_REC2020_TO_LINEAR_SRGB = mat3(
    1.6605, -0.1246, -0.0182,
    -0.5876, 1.1329, -0.1006,
    -0.0728, -0.0083, 1.1187
);

const mat3 LINEAR_SRGB_TO_LINEAR_REC2020 = mat3(
    0.6274, 0.0691, 0.0164,
    0.3293, 0.9195, 0.0880,
    0.0433, 0.0113, 0.8956
);

const mat3 AgXInsetMatrix = mat3(
    0.856627153315983, 0.137318972929847, 0.11189821299995,
    0.0951212405381588, 0.761241990602591, 0.0767994186031903,
    0.0482516061458583, 0.101439036467562, 0.811302368396859
);

const mat3 AgXOutsetMatrix = mat3(
    1.1271005818144368, -0.1413297634984383, -0.14132976349843826,
    -0.11060664309660323, 1.157823702216272, -0.11060664309660294,
    -0.016493938717834573, -0.016493938717834257, 1.2519364065950405
);

const float AgxMinEv = -12.47393;
const float AgxMaxEv = 4.026069;

vec3 agxCdl(vec3 color, vec3 slope, vec3 offset, vec3 power, float saturation) {
    color = LINEAR_SRGB_TO_LINEAR_REC2020 * color;

    // Inset
    color = AgXInsetMatrix * color;
    color = max(color, 1e-10);

    // Log2 space encoding
    color = clamp(log2(color), AgxMinEv, AgxMaxEv);
    color = (color - AgxMinEv) / (AgxMaxEv - AgxMinEv);
    color = clamp(color, 0.0, 1.0);

    // Sigmoid fit
    vec3 x2 = color * color;
    vec3 x4 = x2 * x2;
    color = + 15.5     * x4 * x2
            - 40.14    * x4 * color
            + 31.96    * x4
            - 6.868    * x2 * color
            + 0.4298   * x2
            + 0.1191   * color
            - 0.00232;

    // CDL Grading
    color = pow(color * slope + offset, power);
    const vec3 lw = vec3(0.2126, 0.7152, 0.0722);
    float luma = dot(color, lw);
    color = luma + saturation * (color - luma);

    color = AgXOutsetMatrix * color;
    color = pow(max(vec3(0.0), color), vec3(2.2));
    color = LINEAR_REC2020_TO_LINEAR_SRGB * color;
    color = clamp(color, 0.0, 1.0);

    return color;
}

vec3 AgX(vec3 color) {
    return agxCdl(color, vec3(1.0), vec3(0.0), vec3(1.0), 1.0);
}

vec3 AgXPunchy(vec3 color) {
    return agxCdl(color, vec3(1.0), vec3(0.0), vec3(1.35), 1.4);
}

void main() {
    vec3 color = texture(screenTexture, TexCoords).rgb;

    if (u_enableBloom) {
        vec3 bloom = texture(bloomTexture, TexCoords).rgb;
        color += bloom * u_bloomIntensity;
    }

    color *= exp2(u_exposure);

    if (u_tonemapper == 1) {
        color = Reinhard(color);
    } else if (u_tonemapper == 2) {
        color = Uncharted2(color);
    } else if (u_tonemapper == 3) {
        color = ACESFilm(color);
    } else if (u_tonemapper == 4) {
        color = KhronosPBRNeutral(color);
    } else if (u_tonemapper == 5) {
        color = AgX(color);
    } else if (u_tonemapper == 6) {
        color = AgXPunchy(color);
    }

    FragColor = vec4(color, 1.0);
}