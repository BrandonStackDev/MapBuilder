#version 120

uniform vec3 cameraPosition;
uniform sampler2D texture0;

varying float vSlope;
varying vec2 texCoord;


void main()
{
    // Sample base texture color
    vec4 texColor = texture2D(texture0, texCoord);

    // Create grayscale based on slope
    float tint = vSlope * 0.5; // reduce strength to make it gentle
    vec3 grayTint = vec3(tint);

    // Blend tint onto texture (soft light mix)
    vec3 finalColor = mix(texColor.rgb, texColor.rgb + grayTint, 0.5);

    gl_FragColor = vec4(finalColor, 1.0);
}
