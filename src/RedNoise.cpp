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
#define WIDTH  640
#define HEIGHT 480

enum class RenderMode { Wireframe = 1, Rasterised = 2, RayTraced = 3 };

struct Camera {
    glm::vec3 position;
    glm::mat3 orientation;
    float focalLength;
    bool autoRotate;
};

struct Light {
    glm::vec3 position;
    float ambient;
};

struct Scene {
    RenderMode mode;
    glm::vec3 center;
};

// 全局变量
Camera camera = {
    glm::vec3(0.0f, 0.0f, 4.0f),    // position
    glm::mat3(1.0f),                // orientation
    2.0f,                           // focalLength
    false                           // autoRotate
};

Light light = {
    glm::vec3(0.0f, 0.9f, 0.1f),  // position
    0.15f                           // ambient
};

Scene scene = {
    RenderMode::Rasterised,        // mode
    glm::vec3(0.0f, 0.0f, 0.0f)   // center
};

// 存储顶点法线
std::map<size_t, std::array<glm::vec3, 3>> vertexNormalsMap;

glm::mat3 rotX(float a) {
    float c = std::cos(a), s = std::sin(a);
    return glm::mat3( 1,0,0,   
					  0,c,s,
					  0,-s,c );
}

glm::mat3 rotY(float a) {
    float c = std::cos(a), s = std::sin(a);
    return glm::mat3( c,0,-s,
	               0,1,0,
	  			   s,0,c );
}

std::vector<ModelTriangle> loadOBJ(const std::string& filePath,const std::map<std::string,Colour>& palette,float scale){
    std::string line; //store the line
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> textureCoords;
    std::vector<ModelTriangle> triangles;
    std::ifstream inputFile(filePath); //read file
    std::string colourName;
    Colour colour(255,255,255);
    
    if(!inputFile.is_open()){
            std::cerr<<"fail to read"<<std::endl;
            return triangles;
    }

    while (std::getline(inputFile, line)) {
        std::vector<std::string> tokens = split(line,' ');//每当遇到一个空格，就切一刀

        if (tokens.empty()) {continue;} // 跳过空行，处理下一行

        if (tokens[0] == "v"){
            float x = std::stof(tokens[1])*scale; //string to float
            float y = std::stof(tokens[2])*scale;
            float z = std::stof(tokens[3])*scale;
            vertices.push_back(glm::vec3(x, y, z));
        }
        else if (tokens[0] == "vt"){ // 纹理坐标
            float u = std::stof(tokens[1]);
            float v = std::stof(tokens[2]);
            textureCoords.push_back(glm::vec2(u, v));
        }
        else if (tokens[0]=="usemtl")
        {
            colourName = tokens[1];
            colour = palette.at(colourName);
        }
        else if (tokens[0] == "f"){
            // 支持 "f v" 和 "f v/vt" 格式
            std::vector<std::string> v0Parts = split(tokens[1], '/');
            std::vector<std::string> v1Parts = split(tokens[2], '/');
            std::vector<std::string> v2Parts = split(tokens[3], '/');
            
            // 获取顶点坐标
            glm::vec3 v0 = vertices[std::stoi(v0Parts[0])-1];
            glm::vec3 v1 = vertices[std::stoi(v1Parts[0])-1];
            glm::vec3 v2 = vertices[std::stoi(v2Parts[0])-1];

			glm::vec3 edge0 = v1 - v0;
			glm::vec3 edge1 = v2 - v0;
			glm::vec3 normal = glm::normalize(glm::cross(edge0, edge1));

			ModelTriangle triangle(v0, v1, v2, colour);
			triangle.normal = normal;
			
			// 如果有纹理坐标，添加到三角形
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

std::map<std::string,Colour> loadMTL(const std::string& filePath){
    std::map<std::string, Colour> palette;
    Colour colour;
    std::string colourName;
    std::string line;
    std::ifstream inputFile(filePath);

    if(!inputFile.is_open()){
            std::cerr<<"fail to read"<<std::endl;
            return palette;
    }

    while(std::getline(inputFile,line)){
        std::vector<std::string> tokens = split(line,' ');

        if (tokens.empty()) {continue;}

        if (tokens[0] == "newmtl"){
            colourName = (tokens[1]);
        }else if (tokens[0]== "Kd"){
            float r_float = std::stof(tokens[1]);
            float g_float = std::stof(tokens[2]);
            float b_float = std::stof(tokens[3]);
    
            int r = (r_float * 255.0f);
            int g = (g_float * 255.0f);
            int b = (b_float * 255.0f);
            colour =Colour(r,g,b);
            palette[colourName] = colour;
        }
    }
    inputFile.close();
    return palette;
}

CanvasPoint projectVertexOntoCanvasPoint(const glm::vec3 &cameraPosition,float focalLength, const glm::vec3 &vertexPosition,const glm::mat3 &cameraOrientation){
    glm::vec3 vertexRelativeToCamera = vertexPosition - cameraPosition;
    float scale = 160.0f;

    glm::vec3 Pcamera = vertexRelativeToCamera * cameraOrientation;

    float x =Pcamera.x;
    float y =Pcamera.y;
    float z =Pcamera.z;
    float u =focalLength * (x/-z)*scale + (WIDTH/2.0f);
    float v =focalLength * (-y/-z)*scale + (HEIGHT/2.0f);
    float depth = 1.0f / -z;
    

    return CanvasPoint(u,v,depth);
}

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
	// 纹理坐标也要插值！这是关键！
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
				if (baryCentric.x >= 0 && baryCentric.y >= 0 && baryCentric.z >= 0) {
					// 计算深度：baryCentric.z * v0 + baryCentric.x * v1 + baryCentric.y * v2
					float pixelDepth = baryCentric.z * v0.depth + baryCentric.x * v1.depth + baryCentric.y * v2.depth;
					
					if (pixelDepth > depths[y][x]) {
						if (texture != nullptr) {
							glm::vec2 t0(v0.texturePoint.x, v0.texturePoint.y);
							glm::vec2 t1(v1.texturePoint.x, v1.texturePoint.y);
							glm::vec2 t2(v2.texturePoint.x, v2.texturePoint.y);
							glm::vec2 texturePoint = baryCentric.z * t0 + baryCentric.x * t1 + baryCentric.y * t2;
							uint32_t texColour = texture->pixels[round(texturePoint.y * texture->height) * texture->width + round(texturePoint.x * texture->width)];
							window.setPixelColour(x, y, texColour);
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

void drawFilledTriangle(DrawingWindow &window, const CanvasTriangle &triangle, const Colour &colour, std::vector<std::vector<float>> &depths, const TextureMap *texture = nullptr){ // Rasteriser
	std::vector<CanvasPoint> sortedPs = sortByY(triangle);
	CanvasPoint extraP = FindExtraP(sortedPs);
	CanvasPoint top = sortedPs[0];
	CanvasPoint mid = sortedPs[1];
	CanvasPoint bottom = sortedPs[2];
	
	// 传入原始三角形顶点用于重心坐标计算，并传递纹理指针
	fillFlatTopTriangle(window, bottom, mid, extraP, colour, depths, triangle.vertices[0], triangle.vertices[1], triangle.vertices[2], texture);
	fillFlatBottomTriangle(window, top, mid, extraP, colour, depths, triangle.vertices[0], triangle.vertices[1], triangle.vertices[2], texture);

}

void drawWireframeScene(DrawingWindow &window, const glm::vec3 &cameraPosition, const glm::mat3 &cameraOrientation, float focalLength, const std::vector<ModelTriangle> &triangles, std::vector<std::vector<float>> &depths) {
    const float minValidDepth = 0.0f;

    for (ModelTriangle triangle : triangles){
		CanvasPoint projectedPoint0 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[0],cameraOrientation);
		CanvasPoint projectedPoint1 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[1],cameraOrientation);
		CanvasPoint projectedPoint2 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[2],cameraOrientation);
		
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
			CanvasPoint projectedPoint0 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[0],cameraOrientation);
			CanvasPoint projectedPoint1 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[1],cameraOrientation);
			CanvasPoint projectedPoint2 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[2],cameraOrientation);
			
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

glm::mat3 lookAt(const glm::vec3 &camera, const glm::vec3 &target, const glm::vec3 &worldUp = {0,1,0}){ //get cameraOrientation matrix
	glm::vec3 F = glm::normalize(target - camera);
	glm::vec3 R = glm::normalize(glm::cross(F, worldUp));
	glm::vec3 U = glm::normalize(glm::cross(R, F));

	return glm::mat3(R.x,R.y,R.z, 
					 U.x,U.y,U.z, 
		            -F.x,-F.y,-F.z);
}

glm::vec3 pixelToRayDirection(int x, int y,int width, int height, float focalLength, const glm::mat3& cameraOrientation){
	//plane的左上角的坐标系转成plane中心的坐标系统的3D点的方向向量
	float scale =160.0f;
	float px = (x  - width  * 0.5f)/scale;
    float py = (y  - height * 0.5f)/scale;
    py = -py;

	glm::vec3 cameraDirection = glm::normalize(glm::vec3(px,py,-focalLength));
	glm::vec3 RayDirection = glm::normalize(cameraOrientation * cameraDirection);// 旋到真实的世界坐标
	return RayDirection;
}

RayTriangleIntersection getClosestValidIntersection(const glm::vec3 &cameraPosition,const glm::vec3 &rayDirection,std::vector<ModelTriangle>& triangles){ //lab6
    glm::vec3 direction = glm::normalize(rayDirection); //单位化
    float bestT = std::numeric_limits<float>::max(); // 当前最优（最小）t，初始为最大值
    RayTriangleIntersection bestIntercetion;
    bestIntercetion.triangleIndex =SIZE_MAX; //if not intercepted

    for (int i = 0; i < triangles.size(); i++) {

        const ModelTriangle triangle = triangles[i];

        glm::vec3 e0 = triangle.vertices[1] - triangle.vertices[0];
        glm::vec3 e1 = triangle.vertices[2] - triangle.vertices[0];
        glm::vec3 SPVector = cameraPosition - triangle.vertices[0];
        glm::mat3 DEMatrix(-direction, e0, e1);
        glm::vec3 possibleSolution = glm::inverse(DEMatrix) * SPVector;

        float t = possibleSolution.x; //沿着光线方向从相机走了多远才到交点
        float u = possibleSolution.y; //交点在三角形第 1 条边方向上的比例
        float v = possibleSolution.z; //交点在三角形第 2 条边方向上的比例
        float w =1-u-v;

        if (!((u >= 0.0) && (u <= 1.0) && (v >= 0.0) && (v <= 1.0) && (u + v) <= 1 &&t >0)) continue;        // 必须做命中判定 task3

        if(t<bestT){
            bestT = t;
            // glm::vec3 intersection = cameraPosition + t * rayDirection;
            glm::vec3 intersection = triangle.vertices[0] + u * e0 + v * e1;
            bestIntercetion.distanceFromCamera =t;
            bestIntercetion.intersectionPoint=intersection;
            bestIntercetion.triangleIndex=i;
            bestIntercetion.intersectedTriangle =triangle;
        }
    }
    return bestIntercetion;
}

bool isInShadow(const glm::vec3& point,int index,std::vector<ModelTriangle>& triangles){
    glm::vec3 toLight = light.position - point;
    glm::vec3 direction = glm::normalize(toLight);
	float distToLight = glm::length(toLight);
    if (distToLight <= 0.0f) return false;

	const float v = 1e-4f;
    glm::vec3 RayOrigin = point + direction * v; 

    RayTriangleIntersection intersection = getClosestValidIntersection(RayOrigin,direction,triangles);

    if (intersection.triangleIndex == SIZE_MAX) return false;
    if (intersection.triangleIndex == index && intersection.distanceFromCamera < 1e-4f) return false;
    return (intersection.distanceFromCamera < distToLight);
}

float calculateProximity(const glm::vec3& target,const glm::vec3& lightPosition){
	float distanceToLight = glm::length(lightPosition - target);
	float proximity = 10.0f / (4.0f * M_PI *distanceToLight * distanceToLight);
	return std::min(1.0f,proximity) ;
}

float calculateAngleOfIncidence(const glm::vec3& intersectionPoint, const glm::vec3& normal, const glm::vec3& lightPosition){
	glm::vec3 N = glm::normalize(normal);
    glm::vec3 toLight = glm::normalize(lightPosition - intersectionPoint);
    float angle = glm::dot(N, toLight);
	return std::max(0.0f, angle);
}

float calculateSpecular(const glm::vec3& intersectionPoint, const glm::vec3& normal, const glm::vec3& lightPosition, const glm::vec3& cameraPosition) {
	glm::vec3 N = glm::normalize(normal);
	glm::vec3 L = glm::normalize(lightPosition - intersectionPoint); 
	glm::vec3 V = glm::normalize(cameraPosition - intersectionPoint); 
	glm::vec3 R = 2.0f * glm::dot(N, L) * N - L;

	float specular = glm::dot(R, V); 
	return std::pow(std::max(0.0f, specular), 256);
}

// 计算顶点法线
void computeVertexNormals(const std::vector<ModelTriangle>& triangles, int startIndex = 0) {
	std::array<glm::vec3, 3> vertexNormals;

    for (int i = 0; i < triangles.size(); i++) {
        for (int v = 0; v < 3; v++) {
            glm::vec3 normal(0.0f);
            glm::vec3 vertex = triangles[i].vertices[v];
            
            // 找到所有包含这个顶点的三角形，加起来它们的法线
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

void drawRayTracedScene(DrawingWindow &window,const glm::vec3 &cameraPosition,const glm::mat3 &cOrientation,const float &focalLength,std::vector<ModelTriangle>triangles, const TextureMap *texture = nullptr){
	// Detect number of CPU cores
	unsigned int numThreads = std::thread::hardware_concurrency();
	if (numThreads == 0) numThreads = 4; // Fallback to 4 threads if detection fails
	
	// Calculate rows per thread
	int rowsPerThread = HEIGHT / numThreads;
	
	// Vector to store thread handles
	std::vector<std::thread> threads;
	
	// Lambda function to process a range of rows
	auto renderRows = [&](int startY, int endY) {
		for(int y = startY; y < endY; y++){
			for(int x = 0; x < WIDTH; x++){
				glm::vec3 rayDirection = pixelToRayDirection(x,y,WIDTH,HEIGHT,focalLength,cOrientation);
				const float v = 1e-4f;            
				glm::vec3 rayOrigin  = cameraPosition + rayDirection * v;
				RayTriangleIntersection intersection = getClosestValidIntersection(rayOrigin,rayDirection,triangles);
				
				if(intersection.triangleIndex != SIZE_MAX){
					glm::vec3 point = intersection.intersectionPoint;
					const ModelTriangle& tri = intersection.intersectedTriangle;
					
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
					
					// 用重心坐标插值顶点法线
					std::array<glm::vec3, 3> vertexNormals = vertexNormalsMap[intersection.triangleIndex];
					glm::vec3 interpolatedNormal = glm::normalize(bary_u * vertexNormals[0] + bary_v * vertexNormals[1] + bary_w * vertexNormals[2]);
					
					float proximity = calculateProximity(point, light.position);
					float angle = calculateAngleOfIncidence(point, interpolatedNormal, light.position);
					float diffuse = proximity * angle;
					float specular = calculateSpecular(point, interpolatedNormal, light.position, cameraPosition);
					
					bool IsShadow = isInShadow(point, intersection.triangleIndex, triangles);
					
					float Brightness;
					if (IsShadow){
						Brightness = light.ambient;
					} else {
						Brightness = light.ambient + diffuse + specular;
						Brightness = std::min(1.0f, Brightness);
					}
					
					bool hasTexture = (tri.texturePoints[0].x != 0 || tri.texturePoints[0].y != 0 ||tri.texturePoints[1].x != 0 || tri.texturePoints[1].y != 0 ||tri.texturePoints[2].x != 0 || tri.texturePoints[2].y != 0);
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
					
					uint32_t colour = (255u << 24) + (std::min(255u, (uint32_t)(colourValue.red * Brightness)) << 16) + (std::min(255u, (uint32_t)(colourValue.green * Brightness)) << 8) + (std::min(255u, (uint32_t)(colourValue.blue * Brightness)));
					window.setPixelColour(x, y, colour);
				}
			}
		}
	};
	
	// Spawn threads
	for (unsigned int i = 0; i < numThreads; i++) {
		int startY = i * rowsPerThread;
		int endY = (i == numThreads - 1) ? HEIGHT : (i + 1) * rowsPerThread;
		threads.emplace_back(renderRows, startY, endY);
	}
	
	// Join all threads
	for (auto& thread : threads) {
		thread.join();
	}
}

void handleEvent(SDL_Event event, DrawingWindow &window){
	float rotation = glm::radians(5.0f);      // 每次转
	float orientation = glm::radians(5.0f);   // 旋转朝向
	static float translation  = 0.25f;        // 平移

	if (event.type == SDL_KEYDOWN){
		if (event.key.keysym.sym == SDLK_LEFT){
			std::cout << "LEFT" << std::endl;
			camera.position = scene.center + rotY(rotation)*(camera.position-scene.center);
			camera.orientation = lookAt(camera.position, scene.center);
		}else if (event.key.keysym.sym == SDLK_RIGHT){
			std::cout << "RIGHT" << std::endl;
			camera.position = scene.center + rotY(-rotation)*(camera.position-scene.center);
			camera.orientation = lookAt(camera.position, scene.center);
		}else if (event.key.keysym.sym == SDLK_UP){
			std::cout << "UP" << std::endl;
			camera.position = scene.center + rotX(rotation)*(camera.position-scene.center);
			camera.orientation = lookAt(camera.position, scene.center);
		}else if (event.key.keysym.sym == SDLK_DOWN){
			std::cout << "DOWN" << std::endl;
			camera.position = scene.center + rotX(-rotation)*(camera.position-scene.center);
			camera.orientation = lookAt(camera.position, scene.center);
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

		case SDLK_j: camera.orientation = camera.orientation * rotY(+orientation); break; // j:pan 
		case SDLK_k: camera.orientation = camera.orientation * rotY(-orientation); break; // k:pan 
		case SDLK_i: camera.orientation = camera.orientation * rotX(+orientation); break; // i:tilt 
		case SDLK_o: camera.orientation = camera.orientation * rotX(-orientation); break; // o:tilt

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

	std::map<std::string, Colour> paletteTexturedBox = loadMTL("src/textured-cornell-box.mtl");
    std::vector<ModelTriangle> trianglesTexturedBox = loadOBJ("src/textured-cornell-box.obj", paletteTexturedBox, 0.35f);

    TextureMap texture("texture.ppm");

    // Logo
    // std::map<std::string, Colour> paletteLogo = loadMTL("hackspace-logo/materials.mtl");
    // std::vector<ModelTriangle> trianglesLogo = loadOBJ("hackspace-logo/logo.obj", paletteLogo, 0.0035f);

    // 添加红色球体
    std::map<std::string, Colour> paletteSphere = loadMTL("src/sphere.mtl");
    std::vector<ModelTriangle> trianglesSphere = loadOBJ("src/sphere.obj", paletteSphere, 0.4f);
	
	// add fixed translation to sphere -- z 负数是往后，正数是往前（靠近相机）
	glm::vec3 offset(0.4f, -1.1f, -0.9f);
	for (auto& tri : trianglesSphere) {
    tri.vertices[0] += offset;
    tri.vertices[1] += offset;
    tri.vertices[2] += offset;
	}
	
	// 合并所有三角形用于光线追踪
	std::vector<ModelTriangle> allTriangles;
	allTriangles.insert(allTriangles.end(), trianglesTexturedBox.begin(), trianglesTexturedBox.end());
	int sphereStartIndex = (int)allTriangles.size();  // sphere 在合并向量中的起始索引
	allTriangles.insert(allTriangles.end(), trianglesSphere.begin(), trianglesSphere.end());
	
	// 计算所有模型的顶点法线用于 Gouraud Shading
	computeVertexNormals(trianglesTexturedBox, 0);
	computeVertexNormals(trianglesSphere, sphereStartIndex);

	// FPS计数
	int fps = 0;
	auto lastTime = std::chrono::high_resolution_clock::now();

	while (true){
		if (window.pollForInputEvents(event))
			handleEvent(event, window);

		if (camera.autoRotate) {
        	camera.position = scene.center + rotY(glm::radians(0.5f)) * (camera.position - scene.center);
        	camera.orientation = lookAt(camera.position, scene.center);
    	} 

	window.clearPixels();
	
	// 为每一帧创建一次深度缓冲，所有物体共享
	std::vector<std::vector<float>> depths(HEIGHT, std::vector<float>(WIDTH, 0.0f));
	
	switch (scene.mode) {
        case RenderMode::Wireframe:
            drawWireframeScene(window, camera.position, camera.orientation, camera.focalLength, trianglesTexturedBox, depths);
			// drawWireframeScene(window, camera.position, camera.orientation, camera.focalLength, trianglesLogo, depths);
			drawWireframeScene(window, camera.position, camera.orientation, camera.focalLength, trianglesSphere, depths);
            break;
        case RenderMode::Rasterised:
            // 传递纹理给 texturedBox，自动判断每个三角形是否有纹理
            drawRasterisedFilledScene(window, camera.position, camera.orientation, camera.focalLength, trianglesTexturedBox, depths, &texture);
			// drawRasterisedFilledScene(window, camera.position, camera.orientation, camera.focalLength, trianglesLogo, depths, nullptr);
			drawRasterisedFilledScene(window, camera.position, camera.orientation, camera.focalLength, trianglesSphere, depths, nullptr);
            break;
        case RenderMode::RayTraced:
			drawRayTracedScene(window, camera.position, camera.orientation, camera.focalLength, allTriangles, &texture);
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