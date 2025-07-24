#version 120

attribute vec3 vertexPosition;
attribute vec3 vertexNormal;
attribute vec2 vertexTexCoord;

uniform mat4 mvp;
uniform mat4 model;
uniform vec3 lightDir;

varying vec3 normal;
varying vec3 worldPos;
varying float height;
varying float vSlope;
varying vec2 texCoord;

void main()
{
    // Pass texture coordinate
    texCoord = vertexTexCoord;

    vec4 worldPosition = model * vec4(vertexPosition, 1.0);
    
    normal = normalize(mat3(model) * vertexNormal);
    worldPos = worldPosition.xyz;

    // Calculate slope using normal's Y (1 = flat, 0 = steep vertical)
    float slope = 1.0 - abs(vertexNormal.y);
    vSlope = slope;

    height = worldPosition.y; // Pass height for height-based shading
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
