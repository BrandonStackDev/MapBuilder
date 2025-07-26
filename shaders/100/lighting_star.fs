#version 100

// GLSL ES 1.00
precision mediump float;

#define MAX_LIGHTS 4

uniform vec3 lightColor[MAX_LIGHTS];
uniform vec3 lightPosition[MAX_LIGHTS];
uniform int lightEnabled[MAX_LIGHTS];
uniform int lightType[MAX_LIGHTS];

varying vec3 fragPosition;
uniform float u_time;
varying float blink;
varying float instanceId;

void main()
{
    //if (blink < 0.1) discard; // optional, can use smooth fade instead
    // vec3 starColor = vec3(
    //     0.5 + 0.5 * sin(instanceId * 3.1),
    //     0.5 + 0.5 * sin(instanceId * 2.3 + 2.0),
    //     0.5 + 0.5 * sin(instanceId * 1.7 + 4.0)
    // );

    gl_FragColor = vec4(
        0.5 + 0.5 * sin(u_time + instanceId * 3.1), 
        0.5 + 0.5 * sin(u_time + instanceId * 2.3 + 2.0), 
        0.5 + 0.5 * sin(u_time + instanceId * 1.7 + 4.0), 
        1.0
    ); // or replace with star color
    //gl_FragColor = vec4(starColor[0], starColor[1], starColor[2], 1.0); // or replace with star color

    // vec3 finalColor = vec3(0.0);
    // for (int i = 0; i < MAX_LIGHTS; i++) {
    //     if (lightEnabled[i] == 1) {
    //         finalColor += lightColor[i];
    //     }
    // }

    // gl_FragColor = vec4(finalColor * blink, 1.0);
}


