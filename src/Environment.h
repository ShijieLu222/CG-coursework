#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <Colour.h>
#include "FileLoader.h"

class Environment {
private:
    std::vector<float> hdrData;
    int width;
    int height;
    bool loaded;

public:
    Environment();
    Environment(const FileLoader::EnvironmentData& envData);
    Colour sampleEnvironment(const glm::vec3& direction) const;
};
