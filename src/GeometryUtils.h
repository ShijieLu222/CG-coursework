#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <map>
#include <array>
#include <ModelTriangle.h>
#include <RayTriangleIntersection.h>
#include <CanvasPoint.h>

namespace GeometryUtils {
    glm::mat3 rotX(float a);
    glm::mat3 rotY(float a);
    
    glm::mat3 lookAt(const glm::vec3& camera, const glm::vec3& target, const glm::vec3& worldUp = {0, 1, 0});
    CanvasPoint projectVertexOntoCanvasPoint(const glm::vec3& cameraPosition,float focalLength,const glm::vec3& vertexPosition,const glm::mat3& cameraOrientation);
    glm::vec3 pixelToRayDirection(int x, int y, int width, int height, float focalLength, const glm::mat3& cameraOrientation);
    RayTriangleIntersection getClosestValidIntersection(const glm::vec3& cameraPosition,const glm::vec3& rayDirection,std::vector<ModelTriangle>& triangles,size_t ignoreIndex = SIZE_MAX);
    void computeVertexNormals(const std::vector<ModelTriangle>& triangles, std::map<size_t, std::array<glm::vec3, 3>>& vertexNormalsMap,int startIndex = 0);
    glm::vec3 calculateReflectedRay(const glm::vec3& incomingRay, const glm::vec3& normal);
}

