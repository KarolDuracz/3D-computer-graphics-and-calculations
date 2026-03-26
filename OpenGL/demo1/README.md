// 26-03-2026
<br /><br />
<b>There's nothing here except main.cpp ( and small models ), as the paths are hardcoded because the custom texture generator isn't working properly yet. But it's already a good place to develop it further and start learning. What you see in the pictures, I compiled it on Windows 8.1 and MSVC 2019.</b>

<h3>OpenGL - demo1 - Load OBJ files from Blender</h3>

A demo to explore some of the basic mechanisms that OpenGL offers. There's only support for Blender .obj files. There's also a parser, and an attempt to implement texture loading from MTL and FSH. FSH sometimes works for some models at the moment... sometimes not. MTL doesn't. That's why there's Custom Texture, which creates a BMP file, which can be painted and then loaded onto a selected model.
<br /><br />
<b>Everything needs some fine tuning, but this is a purely educational demo for exploring OpenCL and GPU computing.</b> THIS IS NOT A DEMO THAT SHOWS HOW TO DO SOMETHING LIKE THIS. It's purely educational for me. But it's okay enough that I'm sharing it here.
<br /><br />

> [!NOTE]
> You can load multiple models into a scene and select them from the GUI menu on the right. You can then change position and rotation. You can also control lighting. Click the buttons on the right in the menu. Select from the top Model > Load Obj . After launch, the first thing you see is a MessageBox window with an OBJ loading error. But press OK and then load models into a scene that way.


<h3>Setup and configuration - This is wrong but this is my setup method </h3>

1. GL.h - I downloaded this repo to get the headers https://github.com/anholt/mesa/tree/master/include/GL - #include <GL/gl.h> - #include <GL/glu.h>
2. There is no glu.h in this repo, but MSVC did not return any errors for me even though I have it in the header. (?)
3. Some DLL libraries are in my system32 like opengl32.dll etc.
4. As you can see MSVC loads libraries from #pragma comment(lib, "opengl32.lib"), #pragma comment(lib, "glu32.lib") etc. to compile and link
5. MSVC configuration project -> Configuration Properties -> C\C++ -> General -> ( field Additional Include Directories ) ->  C:\OpenGL\mesa-master\mesa-master\include [ A place for GL.h headers ]
6.MSVC configuration project -> Configuration Properties -> linker -> System -> SubSystem -> Windows/Subsystem
7. In configuration manager x86 in my case  (instead of x64)

<h3>How to use</h3>

1. Loading custom textures in this demo is done by pasting the models into the folder C:\Windows\Temp\f1carmodel
2. Then you do "Create Texture" which saves the model data and creates a base for a BMP texture which can be painted and loaded, as can be seen in the example of this car model below in the image.
3. In this menu below there is an option to load BMP (but it needs improvement)
4. MODEL CONTROL - Controls the model selected from the list on the right in the GUI menu. This means the currently selected model. You can control it with the arrow keys. The spacebar brakes.
5. On the right are the basic GUI buttons for controlling the manual position and rotation of models
6. In the lower left corner you can see help which shows keyboard shortcuts such as WSQE, i.e. moving the camera
7. To rotate the scene you do by holding the RIGHT MOUSE BUTTON.
8. Zoom in/out mouse wheel
9. If you want to return to the main stage camera, turn off object tracking in the MODEL CONTROL menu. ( disable follow camera / disable model control )

<h3>Other information</h3>

https://threejs.org/editor/ - Here you can create basic structures on the scene (or in Blender) and save the scene to an OBJ file.

![dump](https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenGL/demo1/326%20-%2026-03-2026%20-%20for%20development%20and%20learning.png?raw=true)

![dump](https://github.com/KarolDuracz/3D-computer-graphics-and-calculations/blob/main/OpenGL/demo1/325%20-%2026-03-2026%20-%20for%20development%20and%20learning.png?raw=true)
