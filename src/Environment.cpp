#include "Environment.h"
#include <cmath>
#include <algorithm>

Environment::Environment() : width(0), height(0), loaded(false) {}

Environment::Environment(const FileLoader::EnvironmentData& envData) : width(envData.width), height(envData.height), loaded(envData.isValid) {
    if (envData.isValid) {
        hdrData = envData.hdrPixels;
    }
}

Colour Environment::sampleEnvironment(const glm::vec3& direction) const {
    if (!loaded || width == 0 || height == 0) {
        return Colour(0, 0, 0);
    }
    
    // Convert direction to spherical coordinates
    glm::vec3 dir = glm::normalize(direction);
    float theta = std::atan2(dir.z, dir.x);
    float phi = std::acos(std::max(-1.0f, std::min(1.0f, dir.y)));
    
    // Map to texture coordinates
    float u = (theta + M_PI) / (2.0f * M_PI);
    float v = phi / M_PI;
    
    // Sample from HDR texture
    int texX = (int)(u * (width - 1));
    int texY = (int)(v * (height - 1));
    texX = std::max(0, std::min(width - 1, texX));
    texY = std::max(0, std::min(height - 1, texY));
    
    int index = (texY * width + texX) * 3;
    float r = hdrData[index] * 255.0f;
    float g = hdrData[index + 1] * 255.0f;
    float b = hdrData[index + 2] * 255.0f;
    
    // to valid colour range
    r = std::min(255.0f, r);
    g = std::min(255.0f, g);
    b = std::min(255.0f, b);
    
    return Colour((uint8_t)r, (uint8_t)g, (uint8_t)b);
}
