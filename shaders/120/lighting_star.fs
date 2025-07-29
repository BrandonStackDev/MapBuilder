#version 120

varying vec3 fragPosition;
uniform float u_time;
varying float blink;
varying float instanceId;

void main()
{
    gl_FragColor = vec4(
        0.5 + 0.5 * sin(u_time + instanceId * 3.1), 
        0.5 + 0.5 * sin(u_time + instanceId * 2.3 + 2.0), 
        0.5 + 0.5 * sin(u_time + instanceId * 1.7 + 4.0), 
        1.0
    ); // or replace with star color
}


