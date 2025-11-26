#include "GeometryUtils.h"
#include <cmath>
#include <limits>
#include <map>

namespace GeometryUtils {
    glm::mat3 rotX(float a) {
        float c = std::cos(a), s = std::sin(a);
        return glm::mat3(1, 0, 0,
                         0, c, s,
                         0, -s, c);
    }

    glm::mat3 rotY(float a) {
        float c = std::cos(a), s = std::sin(a);
        return glm::mat3(c, 0, -s,
                         0, 1, 0,
                         s, 0, c);
    }

    glm::mat3 lookAt(const glm::vec3& camera, const glm::vec3& target, const glm::vec3& worldUp) {
        glm::vec3 F = glm::normalize(target - camera);
        glm::vec3 R = glm::normalize(glm::cross(F, worldUp));
        glm::vec3 U = glm::normalize(glm::cross(R, F));

        return glm::mat3(R.x, R.y, R.z,
                         U.x, U.y, U.z,
                         -F.x, -F.y, -F.z);
    }

    CanvasPoint projectVertexOntoCanvasPoint(const glm::vec3& cameraPosition,float focalLength,const glm::vec3& vertexPosition,const glm::mat3& cameraOrientation) {
        glm::vec3 vertexRelativeToCamera = vertexPosition - cameraPosition;
        float scale = 160.0f;

        glm::vec3 Pcamera = vertexRelativeToCamera * cameraOrientation;

        float x = Pcamera.x;
        float y = Pcamera.y;
        float z = Pcamera.z;
        float u = focalLength * (x / -z) * scale + (640.0f / 2.0f);
        float v = focalLength * (-y / -z) * scale + (480.0f / 2.0f);
        float depth = 1.0f / -z;

        return CanvasPoint(u, v, depth);
    }

    glm::vec3 pixelToRayDirection(int x, int y, int width, int height, float focalLength, const glm::mat3& cameraOrientation) {
        float scale = 160.0f;
        float px = (x - width * 0.5f) / scale;
        float py = (y - height * 0.5f) / scale;
        py = -py;

        glm::vec3 cameraDirection = glm::normalize(glm::vec3(px, py, -focalLength));
        glm::vec3 rayDirection = glm::normalize(cameraOrientation * cameraDirection);
        return rayDirection;
    }

    RayTriangleIntersection getClosestValidIntersection(const glm::vec3& cameraPosition,const glm::vec3& rayDirection,std::vector<ModelTriangle>& triangles,size_t ignoreIndex) {
        glm::vec3 direction = glm::normalize(rayDirection);
        float bestT = std::numeric_limits<float>::max();
        RayTriangleIntersection bestIntersection;
        bestIntersection.triangleIndex = SIZE_MAX;

        for (int i = 0; i < triangles.size(); i++) {
            if (i == ignoreIndex) continue;

            const ModelTriangle triangle = triangles[i];

            glm::vec3 e0 = triangle.vertices[1] - triangle.vertices[0];
            glm::vec3 e1 = triangle.vertices[2] - triangle.vertices[0];
            glm::vec3 SPVector = cameraPosition - triangle.vertices[0];
            glm::mat3 DEMatrix(-direction, e0, e1);
            glm::vec3 possibleSolution = glm::inverse(DEMatrix) * SPVector;

            float t = possibleSolution.x;
            float u = possibleSolution.y;
            float v = possibleSolution.z;
            float w = 1 - u - v;

            if (!((u >= 0.0) && (u <= 1.0) && (v >= 0.0) && (v <= 1.0) && (u + v) <= 1 && t > 0.001f)) continue;

            if (t < bestT) {
                bestT = t;
                glm::vec3 intersection = triangle.vertices[0] + u * e0 + v * e1;
                bestIntersection.distanceFromCamera = t;
                bestIntersection.intersectionPoint = intersection;
                bestIntersection.triangleIndex = i;
                bestIntersection.intersectedTriangle = triangle;
            }
        }
        return bestIntersection;
    }

    void computeVertexNormals(const std::vector<ModelTriangle>& triangles, std::map<size_t, std::array<glm::vec3, 3>>& vertexNormalsMap,int startIndex) {
        std::array<glm::vec3, 3> vertexNormals;

        for (int i = 0; i < triangles.size(); i++) {
            for (int v = 0; v < 3; v++) {
                glm::vec3 normal(0.0f);
                glm::vec3 vertex = triangles[i].vertices[v];
                
                for (int j = 0; j < triangles.size(); j++) {
                    for (int k = 0; k < 3; k++) {
                        if (glm::length(triangles[j].vertices[k] - vertex) < 0.001f) {
                            normal = normal + triangles[j].normal;
                            break;
                        }
                    }
                }
                
                vertexNormals[v] = glm::normalize(normal);
            }
            
            vertexNormalsMap[startIndex + i] = vertexNormals;
        }
    }
}

