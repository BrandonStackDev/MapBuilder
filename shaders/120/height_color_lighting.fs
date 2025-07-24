#version 120

uniform vec3 lightDir;
uniform vec3 cameraPosition;
uniform sampler2D texture0;

varying vec3 normal;
varying vec3 worldPos;
varying float height;
varying float vSlope;
varying vec2 texCoord;

void main()
{
    // Simple height-based color mapping
    // Sample base texture color
    vec4 texColor = texture2D(texture0, texCoord);

    // Create grayscale based on slope
    float tint = vSlope * 0.5; // reduce strength to make it gentle
    vec3 grayTint = vec3(tint);

    // Blend tint onto texture (soft light mix)
    vec3 finalColor = mix(texColor.rgb, texColor.rgb + grayTint, 0.5);



    // Basic diffuse lighting
    vec3 norm = normalize(normal);
    vec3 light = normalize(lightDir);
    float diffuse = dot(norm, -light); //max(dot(norm, -light), 0.0);  // Use negative if lightDir is TO light
    //float diffuse = max(dot(normalize(normal), -normalize(lightDir)), 0.0);
    vec3 color = finalColor * diffuse;

    gl_FragColor = vec4(color, 1.0);
}
