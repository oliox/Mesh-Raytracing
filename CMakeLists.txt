cmake_minimum_required(VERSION 3.20)
project(MeshRaytracing)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add your include folder
include_directories(${CMAKE_SOURCE_DIR}/include)
# -----------------------------
# Add GLAD
# -----------------------------
add_library(glad external/glad/src/gl.c)
target_include_directories(glad PUBLIC external/glad/include)

# -----------------------------
# Add GLFW
# -----------------------------
add_subdirectory(external/glfw-3.4)
add_subdirectory(external/assimp-master)

# -----------------------------
# Your Executable
# -----------------------------
add_executable(mesh_rt
    src/main.cpp
)

# -----------------------------
# Link everything
# -----------------------------
target_link_libraries(mesh_rt
    PRIVATE
        glad
        glfw
        assimp
        
)

file(COPY ${CMAKE_SOURCE_DIR}/src/shaders DESTINATION ${CMAKE_BINARY_DIR})

# On Linux: also link dl
if(UNIX AND NOT APPLE)
    target_link_libraries(mesh_rt PRIVATE dl)
endif()

# -----------------------------
# Include directories
# -----------------------------
target_include_directories(mesh_rt
    PRIVATE
        external/glad/include
        external/glfw-3.4/include
)
