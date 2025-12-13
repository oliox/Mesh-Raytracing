#version 330 core

out vec2 uv;

void main()
{
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 pos = positions[gl_VertexID];
    uv = pos * 0.5 + 0.5;

    gl_Position = vec4(pos, 0.0, 1.0);
}

// #version 330 core

// layout(location = 0) in vec3 vPos;
// layout(location = 1) in vec3 vNormal;
// layout(location = 2) in vec2 vUV;

// uniform mat4 MVP;

// out vec3 FragNormal;
// out vec2 FragUV;
// out vec3 norm;

// void main() {
//     FragNormal = vNormal;
//     FragUV = vUV;
//     gl_Position = MVP * vec4(vPos, 1.0);
// }