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
        rotX -= (float)dy * sensitivity;   // vertical drag rotates around X axis


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


//A must be a list of triangles inside 
int buildBVHNode(std::vector<BVHNode> & bounding_volumes, std::vector<Triangle> & triangles, int firstTri, int numTri, aiVector3D min, aiVector3D max, int lastAxis, int failedSplits)
{
    // if (bounding_volumes.size() == 0) {
    // printf("this node min: %f %f %f \n", min.x, min.y, min[2]);
    // printf("this node max: %f %f %f \n", max.x, max.y, max[2]);
    // }
    BVHNode b;
    bounding_volumes.emplace_back(b);   // construct in-place
    int idx = bounding_volumes.size() - 1;
    // bounding_volumes[idx].boundsMin[0] = mesh->mAABB.mMin.x;
    // bounding_volumes[idx].boundsMin[1] = mesh->mAABB.mMin.y;
    // bounding_volumes[idx].boundsMin[2] = mesh->mAABB.mMin.z;
    // bounding_volumes[idx].boundsMax[0] = mesh->mAABB.mMax.x;
    // bounding_volumes[idx].boundsMax[1] = mesh->mAABB.mMax.y;
    // bounding_volumes[idx].boundsMax[2] = mesh->mAABB.mMax.z;

    // xLen = max.x-min.x;
    // yLen = max.y-min.y;
    // zLen = max.z-min.z;

    // std::vector<int> vec = {xLen, yLen, zLen};
    // auto max_it = std::max_element(vec.begin(), vec.end());

    // // Calculate the distance (index) from the beginning of the vector to the max element
    // int axis = static_cast<int>(std::distance(vec.begin(), max_it));
    //rotate through all 3 axes
    int axis = (lastAxis + 1) % 3;
    
    auto leftMin = min;
    auto leftMax = max;
    leftMax[axis] = (min[axis] + max[axis])/2;

    auto rightMin = min;
    auto rightMax = max;
    rightMin[axis] = (min[axis] + max[axis])/2;

    auto pivot = (min[axis] + max[axis])/2;
    //loop through triangles, if centerpoint(?) is less than pivot put in left otherwise right
    // store max overflow, expand left box size to cover that overflow
    if (numTri != 0 && numTri != 1) { 
        std::vector<Triangle> leftPartition;
        std::vector<Triangle> rightPartition;
        float maxPointInLeft = pivot;
        float minPointInRight = pivot;
        for (int i = firstTri; i < firstTri+numTri; i++) {
            auto tri = triangles[i];
            float pos = (tri.v0[axis] + tri.v1[axis] + tri.v2[axis])/3;
            // if triangle is on the boundary put into left side, then expand left side to fully cover
            // this might not terminate, better way could be to use midpoint and grow both right and left side
            if (pos < pivot) {
                leftPartition.push_back(triangles[i]);
                float maxVert = std::max(tri.v0[axis], std::max(tri.v1[axis], tri.v2[axis]));
                if (maxVert > maxPointInLeft) {
                    maxPointInLeft = maxVert;
                }
            } else {
                rightPartition.push_back(triangles[i]);
                float minVert = std::min(tri.v0[axis], std::min(tri.v1[axis], tri.v2[axis]));
                if (minVert < minPointInRight) {
                    minPointInRight = minVert;
                }
            }
        }
        //ensures all triangles are fully enclosed
        // this could leave triangles which are actually in other bounding boxes and not counted. But every triangle will exist in exactly one box per level and the array wont be messed with (i hope)
        if (maxPointInLeft > pivot) {
            leftMax[axis] = maxPointInLeft;
        }
        if (minPointInRight < pivot) {
            rightMin[axis] = minPointInRight;
        }
        //overwrite back into triangles
        for (int i = firstTri; i < firstTri+leftPartition.size(); i++) {
            triangles[i] = leftPartition[i-firstTri];
        }
        for (int i = firstTri+leftPartition.size(); i < firstTri+numTri; i++) {
            triangles[i] = rightPartition[i-firstTri-leftPartition.size()];
        }

        // Avoid infinite loop where it halves in the long axis but then a really long triangle in that axis just regrows it to the same size
        //this causes early termination though
        if (leftPartition.size() == numTri || rightPartition.size() == numTri) {
            if (failedSplits == 2) {
                bounding_volumes[idx].left = -1;
                bounding_volumes[idx].right = -1;
            } else {
                bounding_volumes[idx].left = buildBVHNode(bounding_volumes, triangles, firstTri, leftPartition.size(), leftMin, leftMax, axis, failedSplits + 1);
                bounding_volumes[idx].right = buildBVHNode(bounding_volumes, triangles, firstTri + leftPartition.size(), rightPartition.size(), rightMin, rightMax, axis, failedSplits + 1);
            }
        } else {
            bounding_volumes[idx].left = buildBVHNode(bounding_volumes, triangles, firstTri, leftPartition.size(), leftMin, leftMax, axis, 0);
            bounding_volumes[idx].right = buildBVHNode(bounding_volumes, triangles, firstTri + leftPartition.size(), rightPartition.size(), rightMin, rightMax, axis, 0);
        }

    } else {
        bounding_volumes[idx].left = -1;
        bounding_volumes[idx].right = -1;
    }
    //todo add termination condition
    bounding_volumes[idx].firstTri = firstTri;
    bounding_volumes[idx].triCount = numTri;
    bounding_volumes[idx].boundsMin[0] = min.x;
    bounding_volumes[idx].boundsMin[1] = min.y;
    bounding_volumes[idx].boundsMin[2] = min.z;
    bounding_volumes[idx].boundsMax[0] = max.x;
    bounding_volumes[idx].boundsMax[1] = max.y;
    bounding_volumes[idx].boundsMax[2] = max.z;
    return idx;
}

struct MeshVertex {
    float position[3];
    float normal[3];
    float uv[2];
};

void printBVHTriangles(BVHNode b) {
    for (int i = b.firstTri; i < b.firstTri + b.triCount; i++) {
        printf("%d ", i);
    }
    printf("\n");
}


int main(void)
{

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile("C:/Users/oliox/Documents/Code/Mesh-Raytracing/meshes/closed/camel_simple.obj", 
                                            aiProcess_Triangulate | 
                                            aiProcess_FlipUVs | 
                                            aiProcess_GenNormals |
                                            aiProcess_GenBoundingBoxes);

    aiMesh* mesh = scene->mMeshes[0]; // load first mesh
    // normalize mesh
    aiVector3D center = (mesh->mAABB.mMin + mesh->mAABB.mMax) * 0.5f;
    aiVector3D extent = mesh->mAABB.mMax - mesh->mAABB.mMin;

    float maxExtent = std::max(extent.x, std::max(extent.y, extent.z));
    float scale = 1.0f / maxExtent;   // fits into [-0.5, 0.5]

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
    {
        aiMesh* mesh = scene->mMeshes[m];

        for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
        {
            mesh->mVertices[v] -= center;
            mesh->mVertices[v] *= scale;
        }

        // Optional: renormalize normals (safe practice)
        if (mesh->HasNormals())
        {
            for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
            {
                mesh->mNormals[v].Normalize();
            }
        }
    }
    mesh->mAABB.mMin = (mesh->mAABB.mMin - center) / maxExtent;
    mesh->mAABB.mMax = (mesh->mAABB.mMax  - center) / maxExtent;


    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        printf("error loading the model sad");
    }else {
    printf("loaded the model happy");
    }


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
    bounding_volumes.reserve(500); // todo
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

        // printf("%f\n", t.v0[0]);

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

    //build bvh
    printf("%d \n", buildBVHNode(bounding_volumes, triangles, 0, triangles.size(), mesh->mAABB.mMin, mesh->mAABB.mMax, 0, 0));
    
    // BVHNode b0;
    // b0.boundsMin[0] = mesh->mAABB.mMin.x;
    // b0.boundsMin[1] = mesh->mAABB.mMin.y;
    // b0.boundsMin[2] = mesh->mAABB.mMin.z;
    // b0.boundsMax[0] = mesh->mAABB.mMax.x;
    // b0.boundsMax[1] = mesh->mAABB.mMax.y;
    // b0.boundsMax[2] = mesh->mAABB.mMax.z;
    // b0.firstTri = 0;

    // printBVHTriangles(bounding_volumes[0]);
    // printBVHTriangles(bounding_volumes[test.left]);
    // printBVHTriangles(bounding_volumes[test.right]);

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

    GLuint bvhSSBO;
    glGenBuffers(1, &bvhSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhSSBO);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        bounding_volumes.size() * sizeof(BVHNode),
        bounding_volumes.data(),
        GL_STATIC_DRAW
    );

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bvhSSBO);
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

