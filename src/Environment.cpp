#include "Environment.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <fstream>
#include "../libs/stb/stb_image.h"

Environment::Environment() : width(0), height(0), loaded(false) {}

Environment::Environment(const FileLoader::EnvironmentData& envData) 
    : width(envData.width), height(envData.height), loaded(envData.isValid) {
    if (envData.isValid) {
        hdrData = envData.hdrPixels;
    }
}

Environment::~Environment() {
    hdrData.clear();
}

bool Environment::loadHDR(const std::string& filename) {
    int channels;
    float* data = stbi_loadf(filename.c_str(), &width, &height, &channels, 3);
    
    if (data == nullptr) {
        std::cerr << "Failed to load HDR file: " << filename << std::endl;
        return false;
    }
    
    std::cout << "Successfully loaded HDR: " << filename << " (" << width << "x" << height << ")" << std::endl;
    
    hdrData.resize(width * height * 3);
    std::copy(data, data + width * height * 3, hdrData.begin());
    
    stbi_image_free(data);
    loaded = true;
    
    return true;
}

Colour Environment::sampleEnvironment(const glm::vec3& direction) const {
    if (!loaded || width == 0 || height == 0) {
        // 程序化天空：蓝色渐变
        glm::vec3 dir = glm::normalize(direction);
        float v = dir.y * 0.5f + 0.5f;
        
        float r = 135 + (255 - 135) * v;
        float g = 206 + (255 - 206) * v;
        float b = 235 + (255 - 235) * v;
        
        return Colour((uint8_t)r, (uint8_t)g, (uint8_t)b);
    }
    
    // 球形贴图采样
    glm::vec3 dir = glm::normalize(direction);
    
    // 计算球面坐标
    float theta = std::atan2(dir.z, dir.x);
    float phi = std::acos(std::max(-1.0f, std::min(1.0f, dir.y)));
    
    // 转换到纹理坐标 [0, 1]
    float u = (theta + M_PI) / (2.0f * M_PI);
    float v = phi / M_PI;
    
    int texX = (int)(u * (width - 1));
    int texY = (int)(v * (height - 1));
    
    texX = std::max(0, std::min(width - 1, texX));
    texY = std::max(0, std::min(height - 1, texY));
    
    // 从 HDR 数据采样 (RGB float)
    int pixelIndex = (texY * width + texX) * 3;
    
    float r = hdrData[pixelIndex] * 255.0f;
    float g = hdrData[pixelIndex + 1] * 255.0f;
    float b = hdrData[pixelIndex + 2] * 255.0f;
    
    // Tone mapping (简单的线性夹紧)
    r = std::min(255.0f, r);
    g = std::min(255.0f, g);
    b = std::min(255.0f, b);
    
    return Colour((uint8_t)r, (uint8_t)g, (uint8_t)b);
}
