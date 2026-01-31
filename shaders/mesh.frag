#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Simple lighting: directional light from above
    vec3 lightDir = normalize(vec3(0.3, 1.0, 0.5));
    
    // Approximate normal from world position gradients (for smooth shading)
    vec3 dFdxPos = dFdx(fragWorldPos);
    vec3 dFdyPos = dFdy(fragWorldPos);
    vec3 normal = normalize(cross(dFdxPos, dFdyPos));
    
    // Simple diffuse lighting
    float diffuse = max(dot(normal, lightDir), 0.0);
    float ambient = 0.3;
    float lighting = ambient + diffuse * 0.7;
    
    // Apply lighting to vertex color
    vec3 litColor = fragColor * lighting;
    
    outColor = vec4(litColor, 1.0);
}
