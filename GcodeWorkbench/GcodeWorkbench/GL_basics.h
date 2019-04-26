#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

#include <stdio.h>
#include <tchar.h>
#include <math.h>
#include <assert.h>

#include <gl3w/gl3w.h>

#include <GL/GLU.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

/*
If you don't have opencv at the `moment, this macro will
compile the document to not use the features that require OpenCV
*/
#ifndef __OPENCV_INSTALLED__
#if __has_include(<opencv2/opencv.hpp>)
#define __OPENCV_INSTALLED__
#endif
#endif

using namespace glm;
using namespace std;

////////Usefull macros
#ifndef __MACROS__
#define __MACROS__

template <typename F> struct _scope_exit_t {
	_scope_exit_t(F f) : f(f) {}
	~_scope_exit_t() { f(); }
	F f;
};
template <typename F> _scope_exit_t<F> _make_scope_exit_t(F f) {
	return _scope_exit_t<F>(f);
};
#define _concat_impl(arg1, arg2) arg1 ## arg2
#define _concat(arg1, arg2) _concat_impl(arg1, arg2)
#define defer(code) \
	auto _concat(scope_exit_, __COUNTER__) = _make_scope_exit_t([=](){ code; })

//replaces glfunction() with gl(function()) and prints errors automatically
#ifdef NDEBUG
#define gl(OPENGL_CALL) \
	gl##OPENGL_CALL
#else
#define gl(OPENGL_CALL) \
	gl##OPENGL_CALL; \
	{ \
		int potentialGLError = glGetError(); \
		if (potentialGLError != GL_NO_ERROR) \
		{ \
			fprintf(stderr, "OpenGL Error '%s' (%d) gl" #OPENGL_CALL " " __FILE__ ":%d\n", (const char*)gluErrorString(potentialGLError), potentialGLError, __LINE__); \
			assert(potentialGLError == GL_NO_ERROR && "gl" #OPENGL_CALL); \
		} \
	}
#endif
#endif

namespace GLBasics {

	struct basicIDs {
		GLuint ShaderProgram;
		GLuint VAO, VBO, EBO;
		GLuint textUniformLocation;
		GLuint xformUniformLocation;
		GLuint aspectUniformLocation;
		GLuint imSizeUniformLocation;
		GLuint bufferTextID, fboID;

		void getTextureLocation() { textUniformLocation = gl(GetUniformLocation(ShaderProgram, "Text")); };
		void getXFormLocation() { xformUniformLocation = gl(GetUniformLocation(ShaderProgram, "xform")); };
		void getAspectLocation() { aspectUniformLocation = gl(GetUniformLocation(ShaderProgram, "aspect")); };
		void getImSizeLocation() { imSizeUniformLocation = gl(GetUniformLocation(ShaderProgram, "resolution")); };

		void getBasicUniforms() {
			getTextureLocation();
			getXFormLocation();
			getAspectLocation();
			getImSizeLocation();
		}
	};

	void genFrameBuffer(GLuint * textID, GLuint * fboID, int width, int height);
	void genFloatFrameBuffer(GLuint* textID, GLuint* fboID, int width, int height);
	void genFrameBufferWithDepth(GLuint * textID, GLuint * fboID, GLuint * dboID, int width, int height);
	void simpleQuadVAO(basicIDs &IDS);
	void simpleShaderBinding(basicIDs IDS, int w, int h, bool blend = false, bool clear = false, vec3 clearCol = vec3(1));

	template<typename T>
	void genStreamVBO(GLuint * ID, int vectorLength, int VBOLength);
	void genStaticRectVBO(GLuint * ID);
		
	int loadShader(const char * vertexFilename, const char * fragmentFilename, const char * dataPath = "../data/");
	unsigned char * getPixels(GLuint texture, int w, int h, bool singleChannel);

	const unsigned int shapeIndices[] = {  // note that we start from 0!
		0, 3, 1,  // first Triangle
		1, 3, 2   // second Triangle
	};

	const glm::vec3 rectVertices[] = {
		{ -1.0f,  1.0f,  0.0f, },  // top left
	{ 1.0f,  1.0f,  0.0f, },  // top right
	{ 1.0f, -1.0f,  0.0f },  // bottom right
	{ -1.0f, -1.0f,  0.0f, },  // bottom left
	{ -1.0f,  1.0f,  0.0f, },  // top left
	};

	const GLfloat streamDrawRectVertices[] = {
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
	};

	GLuint loadimage(const char * imagePath, int * W, int * H);
	GLuint makeTexture(unsigned char * data, int W, int H);
	void loadTexture(const char* path, GLuint* id, int* imW, int* imH, bool flip = false);

	//returns rgba from file
	unsigned char * rgbaFromFile(const char * file, int * W, int * H);
	unsigned char * rgbaFromTexture(GLuint ID, int w, int h);

	//returns single channel grayscale from rgba
	unsigned char * grayscaleFromLuminance(unsigned char * img, int W, int H);
	unsigned char * grayscaleFromRed(unsigned char * img, int W, int H);

	GLuint textureFromRGBA(unsigned char * img, int W, int H, bool free_STB_data = true);
	GLuint textureFromGrayscale(unsigned char * img, int W, int H);

	template<typename T>
	void  genStreamVBO(GLuint * ID, int vectorLength, int VBOLength)
	{
		gl(GenBuffers(1, ID));
		gl(BindBuffer(GL_ARRAY_BUFFER, *ID));
		gl(BufferData(GL_ARRAY_BUFFER, VBOLength * vectorLength * sizeof(T), NULL, GL_STREAM_DRAW));
	}
}
