#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <Colour.h>
#include "FileLoader.h"

class Environment {
private:
    std::vector<float> hdrData;     // HDR 浮点数据 (RGB)
    int width;
    int height;
    bool loaded;

public:
    Environment();
    Environment(const FileLoader::EnvironmentData& envData);
    ~Environment();
    
    bool loadHDR(const std::string& filename);
    Colour sampleEnvironment(const glm::vec3& direction) const;
    bool isLoaded() const { return loaded; }
    
    int getWidth() const { return width; }
    int getHeight() const { return height; }
};
