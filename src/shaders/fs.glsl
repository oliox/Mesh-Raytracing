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

vec3 ro;
vec3 rd;

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

layout(std430, binding = 1) buffer BVHBuffer {
    BVHNode nodes[];
};

//slab method
bool rayAABBIntersect(
    vec3 ro,
    vec3 rd,
    vec3 bmin,
    vec3 bmax
) {
    vec3 invDir = 1.0 / rd;

    vec3 t0 = (bmin - ro) * invDir;
    vec3 t1 = (bmax - ro) * invDir;

    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);

    float tNear = max(max(tmin.x, tmin.y), tmin.z);
    float tFar  = min(min(tmax.x, tmax.y), tmax.z);

    // tHit = tNear;
    return tFar >= max(tNear, 0.0);
}

//Möller–Trumbore
bool rayTriangleIntersect(
    vec3 orig,
    vec3 dir,
    vec3 v0,
    vec3 v1,
    vec3 v2,
    out float tHit,
    out vec3 hitPos
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
    if (t <= 0.0)
        return false;

    tHit = t;
    hitPos = orig + t * dir;
    return true;
}

// return (pos, triindex)
// vec2 closestHitFromBVH(BVHNode b) {
//     if (rayAABBIntersect(
//                 ro,
//                 rd,
//                 b.boundsMin.xyz,
//                 b.boundsMax.xyz
//             )) {
//                 if (b.left == -1 && b.right == -1) {
//                     // return triangle intersections
//                     // there should only be 1 triangle at this level anyway
//                     for (int i = 0; i < b.triCount; i++) {
//                         Triangle tri = triangles[b.firstTri + i];
//                         float t;
//                         vec3 p;
//                         if (rayTriangleIntersect(ro,
//                                 rd,
//                                 tri.v0.xyz,
//                                 tri.v1.xyz,
//                                 tri.v2.xyz, t, p)) {
//                                     return vec2(t, float(b.firstTri + i));
//                                 }
                                
//                     }
//                     return vec2(10000.0, -1);
//                 }
//                 if (b.left == -1) {
//                     BVHNode right = nodes[b.right];
//                     return closestHitFromBVH(right);
//                 }
//                 if (b.right == -1) {
//                     BVHNode left = nodes[b.left];
//                     return closestHitFromBVH(left);
//                 }
//                 BVHNode right = nodes[b.right];
//                 BVHNode left = nodes[b.left];
  
//                 vec2 leftHit = closestHitFromBVH(left);
//                 vec2 rightHit = closestHitFromBVH(right);
//                 if (leftHit.x < rightHit.x) {
//                     return leftHit;
//                 } else {
//                     return rightHit;
//                 }
//             } else {
//                 return vec2(10000.0, -1);
//             }
// }

vec2 closestHitFromBVH_iterative()
{
    const int MAX_STACK_SIZE = 64;

    int stack[MAX_STACK_SIZE];
    int stackPtr = 0;

    // push root node index
    stack[stackPtr++] = 0;

    float closestT = 1e30;
    int closestTri = -1;

    while (stackPtr > 0)
    {
        int nodeIndex = stack[--stackPtr];
        BVHNode node = nodes[nodeIndex];

        // AABB test
        if (!rayAABBIntersect(
                ro,
                rd,
                node.boundsMin.xyz,
                node.boundsMax.xyz))
        {
            continue;
        }

        // Leaf node
        if (node.left == -1 && node.right == -1)
        {
            for (int i = 0; i < node.triCount; i++)
            {
                Triangle tri = triangles[node.firstTri + i];

                float t;
                vec3 hitPos;
                if (rayTriangleIntersect(
                        ro,
                        rd,
                        tri.v0.xyz,
                        tri.v1.xyz,
                        tri.v2.xyz,
                        t,
                        hitPos))
                {
                    if (t < closestT)
                    {
                        closestT = t;
                        closestTri = node.firstTri + i;
                    }
                }
            }
        }
        else
        {
            // Push children
            // (order doesn't matter, but near-first is an optimization)
            if (node.left != -1)
                stack[stackPtr++] = node.left;

            if (node.right != -1)
                stack[stackPtr++] = node.right;

            // Safety guard
            if (stackPtr >= MAX_STACK_SIZE)
                break;
        }
    }

    return vec2(closestT, float(closestTri));
}


void main() {
    // 0 = bvh raytraced, 1 = naive raytraced, 2 = bvh visual
    int viewMode = 2;
    // Triangle tri = triangles[i];
    // vec3 rayDir = normalize(
    //     (invViewProj * vec4(uv * 2.0 - 1.0, 1.0, 1.0)).xyz
    // );
    ro = (MVP*vec4(uv.x-0.5, uv.y-0.5, -1.0, 0.0)).xyz;
    rd = (MVP*vec4(0.0, 0.0, 1.0, 0.0)).xyz;
    
    bool hit = false;
    if (viewMode == 0) {
        fragment = vec4(1.0, 0.05, 0.05, 1.0);
        BVHNode b0 = nodes[0];
        vec2 closestHit = closestHitFromBVH_iterative();
        if (closestHit.y == -1) {
            //missed
            fragment = vec4(0.0, 0.0, 0.0, 1.0);
        } else {
            //hit, shade by normal
            Triangle t = triangles[int(closestHit.y)];
            vec3 hitNormal = normalize(t.normal.xyz);
            fragment = vec4(hitNormal * 0.5 + 0.5, 1.0);
        }
        return;
    }  
    if (viewMode == 1) {

        vec3 hitNormal = vec3(0.0);

        float closestT = 10000;
        for (int i = 0; i < triangles.length(); i++) {
            Triangle tri = triangles[i];

            vec3 p;
            float t;
            if (rayTriangleIntersect(
                    ro,
                    rd,
                    tri.v0.xyz,
                    tri.v1.xyz,
                    tri.v2.xyz, t, p))
            {
                // If you later modify the function to return t,
                // you can do proper depth sorting here.
                hit = true;
                if (t < closestT) {
                    closestT = t;
                    hitNormal = normalize(tri.normal.xyz);
                }
            }
        }

        if (hit) {
            // Simple normal-based shading
            // fragment = vec4(closestT, 1.0);
            fragment = vec4(hitNormal * 0.5 + 0.5, 1.0);
        } else {
            fragment = vec4(0.0, 0.0, 0.0, 1.0);
        }
        return;
    } 
    if (viewMode == 2) {

        float closestT = 1e30;
        
        // fragment = vec4(1.0, 1.0, 0.05, 1.0);
        // fragment = vec4(nodes[0].boundsMin.y+0.5, nodes[0].boundsMax.y+0.5, 0.0, 1.0);
        float hitCounter = 0.0;
        for (int i = 0; i < nodes.length(); ++i) {
            BVHNode node = nodes[i];

            // Only visualize leaves
            if (node.left != -1 || node.right != -1)
                continue;

            if (rayAABBIntersect(
                ro,
                rd,
                node.boundsMin.xyz,
                node.boundsMax.xyz
            )) {
                // if (tHit < closestT) {
                //     closestT = tHit;
                //     hit = true;
                // }
                hitCounter += 0.05;
                hit = true;
            }
        }

        if (hit) {
            fragment = vec4(hitCounter, 0, 0.0, 1.0); // green leaf boxes
        } else {
            fragment = vec4(0.05, 0.05, 0.05, 1.0);
        }
        return;
    }
    // fragment = vec4(1.0, 0.05, 0.05, 1.0);
    // BVHNode node = nodes[nodeIndex];
}