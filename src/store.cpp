#include <CanvasTriangle.h>
#include <DrawingWindow.h>
#include <Utils.h>
#include <fstream>
#include <vector>
#include <iostream>
#include <CanvasPoint.h>
#include <Colour.h>
#include <CanvasTriangle.h>
#include <TextureMap.h>
#include <cmath>
using namespace std; // using namespace std;：让你可以写 vector, cout 这种不加 std:: 前缀的简写。
enum class RenderMode { Wireframe = 0, Rasterised = 1, RayTraced = 2 };
RenderMode Mode = RenderMode::Rasterised;

#define WIDTH 320
#define HEIGHT 240


glm::vec3 cameraPosition(0.0f, 0.0f, 4.0f);
float focalLength = 2.0f;
const glm::vec3 sceneCenter = {0.0f, 0.0f, 0.0f};

glm::mat3 cameraOrientation(1.0f);   // 单位矩阵，表示“朝向不转”

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

void draw(DrawingWindow &window)
{
	window.clearPixels();
	for (size_t y = 0; y < window.height; y++)
	{
		for (size_t x = 0; x < window.width; x++)
		{
			float red = rand() % 256;
			float green = 0.0;
			float blue = 0.0;
			uint32_t colour = (255 << 24) + (int(red) << 16) + (int(green) << 8) + int(blue);
			window.setPixelColour(x, y, colour);
		}
	}
}

void drawWtoB(DrawingWindow &window) // 画白到黑渐变
{
	window.clearPixels();
	for (size_t y = 0; y < window.height; y++)
	{
		for (size_t x = 0; x < window.width; x++)
		{
			float ratio = float(x) / float((window.width - 1)); // 要用浮点除法，不然3 / 2 == 1（不是 1.5）
			float gray = (1 - ratio) * 255.0;
			uint32_t colour = (255 << 24) + (int(gray) << 16) + (int(gray) << 8) + int(gray); // ARGB 灰色的关键：让 R = G = B = 同一个值  << left shift
			window.setPixelColour(x, y, colour);
		}
	}
}

// std::cout 是标准输出流
// v[i] 取第 i 个元素（下标从 0 开始）
// << 是“流插入运算符”，把右边的内容送进左边的输出流
// std::endl 是换行并刷新缓冲区（等价更快的做法是 "\n"）
vector<float> interpolateSingleFloats(float from, float to, int numberOfValues)
{
	vector<float> result;
	float step = (to - from) / float(numberOfValues - 1);
	for (int i = 0; i < numberOfValues; i++)
	{
		result.push_back(from + i * step);
	}
	return result;
}

vector<glm::vec3> interpolateThreeElementValues(glm::vec3 from, glm::vec3 to, int numberOfValues)
{
	vector<glm::vec3> result;
	glm::vec3 step = (to - from) / float(numberOfValues - 1);
	for (int i = 0; i < numberOfValues; i++)
	{
		result.push_back(from + float(i) * step);
	}
	return result;
}

void drawColoRectangle(DrawingWindow &window)
{ // 画彩色矩形
	window.clearPixels();

	glm::vec3 topLeft(255, 0, 0);	   // red
	glm::vec3 topRight(0, 0, 255);	   // blue
	glm::vec3 bottomRight(0, 255, 0);  // green
	glm::vec3 bottomLeft(255, 255, 0); // yellow
	std::vector<glm::vec3> firstCol = interpolateThreeElementValues(topLeft, bottomLeft, HEIGHT);
	std::vector<glm::vec3> lastCol = interpolateThreeElementValues(topRight, bottomRight, HEIGHT);
	for (int y = 0; y < HEIGHT - 1; y++)
	{
		std::vector<glm::vec3> row = interpolateThreeElementValues(firstCol[y], lastCol[y], WIDTH); // 第一列第一个 到最后一列第一个的每个像素的颜色
		for (int x = 0; x < WIDTH - 1; x++)
		{ // 循环写入颜色
			uint32_t colour = (255 << 24) + (uint32_t(row[x][0]) << 16) + (uint32_t(row[x][1]) << 8) + (uint32_t(row[x][2]));
			window.setPixelColour(x, y, colour);
		}
	}
}

void drawColoTriangle(DrawingWindow &window)
{ // 画彩色三角形
	window.clearPixels();

	glm::vec2 topCoordinate(WIDTH / 2, 0);
	glm::vec2 bottomLeftCoordinate(0, HEIGHT);
	glm::vec2 bottomRightCoordinate(WIDTH, HEIGHT);
	glm::vec3 top(0, 255, 0);		  // green
	glm::vec3 bottomLeft(255, 0, 0);  // red
	glm::vec3 bottomRight(0, 0, 255); // blue
	glm::vec3 Bary;

	for (int y = 0; y < HEIGHT - 1; y++)
	{
		for (int x = 0; x < WIDTH - 1; x++)
		{
			glm::vec2 p(x, y);
			Bary = convertToBarycentricCoordinates(topCoordinate, bottomLeftCoordinate, bottomRightCoordinate, p);
			if (Bary[0] > 0 && Bary[1] > 0 && Bary[2] > 0)
			{
				glm::vec3 pixel = Bary[0] * bottomLeft + Bary[1] * bottomRight + Bary[2] * top; // 左右上 存储的RGB
				uint32_t colour = (255 << 24) + (uint32_t(pixel[0]) << 16) + (uint32_t(pixel[1]) << 8) + (uint32_t(pixel[2]));
				window.setPixelColour(x, y, colour);
			}
		}
	}

	// std::vector<glm::vec3> firstCol = interpolateThreeElementValues();
	// std::vector<glm::vec3> lastCol = interpolateThreeElementValues();
	// for (int y = 0; y < HEIGHT-1; y++){
	// 	std::vector<glm::vec3> row = interpolateThreeElementValues(firstCol[y],lastCol[y],WIDTH);//第一列第一个 到最后一列第一个的每个像素的颜色
	// 	for (int x = 0; x < WIDTH-1; x++){ //循环写入颜色
	// 		uint32_t colour = (255 << 24) + (uint32_t(row[x][0]) << 16) + (uint32_t(row[x][1]) << 8) +(uint32_t(row[x][2]));
	// 		window.setPixelColour(x,y,colour);
	// 	}
	// }
}

void drawLine(DrawingWindow &window, const CanvasPoint &from, const CanvasPoint &to, const Colour &colour)
{
	float xDiff = to.x - from.x;
	float yDiff = to.y - from.y;
	float numberOfSteps = max(abs(xDiff), abs(yDiff));
	float xStepSize = xDiff / numberOfSteps;
	float yStepSize = yDiff / numberOfSteps;
	uint32_t colourValue = (255u << 24) + (uint32_t(colour.red) << 16) + (uint32_t(colour.green) << 8) + uint32_t(colour.blue);
	for (float i = 0.0; i <= numberOfSteps; i++)
	{
		float x = from.x + xStepSize * i;
		float y = from.y + yStepSize * i;

		if (x >= 0 && x < window.width && y >= 0 && y < window.height)
		{
			window.setPixelColour(round(x), round(y), colourValue);
		}
	}
}

void drawStrokedTriangle(DrawingWindow &window, const CanvasTriangle &triangle, const Colour &colour)
{
	drawLine(window, triangle.vertices[0], triangle.vertices[1], colour);
	drawLine(window, triangle.vertices[1], triangle.vertices[2], colour);
	drawLine(window, triangle.vertices[2], triangle.vertices[0], colour);
}

vector<CanvasPoint> sortByY(const CanvasTriangle &triangle)
{
	CanvasPoint v0 = triangle.vertices[0]; // top
	CanvasPoint v1 = triangle.vertices[1]; // mid
	CanvasPoint v2 = triangle.vertices[2]; // bottom

	if (v1.y < v0.y)
		std::swap(v0, v1);
	if (v2.y < v0.y)
		std::swap(v0, v2);
	if (v2.y < v1.y)
		std::swap(v1, v2);

	vector<CanvasPoint> sortedPs;
	sortedPs.push_back(v0);
	sortedPs.push_back(v1);
	sortedPs.push_back(v2);

	return sortedPs;
}

CanvasPoint FindExtraP(vector<CanvasPoint> sortedPs){ // also for tuxture point
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

void fillFlatTopTriangle(DrawingWindow &window, CanvasPoint bottom, CanvasPoint mid, CanvasPoint extraP, Colour colour,std::vector<std::vector<float>> &depths)
{ // 平顶三角形
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

	//add depth rate -lab-4-task 9 y变化一个，深度变化多少
	float dDepthxL =(bottom.depth - midLp.depth) / (bottom.y - midLp.y);
	float dDepthxR =(bottom.depth - midRp.depth) / (bottom.y - midRp.y);

	int yStart = (int)midLp.y + 1;
	int yEnd = (int)bottom.y;

	uint32_t colourValue = (255u << 24) + (uint32_t(colour.red) << 16) + (uint32_t(colour.green) << 8) + (uint32_t(colour.blue));

	for (int y = yStart; y < yEnd; y++){
		float xLstart = midLp.x + (y - midLp.y) * dxL;
        float xRstart = midRp.x + (y - midRp.y) * dxR;
        float depthLstart = midLp.depth + (y - midLp.y) * dDepthxL;
        float depthRstart = midRp.depth + (y - midRp.y) * dDepthxR;
		for (int x = xLstart; x < xRstart; x++){ //遍历每个像素
			if (y >= 0 && y < HEIGHT && x >= 0 && x < WIDTH) {
				float ratio = (x - xLstart) / (xRstart - xLstart);// 计算 x 在 [xL, xR] 区间上的比例
				float pixelDepth = depthLstart + ratio * (depthRstart - depthLstart);
				if (pixelDepth > depths[y][x]) {
					window.setPixelColour(x, y, colourValue);
					depths[y][x] = pixelDepth;
				}
			}
		}
	}
}

void fillFlatBottomTriangle(DrawingWindow &window, CanvasPoint top, CanvasPoint mid, CanvasPoint extraP, Colour colour,std::vector<std::vector<float>> &depths){ // 顶角三角形
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
	//for depths
	float dDepthxL = (midLp.depth - top.depth) / (midLp.y - top.y); // 每1个像素行，depth变化多少
	float dDepthxR = (midRp.depth - top.depth) / (midRp.y - top.y);

	int yStart = (int)top.y + 1;
	int yEnd = (int)midLp.y;

	uint32_t colourValue = (255u << 24) + (uint32_t(colour.red) << 16) + (uint32_t(colour.green) << 8) + (uint32_t(colour.blue));
	for (int y = yStart; y <= yEnd; y++){ // y<=
		float xLstart = top.x + (y - top.y) * dxL;
        float xRstart = top.x + (y - top.y) * dxR;
        float depthLstart = top.depth + (y - top.y) * dDepthxL;
        float depthRstart = top.depth + (y - top.y) * dDepthxR;

		for (int x = xLstart; x < xRstart; x++){
			if (y >= 0 && y < HEIGHT && x >= 0 && x < WIDTH) {
				float ratio = (x - xLstart) / (xRstart - xLstart);// 计算 x 在 [xL, xR] 区间上的比例
				float pixelDepth =depthLstart + ratio * (depthRstart - depthLstart);
				if (pixelDepth > depths[y][x]) {
					window.setPixelColour(x, y, colourValue);
					depths[y][x] = pixelDepth;
				}
			}
		}	
	}
}

void fillTriangleBybary(DrawingWindow &window, CanvasTriangle triangle, Colour colour,std::vector<std::vector<float>> &depths){
	
	const CanvasPoint v0 = triangle.vertices[0];
    const CanvasPoint v1 = triangle.vertices[1];
    const CanvasPoint v2 = triangle.vertices[2];

	for (int y = 0; y < window.height; y++){
		for (int x = 0; x < window.width; x++){
			glm::vec3 baryCentric = convertToBarycentricCoordinates(glm::vec2(v0.x, v0.y), glm::vec2(v1.x, v1.y), glm::vec2(v2.x, v2.y), glm::vec2(x, y));
			if (baryCentric.x >= 0 && baryCentric.y >= 0 && baryCentric.z >= 0) {
				float depth = baryCentric.z * v0.depth +baryCentric.x * v1.depth +baryCentric.y * v2.depth; //care order
				if (depth > depths[y][x]){
					uint32_t colourValue = (255u << 24) + (uint32_t(colour.red) << 16) + (uint32_t(colour.green) << 8) + (uint32_t(colour.blue));
					window.setPixelColour(x,y,colourValue);
					depths[y][x] = depth;
				}
			}
		}
	}
}

void drawFilledTriangle(DrawingWindow &window, const CanvasTriangle &triangle, const Colour &colour, std::vector<std::vector<float>> &depths)
{ // Rasteriser
	vector<CanvasPoint> sortedPs = sortByY(triangle);
	CanvasPoint extraP = FindExtraP(sortedPs);
	CanvasPoint top = sortedPs[0];
	CanvasPoint mid = sortedPs[1];
	CanvasPoint bottom = sortedPs[2];
	fillFlatTopTriangle(window, bottom, mid, extraP, colour,depths);
	fillFlatBottomTriangle(window, top, mid, extraP, colour,depths);

	// fillTriangleBybary(window,triangle,colour,depths);

	// Colour white(255, 255, 255);
	// drawStrokedTriangle(window, triangle, white);
}

// lab -3 TASK 5

void fillFlatTopTexturedTriangle(DrawingWindow &window, CanvasPoint bottom, CanvasPoint mid, CanvasPoint extraP, const TextureMap &Texture)
{
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
	float xLstart = midLp.x + (yStart - midLp.y) * dxL;
	float xRstart = midRp.x + (yStart - midRp.y) * dxR;

	// 计算纹理图上的 (u,v) 变化值
	float du_dy_L = (bottom.texturePoint.x - midLp.texturePoint.x) / (bottom.y - midLp.y); // 每下降1个像素行，x变化多少
	float dv_dy_L = (bottom.texturePoint.y - midLp.texturePoint.y) / (bottom.y - midLp.y); // 每下降1个像素行，y变化多少
	float du_dy_R = (bottom.texturePoint.x - midRp.texturePoint.x) / (bottom.y - midRp.y);
	float dv_dy_R = (bottom.texturePoint.y - midRp.texturePoint.y) / (bottom.y - midRp.y);

	// 把起始行对应的 u,v 也对齐到 yStart
	float uL = midLp.texturePoint.x + (yStart - midLp.y) * du_dy_L;
	float vL = midLp.texturePoint.y + (yStart - midLp.y) * dv_dy_L;
	float uR = midRp.texturePoint.x + (yStart - midRp.y) * du_dy_R;
	float vR = midRp.texturePoint.y + (yStart - midRp.y) * dv_dy_R;

	for (float y = yStart; y < yEnd; y++)
	{
		for (float x = xLstart; x < xRstart; x++)
		{
			float ratio = (x - xLstart) / (xRstart - xLstart); // 表示从左到右走了多少“百分比
			// float u = uL + ratio * (uR - uL);				   // u,v 是这个屏幕像素在纹理图里对应的位置。
			float v = vL + ratio * (vR - vL);
			size_t idx = (size_t)v * (size_t)Texture.width + (size_t)x;
			window.setPixelColour(x, y, Texture.pixels[idx]);
		}
		xLstart = xLstart + dxL;
		xRstart = xRstart + dxR;
		uL = uL + du_dy_L;
		vL = vL + dv_dy_L;
		uR = uR + du_dy_R;
		vR = vR + dv_dy_R;
	}
}

void fillFlatBottomTexturedTriangle(DrawingWindow &window, CanvasPoint top, CanvasPoint mid, CanvasPoint extraP, const TextureMap &Texture)
{
	CanvasPoint midLp, midRp; // 分割的左右中心点
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

	float dxL = (midLp.x - top.x) / (midLp.y - top.y); // 每1个像素行，左边x变化多少
	float dxR = (midRp.x - top.x) / (midRp.y - top.y); // 每1个像素行，右边x变化多少

	int yStart = top.y + 1;
	int yEnd = midLp.y;
	float xLstart = top.x + (yStart - top.y) * dxL;
	float xRstart = top.x + (yStart - top.y) * dxR;

	// 纹理 (u,v) 随 y 的斜率（两条边）
	float du_dy_L = (midLp.texturePoint.x - top.texturePoint.x) / (midLp.y - top.y);
	float dv_dy_L = (midLp.texturePoint.y - top.texturePoint.y) / (midLp.y - top.y);
	float du_dy_R = (midRp.texturePoint.x - top.texturePoint.x) / (midRp.y - top.y);
	float dv_dy_R = (midRp.texturePoint.y - top.texturePoint.y) / (midRp.y - top.y);

	// 对齐到 yStart 的左右端 (u,v)
	float uL = top.texturePoint.x + (yStart - top.y) * du_dy_L;
	float vL = top.texturePoint.y + (yStart - top.y) * dv_dy_L;
	float uR = top.texturePoint.x + (yStart - top.y) * du_dy_R;
	float vR = top.texturePoint.y + (yStart - top.y) * dv_dy_R;

	for (float y = yStart; y <= yEnd; y++)
	{
		for (float x = xLstart; x < xRstart; x++)
		{
			float ratio = (x - xLstart) / (xRstart - xLstart); // 表示从左到右走了多少“百分比
			float u = uL + ratio * (uR - uL);				   // u,v 是这个屏幕像素在纹理图里对应的位置。
			float v = vL + ratio * (vR - vL);
			int tx = (int)u; // 纹理列（x）
			int ty = (int)v; // 纹理行（y）
			int idx = ty * Texture.width + tx;
			window.setPixelColour(x, y, Texture.pixels[idx]);
		}
		xLstart = xLstart + dxL;
		xRstart = xRstart + dxR;
		uL = uL + du_dy_L;
		vL = vL + dv_dy_L;
		uR = uR + du_dy_R;
		vR = vR + dv_dy_R;
	}
}

void drawTexturedTriangle(DrawingWindow &window, const CanvasTriangle &triangle, const TextureMap &texture)
{
	vector<CanvasPoint> sortedPs = sortByY(triangle);
	CanvasPoint extraP = FindExtraP(sortedPs);
	CanvasPoint top = sortedPs[0];
	CanvasPoint mid = sortedPs[1];
	CanvasPoint bottom = sortedPs[2];
	fillFlatTopTexturedTriangle(window, bottom, mid, extraP, texture);
	fillFlatBottomTexturedTriangle(window, top, mid, extraP, texture);
	drawStrokedTriangle(window, triangle, Colour(255, 255, 255));
}

void drawTexturedTriangleBybary(DrawingWindow &window, CanvasTriangle triangle, TextureMap &texture){
	CanvasPoint v0 = triangle.vertices[0];
	CanvasPoint v1 = triangle.vertices[1];
	CanvasPoint v2 = triangle.vertices[2];

	glm::vec2 t0(v0.texturePoint.x, v0.texturePoint.y);
	glm::vec2 t1(v1.texturePoint.x, v1.texturePoint.y);
	glm::vec2 t2(v2.texturePoint.x, v2.texturePoint.y);

	if (t0.y < t1.y)
		std::swap(t0, t1);
	if (t0.y < t2.y)
		std::swap(t0, t2);
	if (t1.y < t2.y)
		std::swap(t1, t2);

	// 若 w = (0.2, 0.3, 0.5)，且t0=(100,20), t1=(140,40), t2=(220,10)，
	// 那 texturePoint = 0.2*(100,20) + 0.3*(140,40) + 0.5*(220,10) = (176,20)。→ 说明屏幕上的这个像素应该去纹理的 (u=176, v=20) 取色

	for (size_t y = 0; y < window.height; y++){
		for (size_t x = 0; x < window.width; x++){
			glm::vec3 baryCentric = convertToBarycentricCoordinates(glm::vec2(v0.x, v0.y), glm::vec2(v1.x, v1.y), glm::vec2(v2.x, v2.y), glm::vec2(x, y));
			if (baryCentric.x >= 0 && baryCentric.y >= 0 && baryCentric.z >= 0){
				glm::vec2 texturePoint = baryCentric.x * t0 + baryCentric.y * t1 + baryCentric.z * t2;

				uint32_t colour = texture.pixels[round(texturePoint.y) * round(texture.width) + round(texturePoint.x)]; // 行号 * 宽度 + 列号
				window.setPixelColour(x, y, colour);
			}
		}
	}
	drawStrokedTriangle(window, CanvasTriangle(v0, v1, v2), Colour(255, 255, 255));
}

void handleEvent(SDL_Event event, DrawingWindow &window){
	float rotation = glm::radians(10.0f);      // 每次转 10度
	float orientation = glm::radians(10.0f);   // 旋转朝向 10度

	if (event.type == SDL_KEYDOWN){
		if (event.key.keysym.sym == SDLK_LEFT){
			std::cout << "LEFT" << std::endl;
			cameraPosition = sceneCenter + rotY(rotation)*(cameraPosition-sceneCenter); // 绕任意点sceneCenter 旋转的标准公式：p' = scenceCenter + R * (p - scenceCenter)
		}else if (event.key.keysym.sym == SDLK_RIGHT){
			std::cout << "RIGHT" << std::endl;
			cameraPosition = sceneCenter + rotY(-rotation)*(cameraPosition-sceneCenter);
		}else if (event.key.keysym.sym == SDLK_UP){
			std::cout << "UP" << std::endl;
			cameraPosition = sceneCenter + rotX(rotation)*(cameraPosition-sceneCenter);
		}else if (event.key.keysym.sym == SDLK_DOWN){
			std::cout << "DOWN" << std::endl;
			cameraPosition = sceneCenter + rotX(-rotation)*(cameraPosition-sceneCenter);
		}else if (event.key.keysym.sym == SDLK_u){
			CanvasTriangle tri(
				CanvasPoint(rand() % window.width, rand() % window.height),
				CanvasPoint(rand() % window.width, rand() % window.height),
				CanvasPoint(rand() % window.width, rand() % window.height));
			Colour colour(rand() % 256, rand() % 256, rand() % 256);
			drawStrokedTriangle(window, tri, colour);
		}else if (event.key.keysym.sym == SDLK_f){// 当按下 f 键时，随机画一个三角形
			CanvasTriangle tri(
				CanvasPoint(rand() % window.width, rand() % window.height),
				CanvasPoint(rand() % window.width, rand() % window.height),
				CanvasPoint(rand() % window.width, rand() % window.height));

			Colour randomColour(
				rand() % 256, // red
				rand() % 256, // green
				rand() % 256  // blue
			);

			std::vector<std::vector<float>> depths(HEIGHT, std::vector<float>(WIDTH, 0.0f));
			drawFilledTriangle(window, tri, randomColour,depths);
		}

		//lab5
		static float translation  = 0.25f;                   // 平移速度
		switch (event.key.keysym.sym){
		case SDLK_1:Mode = RenderMode::Wireframe;break;
        case SDLK_2:Mode = RenderMode::Rasterised;break;
        case SDLK_3:Mode = RenderMode::RayTraced;break;

		case SDLK_a: cameraPosition.x = cameraPosition.x - translation;break;
		case SDLK_d: cameraPosition.x = cameraPosition.x + translation;break;
		case SDLK_w: cameraPosition.y = cameraPosition.y + translation;break;
		case SDLK_s: cameraPosition.y = cameraPosition.y - translation;break;
		case SDLK_q: cameraPosition.z = cameraPosition.z - translation;break;//向前是朝场景
		case SDLK_e: cameraPosition.z = cameraPosition.z + translation;break;

		case SDLK_j: cameraOrientation = cameraOrientation * rotY(+orientation); break; // j:pan 左
		case SDLK_k: cameraOrientation = cameraOrientation * rotY(-orientation); break; // k:pan 右
		case SDLK_i: cameraOrientation = cameraOrientation * rotX(+orientation); break; // i:tilt 上
		case SDLK_o: cameraOrientation = cameraOrientation * rotX(-orientation); break; // o:tilt 下
		}
	}
	else if (event.type == SDL_MOUSEBUTTONDOWN)
	{
		window.savePPM("output.ppm");
		window.saveBMP("output.bmp");
	}
}

glm::mat3 lookAt(const glm::vec3 &camera, const glm::vec3 &target, const glm::vec3&up = {0,1,0}){
	glm::vec3 F = glm::normalize(target - camera);
	glm::vec3 R = glm::normalize(glm::cross(F,up));
	glm::vec3 U = glm::cross(R,F);
	return glm::mat3(R.x,R.y,R.z,  U.x,U.y,U.z,  -F.x,-F.y,-F.z);
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

void drawWireframeScene(DrawingWindow &window,const glm::vec3 &cameraPosition,const glm::mat3 &cameraOrientation,float focalLength,const std::vector<ModelTriangle> &triangles) {
    window.clearPixels();
    Colour white(255,255,255);
    for (ModelTriangle triangle : triangles){
			CanvasPoint projectedPoint0 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[0],cameraOrientation);
			CanvasPoint projectedPoint1 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[1],cameraOrientation);
			CanvasPoint projectedPoint2 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[2],cameraOrientation);
			CanvasTriangle canvasTriangle(projectedPoint0,projectedPoint1,projectedPoint2);
			drawStrokedTriangle(window,canvasTriangle,white);
    }
}

void drawRasterisedFilledScene(DrawingWindow &window,const glm::vec3 &cameraPosition,const glm::mat3 &cameraOrientation,float focalLength,const std::vector<ModelTriangle> &triangles) {
    // 初始化 depth buffer
    static std::vector<std::vector<float>> depths(HEIGHT, std::vector<float>(WIDTH, 0.0f));
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x)
            depths[y][x] = 0.0f;

    window.clearPixels();
	for (ModelTriangle triangle : triangles){
			CanvasPoint projectedPoint0 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[0],cameraOrientation);
			CanvasPoint projectedPoint1 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[1],cameraOrientation);
			CanvasPoint projectedPoint2 = projectVertexOntoCanvasPoint(cameraPosition, focalLength, triangle.vertices[2],cameraOrientation);
			CanvasTriangle canvasTriangle(projectedPoint0,projectedPoint1,projectedPoint2);
			drawFilledTriangle(window,canvasTriangle,triangle.colour,depths);
	}
}

int main(int argc, char *argv[]){
	DrawingWindow window = DrawingWindow(WIDTH, HEIGHT, false);
	SDL_Event event;

	TextureMap texture("/Users/ken/Documents/文稿 - Mac/year3-uni/Graphics/CG2025/Weekly Workbooks/03 Triangles and Textures/texture.ppm");

	CanvasTriangle tri(
		CanvasPoint{160, 10},
		CanvasPoint{300, 230},
		CanvasPoint{10, 150});
	tri.v0().texturePoint = TexturePoint{195, 5};
	tri.v1().texturePoint = TexturePoint{395, 380};
	tri.v2().texturePoint = TexturePoint{65, 330};

	std::map<std::string, Colour> palette = loadMTL("cornell-box.mtl");
	for (const auto &pair : palette){ // pair.first  是键 (std::string) pair.second 是值 (Colour)
		std::cout << "Material: '" << pair.first << "' -> " << pair.second << std::endl;
	}

	std::vector<ModelTriangle> triangles = loadOBJ("cornell-box.obj", palette, 0.35f);
	for (const auto &triangle : triangles){
		std::cout << triangle << std::endl;						   // 打印顶点坐标
		std::cout << "  Colour: " << triangle.colour << std::endl; // 打印颜色
	}

	// lab4-task6

	//depth
	std::vector<std::vector<float>> depths(HEIGHT, std::vector<float>(WIDTH, 0.0f)); //height rows and width columns

	while (true){
		// We MUST poll for events - otherwise the window will freeze !
		if (window.pollForInputEvents(event))
			handleEvent(event, window);

		//旋转scene
		cameraOrientation = lookAt(cameraPosition, sceneCenter);
		// cameraPosition = sceneCenter + rotY(glm::radians(0.5f)) * (cameraPosition - sceneCenter);

		switch (Mode) {
        case RenderMode::Wireframe:
            drawWireframeScene(window, cameraPosition, cameraOrientation, focalLength, triangles);
            break;
        case RenderMode::Rasterised:
            drawRasterisedFilledScene(window, cameraPosition, cameraOrientation, focalLength, triangles);
            break;
        case RenderMode::RayTraced:
            drawRayTracedScene(window, cameraPosition, cameraOrientation, focalLength, triangles);
            break;
    }
		window.renderFrame();
	}
	return 0;
}
