#include <CanvasTriangle.h>
#include <DrawingWindow.h>
#include <Utils.h>
#include <fstream>
#include <vector>
#include <iostream>
#include <CanvasPoint.h>
#include <Colour.h>
#include <TextureMap.h>
#include <ModelTriangle.h>
#include <RayTriangleIntersection.h>
#include <cmath>
#include <map>
#include <string>
#include <limits>
#include <thread>
#include <chrono>
#include <iomanip>
#include <random>
#include "FileLoader.h"
#include "GeometryUtils.h"
#include "Lighting.h"
#define WIDTH  640
#define HEIGHT 480

enum class RenderMode { Wireframe = 1, Rasterised = 2, RayTraced = 3 };

struct Camera {
    glm::vec3 position;
    glm::mat3 orientation;
    float focalLength;
    bool autoRotate;
};

struct Scene {
    RenderMode mode;
    glm::vec3 center;
};

// global variables
Camera camera = {
    glm::vec3(0.0f, 0.0f, 4.0f),    // position
    glm::mat3(1.0f),                // orientation
    2.0f,                           // focalLength
    false                           // autoRotate
};

Light light = {
    glm::vec3(0.0f, 0.8f, 0.1f),  // position
    0.15f                           // ambient
};

Scene scene = {
    RenderMode::Rasterised,        // mode
    glm::vec3(0.0f, 0.0f, 0.0f)   // center
};

std::map<size_t, std::array<glm::vec3, 3>> vertexNormalsMap;//store vertex normals


void drawLine(DrawingWindow &window, const CanvasPoint &from, const CanvasPoint &to, const Colour &colour, std::vector<std::vector<float>> &depths){
	float xDiff = to.x - from.x;
	float yDiff = to.y - from.y;
	float numberOfSteps = std::max(abs(xDiff), abs(yDiff));
	float xStepSize = xDiff / numberOfSteps;
	float yStepSize = yDiff / numberOfSteps;
	float depthStep = (to.depth - from.depth) / numberOfSteps;  // 深度插值步长
	
	uint32_t colourValue = (255u << 24) + (uint32_t(colour.red) << 16) + (uint32_t(colour.green) << 8) + uint32_t(colour.blue);
	
	for (float i = 0.0; i <= numberOfSteps; i++){
		float x = from.x + xStepSize * i;
		float y = from.y + yStepSize * i;
		float depth = from.depth + depthStep * i;  // 当前像素的深度

		if (x >= 0 && x < window.width && y >= 0 && y < window.height){
			int xi = round(x);
			int yi = round(y);
			
			if (depth != 0 && (depths[yi][xi] == 0 || 1.0f/depth > depths[yi][xi])){
				window.setPixelColour(xi, yi, colourValue);
				depths[yi][xi] = 1.0f/depth;  // 更新深度缓冲
			}
		}
	}
}

void drawStrokedTriangle(DrawingWindow &window, const CanvasTriangle &triangle, const Colour &colour, std::vector<std::vector<float>> &depths){
	drawLine(window, triangle.vertices[0], triangle.vertices[1], colour, depths);
	drawLine(window, triangle.vertices[1], triangle.vertices[2], colour, depths);
	drawLine(window, triangle.vertices[2], triangle.vertices[0], colour, depths);
}

std::vector<CanvasPoint> sortByY(const CanvasTriangle &triangle){
	CanvasPoint v0 = triangle.vertices[0]; // top
	CanvasPoint v1 = triangle.vertices[1]; // mid
	CanvasPoint v2 = triangle.vertices[2]; // bottom

	if (v1.y < v0.y)
		std::swap(v0, v1);
	if (v2.y < v0.y)
		std::swap(v0, v2);
	if (v2.y < v1.y)
		std::swap(v1, v2);

	std::vector<CanvasPoint> sortedPs;
	sortedPs.push_back(v0);
	sortedPs.push_back(v1);
	sortedPs.push_back(v2);

	return sortedPs;
}

CanvasPoint FindExtraP(std::vector<CanvasPoint> sortedPs){ // also for tuxture point
	CanvasPoint extraP;
	extraP.y = sortedPs[1].y;
	float dy = sortedPs[2].y - sortedPs[0].y;
	float ratio = (sortedPs[1].y - sortedPs[0].y) / dy; // 用y的计算比例，y的变化占比例eg 0.25，从top到mid
	// 屏幕坐标插值
	extraP.x = sortedPs[0].x + ratio * (sortedPs[2].x - sortedPs[0].x); // 拿比例 * top到bottom的总值 + top起始点的x
	// 纹理坐标也要插值，这是关键！
	extraP.texturePoint.x = sortedPs[0].texturePoint.x + ratio * (sortedPs[2].texturePoint.x - sortedPs[0].texturePoint.x);
	extraP.texturePoint.y = sortedPs[0].texturePoint.y + ratio * (sortedPs[2].texturePoint.y - sortedPs[0].texturePoint.y);

	extraP.depth = sortedPs[0].depth + ratio * (sortedPs[2].depth - sortedPs[0].depth);

	return extraP;
}

void fillFlatTopTriangle(DrawingWindow &window, CanvasPoint bottom, CanvasPoint mid, CanvasPoint extraP, Colour colour, std::vector<std::vector<float>> &depths, CanvasPoint v0, CanvasPoint v1, CanvasPoint v2, const TextureMap *texture){ // 平顶三角形
	CanvasPoint midLp, midRp;
	if (mid.x > extraP.x){
		midRp = mid;
		midLp = extraP;
	}else{
		midRp = extraP;
		midLp = mid;
	}

	float dxL = (bottom.x - midLp.x) / (bottom.y - midLp.y); // 每下降1个像素行，x变化多少
	float dxR = (bottom.x - midRp.x) / (bottom.y - midRp.y);

	int yStart = (int)std::ceil(midLp.y);
	int yEnd = (int)std::floor(bottom.y);

	uint32_t colourValue = (255u << 24) + (uint32_t(colour.red) << 16) + (uint32_t(colour.green) << 8) + (uint32_t(colour.blue));

	for (int y = yStart; y < yEnd; y++){
		int xLstart = (int)std::floor(midLp.x + (y - midLp.y) * dxL);
        int xRstart = (int)std::ceil(midRp.x + (y - midRp.y) * dxR);
        
		for (int x = xLstart; x < xRstart; x++){ //遍历每个像素
			if (y >= 0 && y < HEIGHT && x >= 0 && x < WIDTH) {
				// 使用重心坐标判断是否在三角形内
				glm::vec3 baryCentric = convertToBarycentricCoordinates(
					glm::vec2(v0.x, v0.y), 
					glm::vec2(v1.x, v1.y), 
					glm::vec2(v2.x, v2.y), 
					glm::vec2(x, y)
				);
				
				// 只有重心坐标全部非负才绘制（确保在三角形内）
				if (baryCentric.x >= 0 && baryCentric.y >= 0 && baryCentric.z >= 0) {
					// 计算深度：baryCentric.z * v0 + baryCentric.x * v1 + baryCentric.y * v2
					float pixelDepth = baryCentric.z * v0.depth + baryCentric.x * v1.depth + baryCentric.y * v2.depth;
					
					if (pixelDepth > depths[y][x]) {
						if (texture != nullptr) {
							glm::vec2 t0(v0.texturePoint.x, v0.texturePoint.y);
							glm::vec2 t1(v1.texturePoint.x, v1.texturePoint.y);
							glm::vec2 t2(v2.texturePoint.x, v2.texturePoint.y);
							glm::vec2 texturePoint = baryCentric.z * t0 + baryCentric.x * t1 + baryCentric.y * t2;
							uint32_t textureColour = texture->pixels[round(texturePoint.y * texture->height) * texture->width + round(texturePoint.x * texture->width)];
							window.setPixelColour(x, y, textureColour);
						} else {
							window.setPixelColour(x, y, colourValue);
						}
						depths[y][x] = pixelDepth;
					}
				}
			}
		}
	}
}

void fillFlatBottomTriangle(DrawingWindow &window, CanvasPoint top, CanvasPoint mid, CanvasPoint extraP, Colour colour, std::vector<std::vector<float>> &depths, CanvasPoint v0, CanvasPoint v1, CanvasPoint v2, const TextureMap *texture){ // 顶角三角形
	CanvasPoint midLp, midRp;
	if (mid.x > extraP.x){
		midRp = mid;
		midLp = extraP;
	}else{
		midRp = extraP;
		midLp = mid;
	}

	float dxL = (midLp.x - top.x) / (midLp.y - top.y); // 每1个像素行，x变化多少
	float dxR = (midRp.x - top.x) / (midRp.y - top.y);

	int yStart = (int)std::ceil(top.y);
	int yEnd = (int)std::floor(midLp.y);

	uint32_t colourValue = (255u << 24) + (uint32_t(colour.red) << 16) + (uint32_t(colour.green) << 8) + (uint32_t(colour.blue));
	for (int y = yStart; y <= yEnd; y++){ // y<=
		int xLstart = (int)std::floor(top.x + (y - top.y) * dxL);
        int xRstart = (int)std::ceil(top.x + (y - top.y) * dxR);

		for (int x = xLstart; x < xRstart; x++){
			if (y >= 0 && y < HEIGHT && x >= 0 && x < WIDTH) {
				glm::vec3 baryCentric = convertToBarycentricCoordinates(
					glm::vec2(v0.x, v0.y), 
					glm::vec2(v1.x, v1.y), 
					glm::vec2(v2.x, v2.y), 
					glm::vec2(x, y)
				);
				
				// 只有重心坐标全部非负才绘制-确保在三角形内
				if (baryCentric.x >= 0 && baryCentric.y >= 0 && baryCentric.z >= 0){
					float pixelDepth = baryCentric.z * v0.depth + baryCentric.x * v1.depth + baryCentric.y * v2.depth;
					
					if (pixelDepth > depths[y][x]){
						if (texture != nullptr) {
							glm::vec2 t0(v0.texturePoint.x, v0.texturePoint.y);
							glm::vec2 t1(v1.texturePoint.x, v1.texturePoint.y);
							glm::vec2 t2(v2.texturePoint.x, v2.texturePoint.y);
							glm::vec2 texturePoint = baryCentric.z * t0 + baryCentric.x * t1 + baryCentric.y * t2;
							uint32_t texColour = texture->pixels[round(texturePoint.y * texture->height) * texture->width + round(texturePoint.x * texture->width)];
							window.setPixelColour(x, y, texColour);
						}else{
							window.setPixelColour(x, y, colourValue);
						}
						depths[y][x] = pixelDepth;
					}
				}
			}
		}	
	}
}

void drawFilledTriangle(DrawingWindow &window, const CanvasTriangle &triangle, const Colour &colour, std::vector<std::vector<float>> &depths, const TextureMap *texture = nullptr){ // Rasteriser
	std::vector<CanvasPoint> sortedPs = sortByY(triangle);
	CanvasPoint extraP = FindExtraP(sortedPs);
	CanvasPoint top = sortedPs[0];
	CanvasPoint mid = sortedPs[1];
	CanvasPoint bottom = sortedPs[2];
	
	fillFlatTopTriangle(window, bottom, mid, extraP, colour, depths, triangle.vertices[0], triangle.vertices[1], triangle.vertices[2], texture);
	fillFlatBottomTriangle(window, top, mid, extraP, colour, depths, triangle.vertices[0], triangle.vertices[1], triangle.vertices[2], texture);

}

void drawWireframeScene(DrawingWindow &window, const glm::vec3 &cameraPosition, const glm::mat3 &cameraOrientation, float focalLength, const std::vector<ModelTriangle> &triangles, std::vector<std::vector<float>> &depths) {
    const float minValidDepth = 0.0f;

    for (ModelTriangle triangle : triangles){
		CanvasPoint projectedPoint0 = GeometryUtils::projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[0],cameraOrientation);
		CanvasPoint projectedPoint1 = GeometryUtils::projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[1],cameraOrientation);
		CanvasPoint projectedPoint2 = GeometryUtils::projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[2],cameraOrientation);
		
		if (projectedPoint0.depth < minValidDepth  ||projectedPoint1.depth < minValidDepth ||projectedPoint2.depth < minValidDepth ) {
			continue;
		}
		
		CanvasTriangle canvasTriangle(projectedPoint0,projectedPoint1,projectedPoint2);
		drawStrokedTriangle(window, canvasTriangle, triangle.colour, depths);
    }
}

void drawRasterisedFilledScene(DrawingWindow &window, const glm::vec3 &cameraPosition, const glm::mat3 &cameraOrientation, float focalLength, const std::vector<ModelTriangle> &triangles, std::vector<std::vector<float>> &depths, const TextureMap *texture = nullptr) {
	const float minValidDepth = 0.0f;
	for (const ModelTriangle& triangle : triangles){
			CanvasPoint projectedPoint0 = GeometryUtils::projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[0],cameraOrientation);
			CanvasPoint projectedPoint1 = GeometryUtils::projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[1],cameraOrientation);
			CanvasPoint projectedPoint2 = GeometryUtils::projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[2],cameraOrientation);
			
			// depth = 1/-z，makesure depth not be too small
			if (projectedPoint0.depth < minValidDepth  ||projectedPoint1.depth < minValidDepth ||projectedPoint2.depth < minValidDepth ) {
				continue;
			}
			
			// 传递纹理坐标到 CanvasPoint
			projectedPoint0.texturePoint = triangle.texturePoints[0];
			projectedPoint1.texturePoint = triangle.texturePoints[1];
			projectedPoint2.texturePoint = triangle.texturePoints[2];
			
			CanvasTriangle canvasTriangle(projectedPoint0,projectedPoint1,projectedPoint2);
			
			bool hasTexture = (triangle.texturePoints[0].x != 0 || triangle.texturePoints[0].y != 0 ||
			                   triangle.texturePoints[1].x != 0 || triangle.texturePoints[1].y != 0 ||
			                   triangle.texturePoints[2].x != 0 || triangle.texturePoints[2].y != 0);
			
			if (hasTexture) {
				drawFilledTriangle(window, canvasTriangle, triangle.colour, depths, texture);
			} else {
				drawFilledTriangle(window, canvasTriangle, triangle.colour, depths, nullptr);
			}
	}
}

Colour traceRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, std::vector<ModelTriangle> &triangles, const TextureMap *texture, int sphereStartIndex, int sphereCount, int depth = 0) {
	const int MAX_REFLECTION_DEPTH = 3;
	const float EPSILON = 1e-4f;
	
	if (depth > MAX_REFLECTION_DEPTH) {
		return Colour(0, 0, 0);
	}
	
	RayTriangleIntersection intersection = GeometryUtils::getClosestValidIntersection(rayOrigin, rayDirection, triangles);
	
	if (intersection.triangleIndex == SIZE_MAX) {
		return Colour(0, 0, 0);
	}
	
	glm::vec3 point = intersection.intersectionPoint;
	const ModelTriangle& tri = intersection.intersectedTriangle;
	
	// 如果是镜面，进行递归反射追踪
	if (tri.isMirror) {
		glm::vec3 reflectedDirection = GeometryUtils::calculateReflectedRay(rayDirection, tri.normal);
		glm::vec3 reflectedOrigin = point + tri.normal * EPSILON;
		Colour reflectedColour = traceRay(reflectedOrigin, reflectedDirection, triangles, texture, sphereStartIndex, sphereCount, depth + 1);
		
		// 根据反射率混合颜色
		return Colour(
			(uint8_t)(reflectedColour.red * tri.reflectivity),
			(uint8_t)(reflectedColour.green * tri.reflectivity),
			(uint8_t)(reflectedColour.blue * tri.reflectivity)
		);
	}
	
	glm::vec3 edge0 = tri.vertices[1] - tri.vertices[0];
	glm::vec3 edge1 = tri.vertices[2] - tri.vertices[0];
	glm::vec3 toPoint = point - tri.vertices[0];
	
	float dot00 = glm::dot(edge0, edge0);
	float dot01 = glm::dot(edge0, edge1);
	float dot11 = glm::dot(edge1, edge1);
	float dot20 = glm::dot(toPoint, edge0);
	float dot21 = glm::dot(toPoint, edge1);
	float denominator = dot00 * dot11 - dot01 * dot01;
	
	float bary_v = (dot11 * dot20 - dot01 * dot21) / denominator;
	float bary_w = (dot00 * dot21 - dot01 * dot20) / denominator;
	float bary_u = 1.0f - bary_v - bary_w;
	
	// 检查是否为球体
	bool isSphere = (sphereStartIndex >= 0 && 
	                 (int)intersection.triangleIndex >= sphereStartIndex && 
	                 (int)intersection.triangleIndex < sphereStartIndex + sphereCount);
	
	// 用重心坐标插值顶点法线（仅在球体上使用Phong shading）
	glm::vec3 interpolatedNormal;
	if (isSphere) {
		// 球体使用插值法线 (Phong shading)
		std::array<glm::vec3, 3> vertexNormals = vertexNormalsMap[intersection.triangleIndex];
		interpolatedNormal = glm::normalize(bary_u * vertexNormals[0] + bary_v * vertexNormals[1] + bary_w * vertexNormals[2]);
	} else {
		// 其他物体使用三角形法线 (flat shading)
		interpolatedNormal = tri.normal;
	}
	
	float proximity = Lighting::calculateProximity(point, light.position);
	float angle = Lighting::calculateAngleOfIncidence(point, interpolatedNormal, light.position);
	float diffuse = proximity * angle;
	float specular = Lighting::calculateSpecular(point, interpolatedNormal, light.position, rayOrigin);
	
	float shadowFactor = Lighting::calculateSoftShadow(point, intersection.triangleIndex, triangles);
	
	float Brightness = light.ambient + (diffuse + specular) * shadowFactor;
	Brightness = std::min(1.0f, Brightness);
	
	bool hasTexture = (tri.texturePoints[0].x != 0 || tri.texturePoints[0].y != 0 ||
	                    tri.texturePoints[1].x != 0 || tri.texturePoints[1].y != 0 ||
	                    tri.texturePoints[2].x != 0 || tri.texturePoints[2].y != 0);
	Colour colourValue = tri.colour;
	
	if (hasTexture && texture != nullptr) {
		// 用重心坐标插值纹理坐标
		glm::vec2 t0(tri.texturePoints[0].x, tri.texturePoints[0].y);
		glm::vec2 t1(tri.texturePoints[1].x, tri.texturePoints[1].y);
		glm::vec2 t2(tri.texturePoints[2].x, tri.texturePoints[2].y);
		glm::vec2 textureCoord = bary_u * t0 + bary_v * t1 + bary_w * t2;
		
		// 从纹理采样
		int texX = round(textureCoord.x * texture->width);
		int texY = round(textureCoord.y * texture->height);
		texX = std::max(0, std::min((int)texture->width - 1, texX));
		texY = std::max(0, std::min((int)texture->height - 1, texY));
		uint32_t textureColour = texture->pixels[texY * texture->width + texX];

		colourValue = Colour((textureColour >> 16) & 255, (textureColour >> 8) & 255, textureColour & 255);
	}
	
	return Colour(
		(uint8_t)std::min(255u, (uint32_t)(colourValue.red * Brightness)),
		(uint8_t)std::min(255u, (uint32_t)(colourValue.green * Brightness)),
		(uint8_t)std::min(255u, (uint32_t)(colourValue.blue * Brightness))
	);
}

void drawRayTracedScene(DrawingWindow &window,const glm::vec3 &cameraPosition,const glm::mat3 &cOrientation,const float &focalLength,std::vector<ModelTriangle>triangles, const TextureMap *texture = nullptr, int sphereStartIndex = -1, int sphereCount = 0){
	// Detect number of CPU cores
	unsigned int numThreads = std::thread::hardware_concurrency();
	if (numThreads == 0) numThreads = 4;
	
	// Calculate rows per thread
	int rowsPerThread = HEIGHT / numThreads;
	
	// Vector to store thread handles
	std::vector<std::thread> threads;
	
	// Lambda function to process a range of rows
	auto renderRows = [&](int startY, int endY){
		for(int y = startY; y < endY; y++){
			for(int x = 0; x < WIDTH; x++){
				glm::vec3 rayDirection = GeometryUtils::pixelToRayDirection(x,y,WIDTH,HEIGHT,focalLength,cOrientation);
				const float v = 1e-4f;            
				glm::vec3 rayOrigin  = cameraPosition + rayDirection * v;
				
				Colour pixelColour = traceRay(rayOrigin, rayDirection, triangles, texture, sphereStartIndex, sphereCount, 0);
				uint32_t colour = (255u << 24) + ((uint32_t)pixelColour.red << 16) + ((uint32_t)pixelColour.green << 8) + (uint32_t)pixelColour.blue;
				window.setPixelColour(x, y, colour);
			}
		}
	};
	
	// Spawn threads
	for (unsigned int i = 0; i < numThreads; i++) {
		int startY = i * rowsPerThread;
		int endY;
		if (i == numThreads){
			endY =  HEIGHT;
		} else {
			endY =  (i + 1) * rowsPerThread;
		}
		threads.emplace_back(renderRows, startY, endY);
	}
	
	// Join all threads
	for (auto& thread : threads) {
		thread.join();
	}
}

void handleEvent(SDL_Event event, DrawingWindow &window){
	float rotation = glm::radians(5.0f);      // angle change for rotation
	float orientation = glm::radians(5.0f);   // angle change for orientation
	static float translation  = 0.25f;        // translation distance

	if (event.type == SDL_KEYDOWN){
		if (event.key.keysym.sym == SDLK_LEFT){
			std::cout << "LEFT" << std::endl;
			camera.position = scene.center + GeometryUtils::rotY(rotation)*(camera.position-scene.center);
			camera.orientation = GeometryUtils::lookAt(camera.position, scene.center);
		}else if (event.key.keysym.sym == SDLK_RIGHT){
			std::cout << "RIGHT" << std::endl;
			camera.position = scene.center + GeometryUtils::rotY(-rotation)*(camera.position-scene.center);
			camera.orientation = GeometryUtils::lookAt(camera.position, scene.center);
		}else if (event.key.keysym.sym == SDLK_UP){
			std::cout << "UP" << std::endl;
			camera.position = scene.center + GeometryUtils::rotX(rotation)*(camera.position-scene.center);
			camera.orientation = GeometryUtils::lookAt(camera.position, scene.center);
		}else if (event.key.keysym.sym == SDLK_DOWN){
			std::cout << "DOWN" << std::endl;
			camera.position = scene.center + GeometryUtils::rotX(-rotation)*(camera.position-scene.center);
			camera.orientation = GeometryUtils::lookAt(camera.position, scene.center);
		}
		switch (event.key.keysym.sym){
		case SDLK_1:scene.mode = RenderMode::Wireframe;break;
        case SDLK_2:scene.mode = RenderMode::Rasterised;break;
        case SDLK_3:scene.mode = RenderMode::RayTraced;break;

		case SDLK_a: camera.position.x = camera.position.x - translation;break;
		case SDLK_d: camera.position.x = camera.position.x + translation;break;
		case SDLK_w: camera.position.y = camera.position.y + translation;break;
		case SDLK_s: camera.position.y = camera.position.y - translation;break;
		case SDLK_q: camera.position.z = camera.position.z - translation;break;
		case SDLK_e: camera.position.z = camera.position.z + translation;break;

		case SDLK_j: camera.orientation = camera.orientation * GeometryUtils::rotY(+orientation); break; // j:pan 
		case SDLK_k: camera.orientation = camera.orientation * GeometryUtils::rotY(-orientation); break; // k:pan 
		case SDLK_i: camera.orientation = camera.orientation * GeometryUtils::rotX(+orientation); break; // i:tilt 
		case SDLK_o: camera.orientation = camera.orientation * GeometryUtils::rotX(-orientation); break; // o:tilt

		case SDLK_r: camera.autoRotate = !camera.autoRotate; break;
		}
	}
	else if (event.type == SDL_MOUSEBUTTONDOWN)
	{
		window.savePPM("output.ppm");
		window.saveBMP("output.bmp");
	}
}

int main(int argc, char *argv[]){
	DrawingWindow window = DrawingWindow(WIDTH, HEIGHT, false);
	SDL_Event event;

	std::map<std::string, Colour> paletteTexturedBox = FileLoader::loadMTL("src/textured-cornell-box.mtl");
    std::vector<ModelTriangle> trianglesTexturedBox = FileLoader::loadOBJ("src/textured-cornell-box.obj", paletteTexturedBox, 0.35f);
    TextureMap texture("texture.ppm");
    std::map<std::string, Colour> paletteSphere = FileLoader::loadMTL("src/sphere.mtl");
    std::vector<ModelTriangle> trianglesSphere = FileLoader::loadOBJ("src/sphere.obj", paletteSphere, 0.4f);
	
	// add fixed translation to sphere -- z negative is towards the camera
	glm::vec3 offset(0.4f, -1.1f, -0.9f);
	for (auto& tri : trianglesSphere) {
    tri.vertices[0] += offset;
    tri.vertices[1] += offset;
    tri.vertices[2] += offset;
	}
	
	// combine all triangles for ray tracing
	std::vector<ModelTriangle> allTriangles;
	allTriangles.insert(allTriangles.end(), trianglesTexturedBox.begin(), trianglesTexturedBox.end());
	int sphereStartIndex = (int)allTriangles.size();  // sphere 在合并向量中的起始索引
	allTriangles.insert(allTriangles.end(), trianglesSphere.begin(), trianglesSphere.end());
	
	Lighting::setLight(light);
	
	// calculate normals for Gouraud Shading
	GeometryUtils::computeVertexNormals(trianglesTexturedBox, vertexNormalsMap, 0);
	GeometryUtils::computeVertexNormals(trianglesSphere, vertexNormalsMap, sphereStartIndex);

	// FPS counter
	int fps = 0;
	auto lastTime = std::chrono::high_resolution_clock::now();

	while (true){
		if (window.pollForInputEvents(event))
			handleEvent(event, window);

		if (camera.autoRotate) {
        	camera.position = scene.center + GeometryUtils::rotY(glm::radians(0.5f)) * (camera.position - scene.center);
        	camera.orientation = GeometryUtils::lookAt(camera.position, scene.center);
    	} 

	window.clearPixels();
	
	// 为每一帧创建一次深度缓冲，所有物体共享
	std::vector<std::vector<float>> depths(HEIGHT, std::vector<float>(WIDTH, 0.0f));
	
	switch (scene.mode) {
        case RenderMode::Wireframe:
            drawWireframeScene(window, camera.position, camera.orientation, camera.focalLength, trianglesTexturedBox, depths);
			drawWireframeScene(window, camera.position, camera.orientation, camera.focalLength, trianglesSphere, depths);
            break;
        case RenderMode::Rasterised:
            // 传递纹理给 texturedBox，自动判断每个三角形是否有纹理
            drawRasterisedFilledScene(window, camera.position, camera.orientation, camera.focalLength, trianglesTexturedBox, depths, &texture);
			drawRasterisedFilledScene(window, camera.position, camera.orientation, camera.focalLength, trianglesSphere, depths, nullptr);
            break;
        case RenderMode::RayTraced:
			drawRayTracedScene(window, camera.position, camera.orientation, camera.focalLength, allTriangles, &texture, sphereStartIndex, (int)trianglesSphere.size());
            break;
    	}
		window.renderFrame();

		// FPS计数
		fps++;
		auto currentTime = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed = currentTime - lastTime;
		
		// 每秒打印一次FPS
		if (elapsed.count() >= 1.0) {
			std::cout << "FPS: " << fps << std::endl;
			fps = 0;
			lastTime = currentTime;
		}

		// static int frame = 0; char name[128];
        // sprintf(name, "frames/frame_%04d.bmp", frame++);
        // window.saveBMP(name);
	}
	return 0;
}