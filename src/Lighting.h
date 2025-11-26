#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <ModelTriangle.h>
#include <RayTriangleIntersection.h>
#include <cstddef>

struct Light {
    glm::vec3 position;
    float ambient;
};

class Lighting {
private:
    static Light light;
public:
    static void setLight(const Light& light);
    static float calculateProximity(const glm::vec3& target, const glm::vec3& lightPosition);
    static float calculateAngleOfIncidence(const glm::vec3& intersectionPoint, const glm::vec3& normal, const glm::vec3& lightPosition);
    static float calculateSpecular(const glm::vec3& intersectionPoint, const glm::vec3& normal, const glm::vec3& lightPosition, const glm::vec3& cameraPosition);
    static float calculateSoftShadow(const glm::vec3& point, size_t selfIndex, std::vector<ModelTriangle>& triangles);
};

