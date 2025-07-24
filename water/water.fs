// water.fs
#version 120
varying vec2 fragTexCoord;

uniform float uTime;

void main() {
    // Scroll texture in time to simulate motion
    vec2 uv = fragTexCoord + vec2(uTime * 0.1, uTime * 0.05);

    // Simple water blue gradient
    float brightness = 0.5 + 0.5 * sin(uv.x * 10.0 + uTime);
    gl_FragColor = vec4(0.0, 0.3 + 0.2 * brightness, 0.6, 0.8);
}
