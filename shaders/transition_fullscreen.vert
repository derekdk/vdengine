#version 450

// Fullscreen triangle: 3 vertices, no vertex buffer needed.
// Covers the entire screen with a single triangle.

layout(location = 0) out vec2 fragUV;

void main() {
    // Fullscreen triangle trick: vertex 0=(0,0), 1=(2,0), 2=(0,2)
    fragUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(fragUV * 2.0 - 1.0, 0.0, 1.0);
    // Flip Y for Vulkan coordinate system
    fragUV.y = 1.0 - fragUV.y;
}
