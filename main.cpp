#include "linmath.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <alsa/asoundlib.h>
#include <json/value.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <json/json.h>

std::vector<int16_t> captureAudioData(int durationInMilliseconds) {
    int err;
    int captureSize;
    int16_t *buffer; 

    snd_pcm_t *captureHandle;
    snd_pcm_hw_params_t *hwParams;

    const char *alsaDevice = "default";

    if ((err = snd_pcm_open(&captureHandle, alsaDevice, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        std::cerr << "Error opening ALSA capture: " << snd_strerror(err) << std::endl;
        return {}; 
    }

    if ((err = snd_pcm_hw_params_malloc(&hwParams)) < 0) {
        std::cerr << "Error allocating hardware parameter structure: " << snd_strerror(err) << std::endl;
        return {}; 
    }

    if ((err = snd_pcm_hw_params_any(captureHandle, hwParams)) < 0) {
        std::cerr << "Error initializing hardware parameter structure: " << snd_strerror(err) << std::endl;
        return {}; 
    }

    if ((err = snd_pcm_hw_params_set_access(captureHandle, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        std::cerr << "Error setting access type: " << snd_strerror(err) << std::endl;
        return {}; 
    }

    if ((err = snd_pcm_hw_params_set_format(captureHandle, hwParams, SND_PCM_FORMAT_S16_LE)) < 0) {
        std::cerr << "Error setting sample format: " << snd_strerror(err) << std::endl;
        return {}; 
    }

    unsigned int sampleRate = 44100; // Sample rate (e.g., 44.1 kHz)
    if ((err = snd_pcm_hw_params_set_rate_near(captureHandle, hwParams, &sampleRate, 0)) < 0) {
        std::cerr << "Error setting sample rate: " << snd_strerror(err) << std::endl;
        return {};
    }

    if ((err = snd_pcm_hw_params_set_channels(captureHandle, hwParams, 2)) < 0) {
        std::cerr << "Error setting channel count: " << snd_strerror(err) << std::endl;
        return {}; 
    }

    if ((err = snd_pcm_hw_params(captureHandle, hwParams)) < 0) {
        std::cerr << "Error setting hardware parameters: " << snd_strerror(err) << std::endl;
        return {}; 
    }

    captureSize = (sampleRate * durationInMilliseconds) / 1000 * 4; // 2 bytes per sample, 2 channels

    buffer = (int16_t *)malloc(captureSize);
    if (!buffer) {
        std::cerr << "Error allocating capture buffer." << std::endl;
        return {}; 
    }

    if ((err = snd_pcm_prepare(captureHandle)) < 0) {
        std::cerr << "Error preparing audio interface for capture: " << snd_strerror(err) << std::endl;
        free(buffer);
        snd_pcm_close(captureHandle);
        snd_pcm_hw_params_free(hwParams);
        return {}; 
    }

    std::vector<int16_t> audioData(captureSize / 2);
    int framesRead = snd_pcm_readi(captureHandle, audioData.data(), captureSize / 4);

    if (framesRead < 0) {
        std::cerr << "Error reading from audio interface: " << snd_strerror(framesRead) << std::endl;
        audioData.clear();
    } else {
        audioData.resize(framesRead * 2); 
    }

    free(buffer);
    snd_pcm_close(captureHandle);
    snd_pcm_hw_params_free(hwParams);

    return audioData;
}

class Rectangles {
public:
    Rectangles(int count, float margin) : count(count), margin(margin), height(0.00) {
        std::cout << "maximum margin must be lower than " << 1.0 / count << std::endl;
        if ((1.0 - margin * (count + 1)) / count > 0) {
            width = (1.0 - margin * (count + 1)) / count;
        } else {
            width = 0.8 / count;
            margin = -1 * (count * width) / (count + 1);
        }
        width = width * 4;
        init();
    }

    class Rectangle {
    public:
        GLuint VAO, VBO;
        Rectangle(float width, float height, float diff, float margin, int index)
            : width(width), height(height), diff(diff), margin(margin), index(index) {
            updateVertices();
        }

        void updateVertices() {
            float pheight = height / 2;
            float w = width;
            float d = margin <= 0 ? diff : diff * 2;
            // float d = diff;
            vertices = {
                -1.0f + d, -1.0f * pheight,
                -1.0f + d + w, -1.0f * pheight,
                -1.0f + d + w, pheight,

                -1.0f + d, -1.0f * pheight,
                -1.0f + d + w, pheight,
                -1.0f + d, pheight
            };
            rectangle = {
                { -1.0 + d, -1.0 * pheight },
                { -1.0 + d + w, pheight }
            };
        }

        void changeHeight(float newHeight) {
            this->height = newHeight;
            updateVertices();
        }

        float width;
        float height;
        float diff;
        float margin;
        int index;
        std::vector<float> vertices;
        std::vector<std::pair<float, float>> rectangle;
    };

    void init() {
        float tdiff = margin;
        for (int i = 0; i < count; i++) {
            float diff = tdiff;
            rectangles.emplace_back(width, height, diff, margin, i);
            tdiff += width + margin;
        }
    }

    std::vector<Rectangle>& getRectangles() {
        return this->rectangles;
    }

private:
    int count;
    float margin;
    float height;
    float width;
    std::vector<Rectangle> rectangles;
};

int main() {
    std::ifstream inputFile("config.json");

    if (!inputFile.is_open()) {
        std::cerr << "Failed to open the JSON file." << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << inputFile.rdbuf();
    std::string jsonStr = buffer.str();

    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errs;

    std::istringstream jsonStream(jsonStr);
    if (!Json::parseFromStream(builder, jsonStream, &root, &errs)) {
        std::cerr << "Failed to parse JSON: " << errs << std::endl;
        return 1;
    }

    int TILES;
    std::string Color;
    float Margin, WidthRatio, HeightRatio;

    if (root.isMember("TILES") && root.isMember("Color") && root.isMember("Margin"), root.isMember("WidthRatio"), root.isMember("HeightRatio")) {
        TILES = root["TILES"].asInt();
        Color = root["Color"].asString();
        Margin = root["Margin"].asFloat();
        WidthRatio= 100.0/root["WidthRatio"].asFloat();
        HeightRatio = 100.0/root["HeightRatio"].asFloat();
    } else {
        std::cout << "one of the settings are missing." << std::endl;
    }

    const char *vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec2 position;
        void main()
        {
            gl_Position = vec4(position, 0.0, 1.0);
        }
    )";

    std::string fragmentShaderSourceisf = std::string(R"(
        #version 330 core
        out vec4 FragColor;
        void main()
        {
            FragColor = vec4()") + Color + std::string(R"();
        }
    )");
    
    const char *fragmentShaderSource = fragmentShaderSourceisf.c_str();

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    std::vector<int16_t> audioData(TILES, static_cast<int16_t>(-1.0 * 32757 * 2));
    Rectangles rectangles(audioData.size(), Margin);
    std::vector<int16_t> audioData1(TILES);
    std::vector<int16_t> audioData2(TILES);
    std::vector<int16_t> *currentAudioData = &audioData1;
    std::vector<int16_t> *nextAudioData = &audioData2;

    const double step = 2.0 / 32757;

    for (Rectangles::Rectangle &rectangle : rectangles.getRectangles()){
        rectangle.changeHeight(audioData[rectangle.index] * step);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    int monitorWidth = mode->width;
    int monitorHeight = mode->height;

    GLFWwindow *window = glfwCreateWindow(monitorWidth/WidthRatio, monitorHeight/HeightRatio,
                                          "MiCava", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    std::vector<GLuint> shaderPrograms;
    for (Rectangles::Rectangle &rectangle : rectangles.getRectangles()){
        GLuint vertexShader, fragmentShader;
        vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);
        GLint compileSuccess;
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compileSuccess);
        if (!compileSuccess) {
            GLint logLength;
            glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<GLchar> errorLog(logLength);
            glGetShaderInfoLog(vertexShader, logLength, nullptr, errorLog.data());
            std::cerr << "Vertex Shader Compilation Error:\n"
                    << errorLog.data() << std::endl;
        }

        fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compileSuccess);
        if (!compileSuccess) {
            GLint logLength;
            glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<GLchar> errorLog(logLength);
            glGetShaderInfoLog(fragmentShader, logLength, nullptr, errorLog.data());
            std::cerr << "Fragment Shader Compilation Error:\n"
                    << errorLog.data() << std::endl;
        }

        GLuint shaderProgram;
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        GLint success;
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
            std::cerr << "Shader program linking failed: " << infoLog << std::endl;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        glGenVertexArrays(1, &rectangle.VAO);
        glBindVertexArray(rectangle.VAO);

        glGenBuffers(1, &rectangle.VBO);
        glBindBuffer(GL_ARRAY_BUFFER, rectangle.VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * rectangle.vertices.size(),
                    rectangle.vertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                            (void *)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);

        shaderPrograms.push_back(shaderProgram);
    }

    if (monitor) {
        int windowWidth, windowHeight;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);

        int xPos = (monitorWidth - windowWidth) / 2;
        int yPos = (monitorHeight - windowHeight) / 2;

        glfwSetWindowPos(window, xPos, yPos);
    }

    GLenum error;
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        *nextAudioData = captureAudioData(22);

        std::swap(currentAudioData, nextAudioData);

        for (Rectangles::Rectangle &rectangle : rectangles.getRectangles()){
            rectangle.changeHeight(((*currentAudioData)[rectangle.index] /*+ (65514*2)/3.0*/) * step);
            glBindBuffer(GL_ARRAY_BUFFER, rectangle.VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * rectangle.vertices.size(),
                        rectangle.vertices.data(), GL_STATIC_DRAW);
            glUseProgram(shaderPrograms[rectangle.index]);
            glBindVertexArray(rectangle.VAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    for (Rectangles::Rectangle &rectangle : rectangles.getRectangles()){
        glDeleteVertexArrays(1, &rectangle.VAO);
        glDeleteBuffers(1, &rectangle.VBO);
    }

    for (GLuint shaderProgram : shaderPrograms){
        glDeleteProgram(shaderProgram);
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
