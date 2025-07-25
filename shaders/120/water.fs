#version 120

varying vec2 fragUV;
varying float heightOffset;

uniform float uTime;

void main() {
    // UV scrolling
    vec2 uv = fragUV;
    uv.y += uTime * 0.05;

    // Simulate shimmer using sine wave
    float wave1 = sin(uv.x * 10.0 + uTime);
    float wave2 = sin(uv.y * 15.0 + uTime * 1.5);
    float brightness = 0.4 + 0.3 * (wave1 + wave2) * 0.5;

    // Fresnel-ish effect
    float fresnel = pow(1.0 - abs(heightOffset), 2.0);
    brightness += fresnel * 0.2;
    //brightness = 0.96;

    gl_FragColor = vec4(0.2, 0.4, 0.8, 0.7) * brightness; // semi-transparent water blue
}
