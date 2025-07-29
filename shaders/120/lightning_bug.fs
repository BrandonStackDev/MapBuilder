#version 120

varying vec4 fragColor;
varying float fragBlink;

void main()
{
    vec3 baseColor = vec3(1.0, 1.0, 0.2); // yellowish
    vec3 finalColor = baseColor * fragBlink;

    gl_FragColor = vec4(finalColor, fragBlink); // control alpha as well
}
