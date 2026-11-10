#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUv;
layout(location=3) in float aMaterial;

uniform mat4 uView;
uniform mat4 uProjection;
out vec3 vWorld;
out vec3 vNormal;
out vec2 vUv;
flat out int vMaterial;

void main() {
    vWorld = aPosition;
    vNormal = aNormal;
    vUv = aUv;
    vMaterial = int(aMaterial + 0.5);
    gl_Position = uProjection * uView * vec4(aPosition, 1.0);
}
