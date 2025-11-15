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
#define WIDTH  640
#define HEIGHT 480

enum class RenderMode { Wireframe = 1, Rasterised = 2, RayTraced = 3 };
RenderMode Mode = RenderMode::Rasterised;

bool autoRotation = false;
glm::vec3 cameraPosition(0.0f, 0.0f, 4.0f);
float focalLength = 2.0f;
const glm::vec3 sceneCenter = {0.0f, 0.0f, 0.0f};

glm::mat3 cameraOrientation(1.0f);   // 单位矩阵，表示"朝向不转"

glm::vec3 lightPosition(0.0f, 0.9f, 0.0f);

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
        }else if (tokens[0]=="usemtl")
        {
            colourName = tokens[1];
            colour = palette.at(colourName);
        }
        else if (tokens[0] == "f"){
            glm::vec3 v0 = vertices[std::stoi(tokens[1])-1]; //String To Integer avoid /
            glm::vec3 v1 = vertices[std::stoi(tokens[2])-1];
            glm::vec3 v2 = vertices[std::stoi(tokens[3])-1];
            triangles.push_back(ModelTriangle(v0, v1, v2, colour));
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

CanvasPoint projectVertexOntoCanvasPoint(const glm::vec3 &cameraPosition,float focalLength, glm::vec3 &vertexPosition,const glm::mat3 &cameraOrientation){
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

glm::vec3 getNormalVector(const ModelTriangle& tri){
    glm::vec3 e0 = tri.vertices[1] - tri.vertices[0];
    glm::vec3 e1 = tri.vertices[2] - tri.vertices[0];
    return glm::normalize(glm::cross(e0, e1));
}

RayTriangleIntersection getClosestValidIntersection(const glm::vec3 &cameraPosition,const glm::vec3 &rayDirection,std::vector<ModelTriangle>& triangles){ //lab6
    glm::vec3 direction = glm::normalize(rayDirection); //单位化
    float bestT = std::numeric_limits<float>::infinity(); // 当前最优（最小）t，初始为 +∞
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

void fillFlatTopTriangle(DrawingWindow &window, CanvasPoint bottom, CanvasPoint mid, CanvasPoint extraP, Colour colour, std::vector<std::vector<float>> &depths, CanvasPoint v0, CanvasPoint v1, CanvasPoint v2){ // 平顶三角形
	CanvasPoint midLp, midRp;
	if (mid.x > extraP.x)
	{
		midRp = mid;
		midLp = extraP;
	}
	else
	{
		midRp = extraP;
		midLp = mid;
	}

	float dxL = (bottom.x - midLp.x) / (bottom.y - midLp.y); // 每下降1个像素行，x变化多少
	float dxR = (bottom.x - midRp.x) / (bottom.y - midRp.y);

	int yStart = (int)midLp.y + 1;
	int yEnd = (int)bottom.y;

	uint32_t colourValue = (255u << 24) + (uint32_t(colour.red) << 16) + (uint32_t(colour.green) << 8) + (uint32_t(colour.blue));

	for (int y = yStart; y < yEnd; y++){
		float xLstart = midLp.x + (y - midLp.y) * dxL;
        float xRstart = midRp.x + (y - midRp.y) * dxR;
        
		for (int x = xLstart; x < xRstart; x++){ //遍历每个像素
			if (y >= 0 && y < HEIGHT && x >= 0 && x < WIDTH) {
				// 使用重心坐标计算深度（和你的 store.cpp 一样的顺序）
				glm::vec3 baryCentric = convertToBarycentricCoordinates(
					glm::vec2(v0.x, v0.y), 
					glm::vec2(v1.x, v1.y), 
					glm::vec2(v2.x, v2.y), 
					glm::vec2(x, y)
				);
				
				// 注意顺序：baryCentric.z * v0 + baryCentric.x * v1 + baryCentric.y * v2
				float pixelDepth = baryCentric.z * v0.depth + baryCentric.x * v1.depth + baryCentric.y * v2.depth;
				
				if (pixelDepth > depths[y][x]) {
					window.setPixelColour(x, y, colourValue);
					depths[y][x] = pixelDepth;
				}
			}
		}
	}
}

void fillFlatBottomTriangle(DrawingWindow &window, CanvasPoint top, CanvasPoint mid, CanvasPoint extraP, Colour colour, std::vector<std::vector<float>> &depths, CanvasPoint v0, CanvasPoint v1, CanvasPoint v2){ // 顶角三角形
	CanvasPoint midLp, midRp;
	if (mid.x > extraP.x){
		midRp = mid;
		midLp = extraP;
	}
	else{
		midRp = extraP;
		midLp = mid;
	}

	float dxL = (midLp.x - top.x) / (midLp.y - top.y); // 每1个像素行，x变化多少
	float dxR = (midRp.x - top.x) / (midRp.y - top.y);

	int yStart = (int)top.y + 1;
	int yEnd = (int)midLp.y;

	uint32_t colourValue = (255u << 24) + (uint32_t(colour.red) << 16) + (uint32_t(colour.green) << 8) + (uint32_t(colour.blue));
	for (int y = yStart; y <= yEnd; y++){ // y<=
		float xLstart = top.x + (y - top.y) * dxL;
        float xRstart = top.x + (y - top.y) * dxR;

		for (int x = xLstart; x < xRstart; x++){
			if (y >= 0 && y < HEIGHT && x >= 0 && x < WIDTH) {
				// 使用重心坐标计算深度（和你的 store.cpp 一样的顺序）
				glm::vec3 baryCentric = convertToBarycentricCoordinates(
					glm::vec2(v0.x, v0.y), 
					glm::vec2(v1.x, v1.y), 
					glm::vec2(v2.x, v2.y), 
					glm::vec2(x, y)
				);
				
				// 注意顺序：baryCentric.z * v0 + baryCentric.x * v1 + baryCentric.y * v2
				float pixelDepth = baryCentric.z * v0.depth + baryCentric.x * v1.depth + baryCentric.y * v2.depth;
				
				if (pixelDepth > depths[y][x]) {
					window.setPixelColour(x, y, colourValue);
					depths[y][x] = pixelDepth;
				}
			}
		}	
	}
}

void drawFilledTriangle(DrawingWindow &window, const CanvasTriangle &triangle, const Colour &colour, std::vector<std::vector<float>> &depths){ // Rasteriser
	std::vector<CanvasPoint> sortedPs = sortByY(triangle);
	CanvasPoint extraP = FindExtraP(sortedPs);
	CanvasPoint top = sortedPs[0];
	CanvasPoint mid = sortedPs[1];
	CanvasPoint bottom = sortedPs[2];
	
	// 传入原始三角形顶点用于重心坐标计算
	fillFlatTopTriangle(window, bottom, mid, extraP, colour, depths, triangle.vertices[0], triangle.vertices[1], triangle.vertices[2]);
	fillFlatBottomTriangle(window, top, mid, extraP, colour, depths, triangle.vertices[0], triangle.vertices[1], triangle.vertices[2]);

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

bool isInShadow(const glm::vec3& point,int index,std::vector<ModelTriangle>& triangles){ //一个点在不在阴影里面（检测像素到相机碰撞的）
    glm::vec3 toLight = lightPosition - point;//从 p 往光源发射阴影光线
    glm::vec3 direction = glm::normalize(toLight);
	float distToLight = glm::length(toLight);
    if (distToLight <= 0.0f) return false; // 极端：光源与点重合

	const float v = 1e-4f;                       // 可调：1e-5 ~ 5e-4
    glm::vec3 RayOrigin = point + direction * v; 

    RayTriangleIntersection intersection = getClosestValidIntersection(RayOrigin,direction,triangles);

    if (intersection.triangleIndex == SIZE_MAX) return false;                                   // 没命中 → 不遮
    if (intersection.triangleIndex == index && intersection.distanceFromCamera < 1e-4f) return false; // 撞回自己
    return (intersection.distanceFromCamera < distToLight);                                     // 只接受光源之前的命中
}

void drawRayTracedScene(DrawingWindow &window,const glm::vec3 &cameraPosition,const glm::mat3 &cOrientation,const float &focalLength,std::vector<ModelTriangle>triangles){
	for(int y = 0; y< HEIGHT;y++){
		for(int x =0; x<WIDTH; x++ ){
			glm::vec3 rayDirection = pixelToRayDirection(x,y,WIDTH,HEIGHT,focalLength,cOrientation);
			const float v = 1e-4f;            
			glm::vec3 rayOrigin  = cameraPosition + rayDirection * v;
			RayTriangleIntersection intersection = getClosestValidIntersection(rayOrigin,rayDirection,triangles);
			if(!(intersection.triangleIndex == SIZE_MAX)){

				glm::vec3 point = intersection.intersectionPoint;
    			const ModelTriangle& tri = intersection.intersectedTriangle;
				bool IsShadow = isInShadow(point,intersection.triangleIndex, triangles);
				if (IsShadow){
					window.setPixelColour(x, y, 0xFF000000u);
				}else{
					uint32_t colourValue = (255u << 24) + (uint32_t(intersection.intersectedTriangle.colour.red) << 16) + (uint32_t(intersection.intersectedTriangle.colour.green) << 8) + (uint32_t(intersection.intersectedTriangle.colour.blue));
					window.setPixelColour(x, y, colourValue);
				}
			}
		}
	}
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
		drawStrokedTriangle(window, canvasTriangle, triangle.colour, depths);  // 使用三角形的颜色
    }
}

void drawRasterisedFilledScene(DrawingWindow &window, const glm::vec3 &cameraPosition, const glm::mat3 &cameraOrientation, float focalLength, const std::vector<ModelTriangle> &triangles, std::vector<std::vector<float>> &depths) {
	const float minValidDepth = 0.0f;
	for (ModelTriangle triangle : triangles){
			CanvasPoint projectedPoint0 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[0],cameraOrientation);
			CanvasPoint projectedPoint1 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[1],cameraOrientation);
			CanvasPoint projectedPoint2 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[2],cameraOrientation);
			
			// depth = 1/-z，makesure depth not be too small
			if (projectedPoint0.depth < minValidDepth  ||projectedPoint1.depth < minValidDepth ||projectedPoint2.depth < minValidDepth ) {
				continue;
			}
			
			CanvasTriangle canvasTriangle(projectedPoint0,projectedPoint1,projectedPoint2);
			drawFilledTriangle(window,canvasTriangle,triangle.colour,depths);
	}
}


void handleEvent(SDL_Event event, DrawingWindow &window){
	float rotation = glm::radians(5.0f);      // 每次转
	float orientation = glm::radians(5.0f);   // 旋转朝向
	static float translation  = 0.25f;        // 平移

	if (event.type == SDL_KEYDOWN){
		if (event.key.keysym.sym == SDLK_LEFT){
			std::cout << "LEFT" << std::endl;
			cameraPosition = sceneCenter + rotY(rotation)*(cameraPosition-sceneCenter); // 绕任意点sceneCenter 旋转的标准公式：p' = scenceCenter + R * (p - scenceCenter)
			cameraOrientation = lookAt(cameraPosition, sceneCenter);
		}else if (event.key.keysym.sym == SDLK_RIGHT){
			std::cout << "RIGHT" << std::endl;
			cameraPosition = sceneCenter + rotY(-rotation)*(cameraPosition-sceneCenter);
			cameraOrientation = lookAt(cameraPosition, sceneCenter);
		}else if (event.key.keysym.sym == SDLK_UP){
			std::cout << "UP" << std::endl;
			cameraPosition = sceneCenter + rotX(rotation)*(cameraPosition-sceneCenter);
			cameraOrientation = lookAt(cameraPosition, sceneCenter);
		}else if (event.key.keysym.sym == SDLK_DOWN){
			std::cout << "DOWN" << std::endl;
			cameraPosition = sceneCenter + rotX(-rotation)*(cameraPosition-sceneCenter);
			cameraOrientation = lookAt(cameraPosition, sceneCenter);
		}
		switch (event.key.keysym.sym){
		case SDLK_1:Mode = RenderMode::Wireframe;break;
        case SDLK_2:Mode = RenderMode::Rasterised;break;
        case SDLK_3:Mode = RenderMode::RayTraced;break;

		case SDLK_a: cameraPosition.x = cameraPosition.x - translation;break;
		case SDLK_d: cameraPosition.x = cameraPosition.x + translation;break;
		case SDLK_w: cameraPosition.y = cameraPosition.y + translation;break;
		case SDLK_s: cameraPosition.y = cameraPosition.y - translation;break;
		case SDLK_q: cameraPosition.z = cameraPosition.z - translation;break;
		case SDLK_e: cameraPosition.z = cameraPosition.z + translation;break;

		case SDLK_j: cameraOrientation = cameraOrientation * rotY(+orientation); break; // j:pan 左
		case SDLK_k: cameraOrientation = cameraOrientation * rotY(-orientation); break; // k:pan 右
		case SDLK_i: cameraOrientation = cameraOrientation * rotX(+orientation); break; // i:tilt 上
		case SDLK_o: cameraOrientation = cameraOrientation * rotX(-orientation); break; // o:tilt 下

		case SDLK_r: autoRotation = !autoRotation; break;
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

	std::map<std::string, Colour> paletteCornellBox = loadMTL("cornell-box.mtl");
    std::vector<ModelTriangle> trianglesCornellBox = loadOBJ("cornell-box.obj", paletteCornellBox, 0.35f);

    std::map<std::string, Colour> paletteLogo = loadMTL("hackspace-logo/materials.mtl");
    std::vector<ModelTriangle> trianglesLogo = loadOBJ("hackspace-logo/logo.obj", paletteLogo, 0.0035f);

	while (true){

		if (window.pollForInputEvents(event))
			handleEvent(event, window);

		if (autoRotation) {
        	cameraPosition = sceneCenter + rotY(glm::radians(0.5f)) * (cameraPosition - sceneCenter);
        	cameraOrientation = lookAt(cameraPosition, sceneCenter);
    	} 

	window.clearPixels();
	
	// 为每一帧创建一次深度缓冲，所有物体共享
	std::vector<std::vector<float>> depths(HEIGHT, std::vector<float>(WIDTH, 0.0f));
	
	switch (Mode) {
        case RenderMode::Wireframe:
            drawWireframeScene(window, cameraPosition, cameraOrientation, focalLength, trianglesCornellBox, depths);
			drawWireframeScene(window, cameraPosition, cameraOrientation, focalLength, trianglesLogo, depths);
            break;
        case RenderMode::Rasterised:
            drawRasterisedFilledScene(window, cameraPosition, cameraOrientation, focalLength, trianglesCornellBox, depths);
			drawRasterisedFilledScene(window, cameraPosition, cameraOrientation, focalLength, trianglesLogo, depths);
            break;
        case RenderMode::RayTraced:
            drawRayTracedScene(window, cameraPosition, cameraOrientation, focalLength, trianglesCornellBox);
            break;
    	}
		window.renderFrame();

		static int frame = 0; char name[128];
        sprintf(name, "frames/frame_%04d.bmp", frame++);
        window.saveBMP(name);
	}
	return 0;
}