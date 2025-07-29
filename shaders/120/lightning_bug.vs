#version 120

// GLSL ES 2.0 (OpenGL ES 2.0)

attribute vec3 vertexPosition;
attribute vec4 vertexColor;

attribute mat4 instanceTransform;
attribute vec4 instanceBlink;

uniform mat4 mvp;
uniform float u_time;

varying vec4 fragColor;
varying float fragBlink;

void main()
{
    fragColor = vertexColor;
    fragBlink = instanceBlink.x;

    vec4 worldPosition = instanceTransform * vec4(vertexPosition, 1.0);
    fragBlink = sin(instanceTransform[3].x * 1.21 + u_time * 5.0) * 0.5 + 0.5;

    gl_Position = mvp * worldPosition;
}

