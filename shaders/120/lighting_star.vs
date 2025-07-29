#version 120

attribute vec3 vertexPosition;

uniform mat4 mvp;
uniform float u_time;

attribute mat4 instanceTransform;

varying float blink;
varying float instanceId;

void main()
{
    instanceId = instanceTransform[3][3]; // this is mat.m12 from C
    //instanceTransform[3][1]=1;
    mat4 new_transform = instanceTransform;
    new_transform[3][3]=1.0;
    vec4 worldPos = new_transform * vec4(vertexPosition, 1.0);
    gl_Position = mvp * worldPos;
    

    //blink = 0.5 + 0.5 * sin(instanceId * 9.0 + u_time * 1.5);
    float wave = sin(instanceId * 9.0 + u_time * 1.5);
    blink = 0.75 + 0.25 * wave;  // Range now: 0.5 to 1.0
}

