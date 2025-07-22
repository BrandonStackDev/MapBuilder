#version 120

attribute vec3 vertexPosition;
attribute vec3 vertexNormal;
attribute vec2 vertexTexCoord;

uniform mat4 mvp;

varying float vSlope;
varying vec2 texCoord;

void main()
{
    // Pass texture coordinate
    texCoord = vertexTexCoord;

    // Calculate slope using normal's Y (1 = flat, 0 = steep vertical)
    float slope = 1.0 - abs(vertexNormal.y);
    vSlope = slope;

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
