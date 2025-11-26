#include "Lighting.h"
#include "GeometryUtils.h"
#include <cmath>
#include <limits>
#include <random>
#include <thread>

Light Lighting::light = {glm::vec3(0.0f, 0.8f, 0.1f), 0.15f};

void Lighting::setLight(const Light& newLight) {
    light = newLight;
}

float Lighting::calculateProximity(const glm::vec3& target, const glm::vec3& lightPosition) {
	float distanceToLight = glm::length(lightPosition - target);
	float proximity = 10.0f / (4.0f * M_PI *distanceToLight * distanceToLight);
	return std::min(1.0f,proximity) ;
}

float Lighting::calculateAngleOfIncidence(const glm::vec3& intersectionPoint, const glm::vec3& normal, const glm::vec3& lightPosition) {
	glm::vec3 N = glm::normalize(normal);
    glm::vec3 toLight = glm::normalize(lightPosition - intersectionPoint);
    float angle = glm::dot(N, toLight);
	return std::max(0.0f, angle);
}

float Lighting::calculateSpecular(const glm::vec3& intersectionPoint, const glm::vec3& normal, const glm::vec3& lightPosition, const glm::vec3& cameraPosition) {
	glm::vec3 N = glm::normalize(normal);
	glm::vec3 L = glm::normalize(lightPosition - intersectionPoint); 
	glm::vec3 V = glm::normalize(cameraPosition - intersectionPoint); 
	glm::vec3 R = 2.0f * glm::dot(N, L) * N - L;

	float specular = glm::dot(R, V); 
	return std::pow(std::max(0.0f, specular), 256);
}

float Lighting::calculateSoftShadow(const glm::vec3& point, size_t selfIndex, std::vector<ModelTriangle>& triangles){
    int numSamples = 8;        // 采样次数（减少以提升性能）
    float lightRadius = 0.15f;  // 光源半径
    int visibleSamples = 0;

    // 修复：使用线程安全的随机数生成器，解决多线程导致的噪点
    static thread_local std::mt19937 generator(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    std::uniform_real_distribution<float> dist(-lightRadius, lightRadius);

    for (int i = 0; i < numSamples; i++) {
        // 生成随机偏移（立方体内采样，快速）
        glm::vec3 randomOffset(dist(generator), dist(generator), dist(generator));

        glm::vec3 currentLightPos = light.position + randomOffset;
        glm::vec3 toLight = currentLightPos - point;
        float distToLight = glm::length(toLight);
        
        if (distToLight <= 1e-4f) {
            visibleSamples++;
            continue;
        }
        
        glm::vec3 lightDir = glm::normalize(toLight);
        glm::vec3 rayOrigin = point + lightDir * 0.001f;   
        RayTriangleIntersection intersection = GeometryUtils::getClosestValidIntersection(rayOrigin, lightDir, triangles, selfIndex);

        // 检查光线是否被遮挡
        if (intersection.triangleIndex == SIZE_MAX || intersection.distanceFromCamera > distToLight) {
            visibleSamples++;
        }
    }
    return (float)visibleSamples / (float)numSamples;
}

