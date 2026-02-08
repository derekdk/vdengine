#version 450

// Maximum number of lights (must match Types.h MAX_LIGHTS)
#define MAX_LIGHTS 8

// Light types (must match LightType enum)
#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2

// Inputs from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec3 fragWorldNormal;
layout(location = 4) in vec3 fragViewPos;

// Push constants for material properties
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 albedo;        // RGB albedo + opacity
    vec4 emission;      // RGB emission + intensity
    float roughness;
    float metallic;
    float normalStrength;
    float padding;
} material;

// GPU Light structure (must match GPULight in Types.h)
struct GPULight {
    vec4 positionAndType;   // xyz = position/direction, w = type
    vec4 directionAndRange; // xyz = direction, w = range
    vec4 colorAndIntensity; // xyz = color, w = intensity
    vec4 spotParams;        // x = inner angle cos, y = outer angle cos
};

// Lighting UBO (Set 1, Binding 0)
layout(set = 1, binding = 0) uniform LightingUBO {
    vec4 ambientColorAndIntensity;  // xyz = color, w = intensity
    ivec4 lightCounts;              // x = num lights
    GPULight lights[MAX_LIGHTS];
} lighting;

// Output color
layout(location = 0) out vec4 outColor;

// Calculate attenuation for point/spot lights
float calculateAttenuation(float distance, float range) {
    // Smooth attenuation that reaches zero at range
    float attenuation = clamp(1.0 - (distance / range), 0.0, 1.0);
    return attenuation * attenuation;
}

// Calculate spotlight intensity
float calculateSpotlight(vec3 lightDir, vec3 spotDir, float innerCos, float outerCos) {
    float theta = dot(lightDir, normalize(-spotDir));
    float epsilon = innerCos - outerCos;
    return clamp((theta - outerCos) / epsilon, 0.0, 1.0);
}

// Simple Blinn-Phong lighting calculation
vec3 calculateLighting(vec3 normal, vec3 viewDir, vec3 albedo) {
    int numLights = lighting.lightCounts.x;
    
    // Ambient contribution
    vec3 ambient = lighting.ambientColorAndIntensity.rgb * 
                   lighting.ambientColorAndIntensity.w * albedo;
    
    vec3 totalDiffuse = vec3(0.0);
    vec3 totalSpecular = vec3(0.0);
    
    // Calculate contribution from each light
    for (int i = 0; i < numLights && i < MAX_LIGHTS; i++) {
        GPULight light = lighting.lights[i];
        int lightType = int(light.positionAndType.w);
        vec3 lightColor = light.colorAndIntensity.rgb;
        float intensity = light.colorAndIntensity.w;
        
        vec3 lightDir;
        float attenuation = 1.0;
        
        if (lightType == LIGHT_DIRECTIONAL) {
            // Directional light - direction is stored in position field
            lightDir = normalize(-light.positionAndType.xyz);
        }
        else if (lightType == LIGHT_POINT) {
            // Point light
            vec3 lightPos = light.positionAndType.xyz;
            vec3 toLight = lightPos - fragWorldPos;
            float distance = length(toLight);
            lightDir = toLight / distance;
            attenuation = calculateAttenuation(distance, light.directionAndRange.w);
        }
        else if (lightType == LIGHT_SPOT) {
            // Spot light
            vec3 lightPos = light.positionAndType.xyz;
            vec3 toLight = lightPos - fragWorldPos;
            float distance = length(toLight);
            lightDir = toLight / distance;
            attenuation = calculateAttenuation(distance, light.directionAndRange.w);
            
            // Apply spotlight cone
            float spotFactor = calculateSpotlight(lightDir, light.directionAndRange.xyz,
                                                  light.spotParams.x, light.spotParams.y);
            attenuation *= spotFactor;
        }
        
        // Skip if attenuation is negligible
        if (attenuation < 0.001) continue;
        
        // Diffuse (Lambertian)
        float NdotL = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = NdotL * lightColor * intensity * attenuation;
        
        // Specular (Blinn-Phong)
        vec3 halfDir = normalize(lightDir + viewDir);
        float NdotH = max(dot(normal, halfDir), 0.0);
        // Roughness affects specular power: rough = low power, smooth = high power
        float shininess = mix(256.0, 8.0, material.roughness);
        float spec = pow(NdotH, shininess);
        // Metallic surfaces reflect light color, dielectrics reflect white
        vec3 specularColor = mix(vec3(0.04), albedo, material.metallic);
        vec3 specular = spec * specularColor * lightColor * intensity * attenuation;
        
        totalDiffuse += diffuse;
        totalSpecular += specular;
    }
    
    // Combine all lighting
    vec3 finalColor = ambient + (totalDiffuse * albedo) + totalSpecular;
    
    // Add emission
    finalColor += material.emission.rgb * material.emission.w;
    
    return finalColor;
}

void main() {
    // Calculate normal from screen-space gradients if not provided
    vec3 normal;
    if (length(fragWorldNormal) < 0.01) {
        // Derive normal from position gradients
        vec3 dFdxPos = dFdx(fragWorldPos);
        vec3 dFdyPos = dFdy(fragWorldPos);
        normal = normalize(cross(dFdxPos, dFdyPos));
    } else {
        normal = normalize(fragWorldNormal);
    }
    
    // View direction (from fragment to camera)
    vec3 viewDir = normalize(fragViewPos - fragWorldPos);
    
    // Get albedo color from material (or use vertex color as fallback)
    vec3 albedo = material.albedo.rgb;
    if (length(albedo) < 0.01) {
        albedo = fragColor;
    }
    
    // Calculate final lit color
    vec3 litColor = calculateLighting(normal, viewDir, albedo);
    
    // Output with opacity
    outColor = vec4(litColor, material.albedo.a);
}
