#version 120

attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;

uniform mat4 mvp;
uniform float uTime;

varying vec2 fragUV;
varying float heightOffset;

void main() {
    vec3 pos = vertexPosition;
    float wave = sin((pos.x + pos.z + uTime) * 3.0);
    wave += sin((pos.x - pos.z - uTime * 0.5) * 4.0);
    wave += cos((pos.x * 2.0 + pos.z * 2.0 + uTime) * 1.5);
    pos.y += wave * 0.05;

    fragUV = vertexTexCoord;
    heightOffset = pos.y;

    gl_Position = mvp * vec4(pos, 1.0);
}
