#pragma once

#include <string>
#include <vector>
#include <map>
#include <ModelTriangle.h>
#include <Colour.h>

class FileLoader {
public:
    static std::vector<ModelTriangle> loadOBJ(const std::string& filePath, const std::map<std::string, Colour>& palette, float scale);
    static std::map<std::string, Colour> loadMTL(const std::string& filePath);

    struct EnvironmentData {
        std::vector<float> hdrPixels;
        int width;
        int height;
        bool isValid;
    };
    
    static EnvironmentData loadEnvironmentHDR(const std::string& filename);
};

