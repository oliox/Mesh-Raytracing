// #version 330
// in vec3 FragNormal;
// out vec4 fragment;
// void main()
// {
//     fragment = vec4(normalize(FragNormal), 1.0);
// }

#version 430 core

in vec2 uv;
uniform mat4 invViewProj;
uniform vec3 cameraPos;
out vec4 fragment;

struct Triangle {
    vec4 v0;
    vec4 v1;
    vec4 v2;
    vec4 normal;
};

struct BVHNode {
    vec4 boundsMin;
    vec4 boundsMax;
    int left;
    int right;
    int firstTri;
    int triCount;
};

layout(std430, binding = 0) buffer TriangleBuffer {
    Triangle triangles[];
};

// layout(std430, binding = 1) buffer BVHBuffer {
//     BVHNode nodes[];
// };

void main() {
    Triangle tri = triangles[i];
    vec3 rayDir = normalize(
        (invViewProj * vec4(uv * 2.0 - 1.0, 1.0, 1.0)).xyz
    );
    fragment = vec4(u,v,0.0, 1.0);
    // BVHNode node = nodes[nodeIndex];
}