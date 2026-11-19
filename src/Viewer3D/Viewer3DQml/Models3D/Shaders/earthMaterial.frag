VARYING vec2 uv;
VARYING float terrainHeight;

// Ref:https://github.com/minus34/cesium1/blob/master/Cesium/Shaders/Builtin/Functions/saturation.glsl
vec3 czm_saturation(vec3 rgb, float adjustment)
{
    // Algorithm from Chapter 16 of OpenGL Shading Language
    const vec3 W = vec3(0.2125, 0.7154, 0.0721);
    vec3 intensity = vec3(dot(rgb, W));
    return mix(intensity, rgb, adjustment);
}

void MAIN()
{
    float heightDelta = terrainHeight - taskHeight;
    if (cutTerrain && heightDelta > 0.0)
        discard;
    vec4 textureColor = texture(someTextureMap, uv);
    vec3 color = czm_saturation(textureColor.xyz, 1.5);
    if (showTaskHeight) {
        // Derivative-width contour remains readable at different camera scales.
        float contourWidth = max(fwidth(heightDelta), 0.25);
        float contour = 1.0 - smoothstep(0.0, contourWidth, abs(heightDelta));
        color = mix(color, vec3(1.0, 0.15, 0.05), contour * 0.85);
    }
    BASE_COLOR = vec4(color, 1.0);
}
