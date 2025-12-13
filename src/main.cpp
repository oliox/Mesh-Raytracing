#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
 
#include "linmath.h"
 
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
 
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags


static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}
 
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static std::string LoadFile(const char* path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "Failed to open shader file: " << path << std::endl;

    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool mouseDown = false;
double lastX = 0.0, lastY = 0.0;
float rotX = 0.0f;  // rotation around X axis
float rotY = 0.0f;  // rotation around Y axis

mat4x4 mvp;

 

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            mouseDown = true;
            // get the current cursor position so the first movement is smooth
            glfwGetCursorPos(window, &lastX, &lastY);
        }
        else if (action == GLFW_RELEASE)
        {
            mouseDown = false;
        }
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (mouseDown)
    {
        double dx = xpos - lastX;
        double dy = ypos - lastY;

        // 👉 This is where the “mouse drag action” happens
        // printf("Dragging: dx = %.2f, dy = %.2f\n", dx, dy);
        float sensitivity = 0.005f;

        rotY += (float)dx * sensitivity;   // horizontal drag rotates around Y axis
        rotX += (float)dy * sensitivity;   // vertical drag rotates around X axis


        // TODO: rotate camera, pan, move object, etc.

        lastX = xpos;
        lastY = ypos;
    }
}

void CheckShaderCompile(GLuint shader, const char* name)
{
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

        std::string log(logLength, '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());

        std::cerr << "Shader compile error (" << name << "):\n";
        std::cerr << log << std::endl;
    }
}

struct MeshVertex {
    float position[3];
    float normal[3];
    float uv[2];
};

// array of triangles
// array of boxes -> pointer to children, list of faces in them

// top level A is all faces
// build child -> pass through faces and put into either left right or both
// nlogn
//split over largest axis
// build in xyz axes
//if triangle is on border, always assign it to first, then expand first to overlap second so it fully covers those triangles

struct alignas(16) Triangle {
    float v0[4];   // xyz + padding
    float v1[4];
    float v2[4];
    float normal[4];
};

struct alignas(16) BVHNode {
    float boundsMin[4]; // xyz + padding
    float boundsMax[4];
    int left;
    int right;
    int firstTri;
    int triCount;
};


int main(void)
{

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile("C:/Users/oliox/Documents/Code/Mesh-Raytracing/meshes/closed/cube.obj", 
                                            aiProcess_Triangulate | 
                                            aiProcess_FlipUVs | 
                                            aiProcess_GenNormals);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            printf("error loading the model sad");
        }else {
        printf("loaded the model happy");
        }

        aiMesh* mesh = scene->mMeshes[0]; // load first mesh

    // // save mesh data onto cpu
    // std::vector<MeshVertex> vertices;
    // std::vector<unsigned int> indices;

    // vertices.reserve(mesh->mNumVertices);

    // for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    // {
    //     MeshVertex v;

    //     v.position[0] = mesh->mVertices[i].x;
    //     v.position[1] = mesh->mVertices[i].y;
    //     v.position[2] = mesh->mVertices[i].z;

    //     v.normal[0] = mesh->mNormals[i].x;
    //     v.normal[1] = mesh->mNormals[i].y;
    //     v.normal[2] = mesh->mNormals[i].z;

    //     if (mesh->mTextureCoords[0]) {
    //         v.uv[0] = mesh->mTextureCoords[0][i].x;
    //         v.uv[1] = mesh->mTextureCoords[0][i].y;
    //     } else {
    //         v.uv[0] = v.uv[1] = 0.0f;
    //     }

    //     vertices.push_back(v);
    // }

    // indices.reserve(mesh->mNumFaces * 3);

    // for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    // {
    //     aiFace& face = mesh->mFaces[i];
    //     indices.push_back(face.mIndices[0]);
    //     indices.push_back(face.mIndices[1]);
    //     indices.push_back(face.mIndices[2]);
    // }

    glfwSetErrorCallback(error_callback);
 
    if (!glfwInit())
        exit(EXIT_FAILURE);
 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
 
    GLFWwindow* window = glfwCreateWindow(640, 480, "OpenGL Triangle", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
 
    glfwSetKeyCallback(window, key_callback);
 
    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);
    glfwSwapInterval(1);
 
    // NOTE: OpenGL error checks have been omitted for brevity
    std::vector<Triangle> triangles;
    std::vector<BVHNode> bounding_volumes;

    triangles.reserve(mesh->mNumFaces);
    bounding_volumes.reserve(100); // todo
    //build triangles
    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        Triangle t;

        aiFace& face = mesh->mFaces[i];
        if (face.mNumIndices != 3)
            continue;
        unsigned int i0 = face.mIndices[0];
        unsigned int i1 = face.mIndices[1];
        unsigned int i2 = face.mIndices[2];
        t.v0[0] = mesh->mVertices[i0].x;
        t.v0[1] = mesh->mVertices[i0].y;
        t.v0[2] = mesh->mVertices[i0].z;
        t.v1[0] = mesh->mVertices[i1].x;
        t.v1[1] = mesh->mVertices[i1].y;
        t.v1[2] = mesh->mVertices[i1].z;
        t.v2[0] = mesh->mVertices[i2].x;
        t.v2[1] = mesh->mVertices[i2].y;
        t.v2[2] = mesh->mVertices[i2].z;

        printf("%f\n", t.v0[0]);

        aiVector3D p0 = mesh->mVertices[i0];
        aiVector3D p1 = mesh->mVertices[i1];
        aiVector3D p2 = mesh->mVertices[i2];

        // edges
        aiVector3D e1 = p1 - p0;
        aiVector3D e2 = p2 - p0;

        // face normal
        aiVector3D normal = e1 ^ e2;  // cross product
        normal.Normalize();
        t.normal[0] = normal[0];
        t.normal[1] = normal[1];
        t.normal[2] = normal[2];

        triangles.push_back(t);
    }
    //build them

    GLuint triangleSSBO;
    glGenBuffers(1, &triangleSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, triangleSSBO);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        triangles.size() * sizeof(Triangle),
        triangles.data(),
        GL_STATIC_DRAW
    );
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, triangleSSBO);

    // GLuint bvhSSBO;
    // glGenBuffers(1, &bvhSSBO);
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhSSBO);
    // glBufferData(
    //     GL_SHADER_STORAGE_BUFFER,
    //     bvhNodes.size() * sizeof(BVHNode),
    //     bvhNodes.data(),
    //     GL_STATIC_DRAW
    // );

    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bvhSSBO);
    //todo somehow I have to specify where in the object the elements are
 
//     GLuint vao, vbo, ebo;
//     glGenVertexArrays(1, &vao);
//     glGenBuffers(1, &vbo);
//     glGenBuffers(1, &ebo);

//     glBindVertexArray(vao);

//     // VBO
//     glBindBuffer(GL_ARRAY_BUFFER, vbo);
//     glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(MeshVertex), vertices.data(), GL_STATIC_DRAW);

//     // EBO
//     glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
// glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
 
    std::string vertexShaderCode = LoadFile("C:/Users/oliox/Documents/Code/Mesh-Raytracing/src/shaders/vs.glsl");
    std::string fragmentShaderCode = LoadFile("C:/Users/oliox/Documents/Code/Mesh-Raytracing/src/shaders/fs.glsl");

    
    const char* vertex_shader_text = vertexShaderCode.c_str();
    const char* fragment_shader_text = fragmentShaderCode.c_str();

    const GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
    glCompileShader(vertex_shader);
 
    const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
    glCompileShader(fragment_shader);
    CheckShaderCompile(fragment_shader, "FRAGMENT");
 
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
 
    const GLint mvp_location = glGetUniformLocation(program, "MVP");
    // const GLint vpos_location = glGetAttribLocation(program, "vPos");
    // const GLint vcol_location = glGetAttribLocation(program, "vCol");
 
    // // position
    // glEnableVertexAttribArray(0);
    // glVertexAttribPointer(0,
    //                     3, GL_FLOAT, GL_FALSE,
    //                     sizeof(MeshVertex),
    //                     (void*)offsetof(MeshVertex, position));

    // // normal
    // glEnableVertexAttribArray(1);
    // glVertexAttribPointer(1,
    //                     3, GL_FLOAT, GL_FALSE,
    //                     sizeof(MeshVertex),
    //                     (void*)offsetof(MeshVertex, normal));

    // // uv
    // glEnableVertexAttribArray(2);
    // glVertexAttribPointer(2,
    //                     2, GL_FLOAT, GL_FALSE,
    //                     sizeof(MeshVertex),
    //                     (void*)offsetof(MeshVertex, uv));
 
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    


    while (!glfwWindowShouldClose(window))
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        const float ratio = width / (float) height;
 
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
 
        // mat4x4 m, p, mvp;
        // mat4x4_identity(m);
        // mat4x4_rotate_Z(m, m, (float) glfwGetTime());
        // mat4x4_ortho(p, -ratio, ratio, -1.f, 1.f, 1.f, -1.f);
        // mat4x4_mul(mvp, p, m);

        mat4x4 rotXmat, rotYmat, model;
        mat4x4_identity(rotXmat);
        mat4x4_identity(rotYmat);
        mat4x4_identity(model);
        mat4x4_identity(mvp);
        mat4x4_rotate_X(rotXmat, rotXmat, rotX);
        mat4x4_rotate_Y(rotYmat, rotYmat, rotY);

        // combine them
        mat4x4_mul(model, rotYmat, rotXmat);

        // create full MVP
        mat4x4_mul(mvp, model, mvp);
 
        glUseProgram(program);
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, (const GLfloat*) &mvp);
        GLuint emptyVAO;
        glGenVertexArrays(1, &emptyVAO);
        glBindVertexArray(emptyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
 
    glfwDestroyWindow(window);
 
    glfwTerminate();
    exit(EXIT_SUCCESS);
}

//pass triangles into fragment shader
// use bvh to segment which triangles to check
// calcualte intersection

