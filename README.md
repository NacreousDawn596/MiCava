# MicCava

basically just cava, but with input audio instead of output lol

coded using cpp GLFW3 (raw graphics stuff)

# Dependencies:
- glfw 3.2+
- glew
- alsa-lib
- alsa-utils
- jsoncpp
- mesa
- base-devel

# Compile:
edit the `config.json` however you want, then you can either:
```bash
source bin/activate
glfwc main.cpp
```
or just use the command by yourself:
```bash
cc `pkg-config --cflags glfw3` -o main.cpp.a main.cpp `pkg-config --static --libs glfw3` -lasound -lGL -lGLEW -lstdc++ -ldl -ljsoncpp
```
then execute it:
```
./main.cpp.a
```

# Enjoy!! 
*don't judge my poor coding skills, I don't usually practice C++ a lot + I've just read glfw3 documentation about 5days ago lol*