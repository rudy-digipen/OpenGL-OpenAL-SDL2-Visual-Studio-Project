# Starter C++ Project

This project setups Visual Studio so that we have access to SDL2, OpenGL, GLEW, OpenAL, DearImGui, stb image & vorbis, glm, gsl

**SDL2**      - Makes Windows, Provides OpenGL Contexts, Load WAV audio files, OS wrapper
https://github.com/libsdl-org/SDL/releases/download/release-2.30.6/SDL2-devel-2.30.6-VC.zip

**OpenGL**    - HW accelerated graphics
        + We have to get the functions pointers at runtime
        + Still need OS specific lib file for linker

**GLEW**      - Automates get the OpenGL function pointers
https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0-win32.zip

**OpenAL**    - Cross platform specification for Audio intended for game projects
        + allows basic and 3D audio simulation (more effects with extensions)
https://github.com/kcat/openal-soft/releases/download/1.23.1/openal-soft-1.23.1-bin.zip

**DearImGui** - Real-time UI that is interactive. Super good for Dev and Debug tools. Shouldn't be used for User facing UI.
https://github.com/ocornut/imgui/tree/docking

**GLM**       - Math library that mimics how we do math in GLSL
https://github.com/g-truc/glm/releases/download/1.0.1/glm-1.0.1-light.zip

**gsl**       - Guideline Support Library
https://github.com/microsoft/GSL/archive/refs/tags/v4.0.0.zip

**stb**       - Collection of header only* library files
            + stb_image    -> png, other formats
            + stb_vorbis.c -> ogg files
https://github.com/nothings/stb (get the stb_image.h and stb_vorbis.c)
