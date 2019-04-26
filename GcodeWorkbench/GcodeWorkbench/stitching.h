#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
//#include <tchar.h>
#include <math.h>
#include <assert.h>

#include <gl3w/gl3w.h>

#include <GL/GLU.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include "GL_basics.h"

//needed for xml reading
#ifdef __OPENCV_INSTALLED__
#include "opencv2/core.hpp"
#endif

using namespace std;
using namespace glm;
using namespace GLBasics;

class stitchImg {
public:
	GLuint Id;
	vec2 position, scale;
	string file;
	stitchImg() {}
	stitchImg(GLuint id, vec2 pos, vec2 Scale) {
		Id = id;
		position = pos;
		scale = Scale;
	}
	stitchImg(string path, vec2 pos, vec2 Scale) {
		file = path;
		Id = 0;
		position = pos;
		scale = Scale;
	}

#ifdef __OPENCV_INSTALLED__
	void write(cv::FileStorage& fs) const
	{
		fs << "{" <<
			"path" << file <<
			"x" << (int)position.x <<
			"y" << (int)position.y <<
			"}";
	}

	void read(const cv::FileNode& node) {
		position.x = (float)node["x"];
		position.y = (float)node["y"];
		file = (string)node["path"];
	}
#endif
};

class stitcherSettings {
public:
	float spreadX = 6.6f, spreadY = 7.5f, ladderX = 0, ladderY = 6.8f, imAngle = -0.038f, binaryThresh = 0.0f, orbit = 0.0f;
	float featherX = 0.39f, featherY = 0.34f, cropX = 0.35f, cropY = 0.3f, skew = 0.0f;
	stitcherSettings() {};
#ifdef __OPENCV_INSTALLED__
	void write(cv::FileStorage& fs) const
	{
		//fs << "{" << "spreadX" << spreadX << "spreadY" << spreadY << "ladderX" << ladderX << "ladderY" << ladderY << "imAngle" << imAngle << "offsetX" << offsetX << "offsetY" << offsetY << "featherX" << featherX << "featherY" << featherY << "cropX" << cropX << "cropY" << cropY << "binaryThresh" << binaryThresh << "}";
		fs << "{" <<
			"spreadX" << spreadX <<
			"spreadY" << spreadY <<
			"ladderX" << ladderX <<
			"ladderY" << ladderY <<
			"imAngle" << imAngle <<
			"featherX" << featherX <<
			"featherY" << featherY <<
			"cropX" << cropX <<
			"cropY" << cropY <<
			"binaryThresh" << binaryThresh <<
			"orbit" << orbit<<
			"skew" << skew <<
			"}";
	}

	void read(const cv::FileNode& node) {
		spreadX = (float)node["spreadX"];
		spreadY = (float)node["spreadY"];
		ladderX = (float)node["ladderX"];
		ladderY = (float)node["ladderY"];
		imAngle = (float)node["imAngle"];
		featherX = (float)node["featherX"];
		featherY = (float)node["featherY"];
		cropX = (float)node["cropX"];
		cropY = (float)node["cropY"];
		binaryThresh = (float)node["binaryThresh"];
		orbit = (float)node["orbit"];
		skew = (float)node["skew"];
	}
#endif
};

#ifdef __OPENCV_INSTALLED__
static void read(const cv::FileNode& node, stitchImg& x, const stitchImg& default_value = stitchImg())
{
	if (node.empty())
		x = default_value;
	else
		x.read(node);
}
#endif


bool stitchBufferInit = false;
GLuint textureShaderProgram;
GLuint sVAO, sVBO, sEBO;
GLuint stextUniformLocation;
GLuint sxformUniformLocation;
GLuint saspectUniformLocation;
GLuint featherUniformLocation;
GLuint cropUniformLocation;
GLuint threshUniformLocation;
GLuint skewUniformLocation;
GLuint sbufferTextID, sfboID;

GLuint simpleStitch(vector<stitchImg> imgs, float aspect , stitcherSettings settings, vec2 ** placement = NULL, vec2** offsetMesh = NULL, int drawLimit = -1, float zoom = 1.0f) {
	int canvasW = 2300, canvasH = 2300;
	if (!stitchBufferInit)
	{

		//compile the shaders
		textureShaderProgram = loadShader("rectVert", "textFrag");

		//create VAO for shape shaders
		gl(GenVertexArrays(1, &sVAO));
		gl(GenBuffers(1, &sVBO));
		gl(GenBuffers(1, &sEBO));

		gl(BindVertexArray(sVAO));

		gl(BindBuffer(GL_ARRAY_BUFFER, sVBO));
		gl(BufferData(GL_ARRAY_BUFFER, sizeof(rectVertices), rectVertices, GL_STATIC_DRAW));

		gl(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, sEBO));
		gl(BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(shapeIndices), shapeIndices, GL_STATIC_DRAW));

		gl(VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)(sizeof(float) * 3)));
		gl(EnableVertexAttribArray(0));

		gl(BindBuffer(GL_ARRAY_BUFFER, 0));

		//get uniforms
		stextUniformLocation = gl(GetUniformLocation(textureShaderProgram, "Text"));
		sxformUniformLocation = gl(GetUniformLocation(textureShaderProgram, "xform"));
		saspectUniformLocation = gl(GetUniformLocation(textureShaderProgram, "aspect"));
		featherUniformLocation = gl(GetUniformLocation(textureShaderProgram, "featherXY"));
		cropUniformLocation = gl(GetUniformLocation(textureShaderProgram, "cropXY"));
		threshUniformLocation = gl(GetUniformLocation(textureShaderProgram, "threshold"));
		skewUniformLocation = gl(GetUniformLocation(textureShaderProgram, "skew"));

		genFrameBuffer(&sbufferTextID, &sfboID, canvasW, canvasH);
	}

	gl(BindVertexArray(sVAO));
	gl(BindBuffer(GL_ARRAY_BUFFER, 0));
	gl(Enable(GL_BLEND));
	gl(BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE));

	gl(BindFramebuffer(GL_FRAMEBUFFER, sfboID));
	gl(Disable(GL_CULL_FACE));
	gl(Viewport(0, 0, canvasW, canvasH));
	gl(ClearColor(0.64f, 0.64f, 0.64f, 1.0f));
	gl(Clear(GL_COLOR_BUFFER_BIT));

	gl(UseProgram(textureShaderProgram));

	float minX = 99999999, maxX = -99999999, minY = 999999999, maxY = -99999999;
	float maxXG = 0, maxYG = 0;
	for (int i = 1; i < imgs.size(); i++) {
		stitchImg im = imgs[i];
		float xpos = -im.position.y * settings.spreadX + (im.position.x / 20);
		float ypos = im.position.x * settings.spreadY + (im.position.y / 20);
		minX = glm::min(minX, xpos);
		minY = glm::min(minY, ypos);
		maxX = glm::max(maxX, xpos);
		maxY = glm::max(maxY, ypos);
		maxXG = glm::max(maxXG, im.position.x);
		maxYG = glm::max(maxYG, im.position.y);
	}

	for (int i = 0; i < imgs.size(); i++)
	{
		if (drawLimit != -1)
			if (i >= drawLimit)
				break;

		stitchImg im = imgs[i];

		float angle = settings.imAngle;
		float xpos = -im.position.y * settings.spreadX + ((im.position.x - maxXG / 2) / 20) * settings.ladderX + (maxX - minX) / 2;
		float ypos = im.position.x * settings.spreadY + ((im.position.y - maxYG / 2) / 20) * settings.ladderY - (maxY - minY) / 2;

		int row = (int)im.position.x / 20;
		int col = (int)im.position.y / 20;

		if (placement != NULL)
			placement[col][row] = vec2(xpos, ypos);
		if (offsetMesh != NULL) {
			xpos += offsetMesh[col][row].x;
			ypos += offsetMesh[col][row].y;
		}


		mat4 m = mat4(1);
		m = rotate(m, settings.orbit, vec3(0.0f, 0.0f, 1.0f));
		m = translate(m, vec3(vec2(xpos, ypos) * 2.0f / vec2(canvasW, canvasH), 0));
		m = scale(m, vec3(im.scale / vec2(canvasW, canvasH), 1));
		m = rotate(m, settings.imAngle, vec3(0.0f, 0.0f, 1.0f));

		gl(Uniform1f(saspectUniformLocation, aspect));
		gl(Uniform2f(featherUniformLocation, settings.featherX, settings.featherY));
		gl(Uniform2f(cropUniformLocation, settings.cropX, settings.cropY));
		gl(Uniform1f(threshUniformLocation, settings.binaryThresh));
		gl(Uniform1f(skewUniformLocation, settings.skew));
		gl(BindTexture(GL_TEXTURE_2D, im.Id));
		gl(Uniform1i(stextUniformLocation, 0));
		gl(UniformMatrix4fv(sxformUniformLocation, 1, GL_FALSE, value_ptr(m)));

		gl(DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0));
	}
	stitchBufferInit = true;
	return sbufferTextID;
}

