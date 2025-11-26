#include "FileLoader.h"
#include <Utils.h>
#include <TexturePoint.h>
#include <glm/glm.hpp>
#include <fstream>
#include <iostream>

std::vector<ModelTriangle> FileLoader::loadOBJ(const std::string& filePath, const std::map<std::string, Colour>& palette, float scale) {
    std::string line;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> textureCoords;
    std::vector<ModelTriangle> triangles;
    std::ifstream inputFile(filePath);
    std::string colourName;
    Colour colour(255, 255, 255);
    
    if (!inputFile.is_open()) {
        std::cerr << "fail to read" << std::endl;
        return triangles;
    }

    while (std::getline(inputFile, line)) {
        std::vector<std::string> tokens = split(line, ' ');

        if (tokens.empty()) { continue; }

        if (tokens[0] == "v") {
            float x = std::stof(tokens[1]) * scale;
            float y = std::stof(tokens[2]) * scale;
            float z = std::stof(tokens[3]) * scale;
            vertices.push_back(glm::vec3(x, y, z));
        }
        else if (tokens[0] == "vt") {
            float u = std::stof(tokens[1]);
            float v = std::stof(tokens[2]);
            textureCoords.push_back(glm::vec2(u, v));
        }
        else if (tokens[0] == "usemtl") {
            colourName = tokens[1];
            colour = palette.at(colourName);
        }
        else if (tokens[0] == "f") {
            std::vector<std::string> v0Parts = split(tokens[1], '/');
            std::vector<std::string> v1Parts = split(tokens[2], '/');
            std::vector<std::string> v2Parts = split(tokens[3], '/');
            
            glm::vec3 v0 = vertices[std::stoi(v0Parts[0]) - 1];
            glm::vec3 v1 = vertices[std::stoi(v1Parts[0]) - 1];
            glm::vec3 v2 = vertices[std::stoi(v2Parts[0]) - 1];

            glm::vec3 edge0 = v1 - v0;
            glm::vec3 edge1 = v2 - v0;
            glm::vec3 normal = glm::normalize(glm::cross(edge0, edge1));

            ModelTriangle triangle(v0, v1, v2, colour);
            triangle.normal = normal;
            
            if (v0Parts.size() > 1 && !v0Parts[1].empty() && !textureCoords.empty()) {
                int t0Index = std::stoi(v0Parts[1]) - 1;
                int t1Index = std::stoi(v1Parts[1]) - 1;
                int t2Index = std::stoi(v2Parts[1]) - 1;
                
                triangle.texturePoints[0] = TexturePoint(textureCoords[t0Index].x, textureCoords[t0Index].y);
                triangle.texturePoints[1] = TexturePoint(textureCoords[t1Index].x, textureCoords[t1Index].y);
                triangle.texturePoints[2] = TexturePoint(textureCoords[t2Index].x, textureCoords[t2Index].y);
            }
            
            triangles.push_back(triangle);
        }
    }
    inputFile.close();
    return triangles;
}

std::map<std::string, Colour> FileLoader::loadMTL(const std::string& filePath) {
    std::map<std::string, Colour> palette;
    Colour colour;
    std::string colourName;
    std::string line;
    std::ifstream inputFile(filePath);

    if (!inputFile.is_open()) {
        std::cerr << "fail to read" << std::endl;
        return palette;
    }

    while (std::getline(inputFile, line)) {
        std::vector<std::string> tokens = split(line, ' ');

        if (tokens.empty()) { continue; }

        if (tokens[0] == "newmtl") {
            colourName = tokens[1];
        }
        else if (tokens[0] == "Kd") {
            float r_float = std::stof(tokens[1]);
            float g_float = std::stof(tokens[2]);
            float b_float = std::stof(tokens[3]);
    
            int r = (r_float * 255.0f);
            int g = (g_float * 255.0f);
            int b = (b_float * 255.0f);
            colour = Colour(r, g, b);
            palette[colourName] = colour;
        }
    }
    inputFile.close();
    return palette;
}

