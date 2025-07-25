#version 120

attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;

uniform mat4 mvp;
uniform float uTime;
uniform vec2 worldOffset;

varying vec2 fragUV;
varying float heightOffset;

void main() {
    vec3 pos = vertexPosition;
    // Use global position
    float gx = pos.x + worldOffset.x;
    float gz = pos.z + worldOffset.y;

    // Base wave
    float wave = sin((gx + uTime) * 4.0) * cos((gz + uTime) * 4.0);

    // Additional details - make sure these use global coordinates too
    wave += sin((gx - gz - uTime * 0.5) * 4.0);
    wave += cos((gx * 2.0 + gz * 2.0 + uTime) * 1.5);
    wave += sin((gx - (gz*gz) - uTime * 0.8) * 7.0);
    wave += cos((gx * 0.013 + gz * 0.0011 + uTime) * 1.5);

    // Scale and apply to y-position
    pos.y += wave * 0.5;

    fragUV = vertexTexCoord;
    heightOffset = pos.y-295;

    gl_Position = mvp * vec4(pos, 1.0);
}
