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
uniform mat4 MVP;
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

bool rayTriangleIntersect(
    vec3 orig,
    vec3 dir,
    vec3 v0,
    vec3 v1,
    vec3 v2
) {
    const float EPSILON = 1e-6;

    vec3 e1 = v1 - v0;
    vec3 e2 = v2 - v0;

    vec3 pvec = cross(dir, e2);
    float det = dot(e1, pvec);

    if (abs(det) < EPSILON)
        return false;

    float invDet = 1.0 / det;

    vec3 tvec = orig - v0;
    float u = dot(tvec, pvec) * invDet;
    if (u < 0.0 || u > 1.0)
        return false;

    vec3 qvec = cross(tvec, e1);
    float v = dot(dir, qvec) * invDet;
    if (v < 0.0 || u + v > 1.0)
        return false;

    float t = dot(e2, qvec) * invDet;
    return t > 0.0;
}

void main() {
    // Triangle tri = triangles[i];
    // vec3 rayDir = normalize(
    //     (invViewProj * vec4(uv * 2.0 - 1.0, 1.0, 1.0)).xyz
    // );
    vec3 ro = (MVP*vec4(uv.x-0.5, uv.y-0.5, -1.0, 0.0)).xyz;
    vec3 rd = (MVP*vec4(0.0, 0.0, 1.0, 0.0)).xyz;
    
    bool hit = false;
    float closestT = 1e30;
    vec3 hitNormal = vec3(0.0);

    for (int i = 0; i < triangles.length(); i++) {
        Triangle tri = triangles[i];

        float t;
        if (rayTriangleIntersect(
                ro,
                rd,
                tri.v0.xyz,
                tri.v1.xyz,
                tri.v2.xyz))
        {
            // If you later modify the function to return t,
            // you can do proper depth sorting here.
            hit = true;
            hitNormal = normalize(tri.normal.xyz);
        }
    }

    if (hit) {
        // Simple normal-based shading
        fragment = vec4(hitNormal * 0.5 + 0.5, 1.0);
    } else {
        fragment = vec4(0.0, 0.0, 0.0, 1.0);
    }
    // BVHNode node = nodes[nodeIndex];
}