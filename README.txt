Instructions to run:

unzip external.zip
It should contain the following folders
assimp-master
glad
glfw-3.4

Update absolute filepaths for shaders on line 411 and 412 of main.cpp
Update absolute filepaths of input meshes line 270

To compile:
From the root directory of the project
mkdir build
cd build
cmake ..
