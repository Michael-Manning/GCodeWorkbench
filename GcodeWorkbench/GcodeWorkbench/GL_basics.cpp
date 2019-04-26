#include"pch.h"
#define _CRT_SECURE_NO_WARNINGS
#include "GL_basics.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif 
#include <stb/stb_image.h> 

namespace GLBasics {

	void genFrameBuffer(GLuint * textID, GLuint * fboID, int width, int height) {
		glGenTextures(1, textID);
		glGenFramebuffers(1, fboID);
		{
			gl(BindTexture(GL_TEXTURE_2D, *textID));
			defer(glBindTexture(GL_TEXTURE_2D, 0));
			gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
			gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
			gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
			gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
			gl(TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0));

			gl(BindFramebuffer(GL_FRAMEBUFFER, *fboID));
			defer(glBindFramebuffer(GL_FRAMEBUFFER, 0));

			// attach the texture to FBO color attachment point
			gl(FramebufferTexture2D(GL_FRAMEBUFFER,        // 1. fbo target: GL_FRAMEBUFFER 
				GL_COLOR_ATTACHMENT0,  // 2. attachment point
				GL_TEXTURE_2D,         // 3. tex target: GL_TEXTURE_2D
				*textID,             // 4. tex ID
				0));                    // 5. mipmap level: 0(base)

									   // check FBO status
			GLenum status = gl(CheckFramebufferStatus(GL_FRAMEBUFFER));
			assert(status == GL_FRAMEBUFFER_COMPLETE);

			//gl(BindTexture(GL_TEXTURE_2D, *textID));
			//gl(TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0));
			//gl(BindTexture(GL_TEXTURE_2D, 0));

			gl(BindFramebuffer(GL_FRAMEBUFFER, *fboID));

			gl(ClearColor(0.0f, 0.0f, 0.0f, 1.0f));
			gl(Clear(GL_COLOR_BUFFER_BIT));
		}
	}
	void genFloatFrameBuffer(GLuint* textID, GLuint* fboID, int width, int height) {
		glGenTextures(1, textID);
		glGenFramebuffers(1, fboID);
		{
			gl(BindTexture(GL_TEXTURE_2D, *textID));
			defer(glBindTexture(GL_TEXTURE_2D, 0));
			gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
			gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
			gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
			gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
			gl(TexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, 0));

			gl(BindFramebuffer(GL_FRAMEBUFFER, *fboID));
			defer(glBindFramebuffer(GL_FRAMEBUFFER, 0));

			// attach the texture to FBO color attachment point
			gl(FramebufferTexture2D(GL_FRAMEBUFFER,        // 1. fbo target: GL_FRAMEBUFFER 
				GL_COLOR_ATTACHMENT0,  // 2. attachment point
				GL_TEXTURE_2D,         // 3. tex target: GL_TEXTURE_2D
				*textID,             // 4. tex ID
				0));                    // 5. mipmap level: 0(base)

									   // check FBO status
			GLenum status = gl(CheckFramebufferStatus(GL_FRAMEBUFFER));
			assert(status == GL_FRAMEBUFFER_COMPLETE);

			//gl(BindTexture(GL_TEXTURE_2D, *textID));
			//gl(TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0));
			//gl(BindTexture(GL_TEXTURE_2D, 0));

			gl(BindFramebuffer(GL_FRAMEBUFFER, *fboID));

			gl(ClearColor(0.0f, 0.0f, 0.0f, 1.0f));
			gl(Clear(GL_COLOR_BUFFER_BIT));
		}
	}


	void genFrameBufferWithDepth(GLuint* textID, GLuint* fboID, GLuint* dboID, int width, int height) {
			
		//color texture attachment
		gl(GenTextures(1, textID));
		gl(BindTexture(GL_TEXTURE_2D, *textID));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
		gl(TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0));

		//depth texture attachment
		gl(GenTextures(1, dboID));
		gl(BindTexture(GL_TEXTURE_2D, *dboID));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
		gl(TexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL));


		gl(GenFramebuffers(1, fboID));
		gl(BindFramebuffer(GL_FRAMEBUFFER, *fboID));

		gl(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *textID, 0));
		gl(FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, *dboID, 0));


		GLenum status = gl(CheckFramebufferStatus(GL_FRAMEBUFFER));
		assert(status == GL_FRAMEBUFFER_COMPLETE);


		gl(ClearColor(0.0f, 0.0f, 0.0f, 1.0f));
		gl(Clear(GL_COLOR_BUFFER_BIT));
		gl(BindFramebuffer(GL_FRAMEBUFFER, 0));
		gl(BindTexture(GL_TEXTURE_2D, 0));
		
	}

	void simpleQuadVAO(basicIDs &IDS)
	{
		//create VAO for shape shaders
		gl(GenVertexArrays(1, &IDS.VAO));
		gl(GenBuffers(1, &IDS.VBO));
		gl(GenBuffers(1, &IDS.EBO));

		gl(BindVertexArray(IDS.VAO));

		gl(BindBuffer(GL_ARRAY_BUFFER, IDS.VBO));
		gl(BufferData(GL_ARRAY_BUFFER, sizeof(rectVertices), rectVertices, GL_STATIC_DRAW));

		gl(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, IDS.EBO));
		gl(BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(shapeIndices), shapeIndices, GL_STATIC_DRAW));

		gl(VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)(sizeof(float) * 3)));
		gl(EnableVertexAttribArray(0));
		gl(BindBuffer(GL_ARRAY_BUFFER, 0));
	}

	void simpleShaderBinding(basicIDs IDs, int w, int h, bool blend, bool clear, vec3 clearCol)
	{
		gl(BindVertexArray(IDs.VAO));
		gl(BindBuffer(GL_ARRAY_BUFFER, 0));

		gl(BindFramebuffer(GL_FRAMEBUFFER, IDs.fboID));
		gl(Disable(GL_CULL_FACE));
		gl(Viewport(0, 0, w, h));

		if (blend) {
			gl(Enable(GL_BLEND));
			gl(BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
		}
		if (clear) {
			gl(ClearColor(clearCol.r, clearCol.g, clearCol.b, 1.0f));
			gl(Clear(GL_COLOR_BUFFER_BIT));
		}
		gl(UseProgram(IDs.ShaderProgram));
	}


	int loadShader(const char * vertexFilename, const char * fragmentFilename, const char * dataPath) {
		const char * vertexShaderSource;
		const char * fragmentShaderSource;

		string str, str2; //can't be local

		{
			ostringstream sstream;

			ifstream fs((string)dataPath + "shaders\\" + (string)vertexFilename + ".glsl");
			sstream << fs.rdbuf();
			str = sstream.str();
			vertexShaderSource = str.c_str();

			if (str == "") {
				std::cout << "ERROR: Failed to load vertex shader source \"" << vertexFilename << "\"" << "\n" << std::endl;
			}

			ostringstream sstream2;
			ifstream fs2((string)dataPath + "shaders\\" + (string)fragmentFilename + ".glsl");
			sstream2 << fs2.rdbuf();
			str2 = sstream2.str();
			fragmentShaderSource = str2.c_str();

			if (str2 == "") {
				std::cout << "ERROR: Failed to load fragment shader source \"" << fragmentFilename << "\"" << "\n" << std::endl;
			}
		}
		int shaderProgram;

		int vertexShader = gl(CreateShader(GL_VERTEX_SHADER));
		gl(ShaderSource(vertexShader, 1, &vertexShaderSource, NULL));
		gl(CompileShader(vertexShader));

		int success;
		char infoLog[512];
		gl(GetShaderiv(vertexShader, GL_COMPILE_STATUS, &success));
		if (!success)
		{
			gl(GetShaderInfoLog(vertexShader, 512, NULL, infoLog));
			std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
		}

		int fragmentShader = gl(CreateShader(GL_FRAGMENT_SHADER));
		gl(ShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL));
		gl(CompileShader(fragmentShader));

		gl(GetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success));
		if (!success)
		{
			gl(GetShaderInfoLog(fragmentShader, 512, NULL, infoLog));
			std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
		}

		shaderProgram = gl(CreateProgram());
		gl(AttachShader(shaderProgram, vertexShader));
		gl(AttachShader(shaderProgram, fragmentShader));
		gl(LinkProgram(shaderProgram));

		gl(GetProgramiv(shaderProgram, GL_LINK_STATUS, &success));
		if (!success) {
			gl(GetProgramInfoLog(shaderProgram, 512, NULL, infoLog));
			std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
		}

		gl(DeleteShader(vertexShader));
		gl(DeleteShader(fragmentShader));
		return shaderProgram;
	}

	unsigned char * getPixels(GLuint texture, int w, int h, bool singleChannel)
	{
		gl(BindTexture(GL_TEXTURE_2D, texture));
		if (singleChannel) {
			unsigned char * data = new unsigned char[w * h];
			gl(GetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, data));
			return data;
		}
		unsigned char * data = new unsigned char[w * h * 3];
		gl(GetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, data));
		return data;
	}

	GLuint loadimage(const char * imagePath, int * W, int * H) {
		int comp, imW, imH;
		GLuint ID;

		stbi_set_flip_vertically_on_load(false);
		unsigned char* image = stbi_load(imagePath, &imW, &imH, &comp, STBI_rgb_alpha);
		*W = imW;
		*H = imH;
		if (image == NULL) {
			fprintf(stderr, "failed to load image\n");
			return 0;
		}
		gl(GenTextures(1, &ID));
		gl(BindTexture(GL_TEXTURE_2D, ID));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		gl(TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imW, imH, 0, GL_RGBA, GL_UNSIGNED_BYTE, image));
		gl(BindTexture(GL_TEXTURE_2D, 0));

		stbi_image_free(image);
		return ID;
	}

	GLuint makeTexture(unsigned char * data, int W, int H)
	{
		GLuint ID;
		gl(GenTextures(1, &ID));
		gl(BindTexture(GL_TEXTURE_2D, ID));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		gl(TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, data));
		gl(BindTexture(GL_TEXTURE_2D, 0));

		return ID;
	}

	void loadTexture(const char* path, GLuint* id, int* imW, int* imH, bool flip) {
		stbi_set_flip_vertically_on_load(flip);
		int comp;
		unsigned char* image = stbi_load(path, imW, imH, &comp, STBI_rgb_alpha);
		if (image == NULL) {
			fprintf(stderr, "failed to load image\n");
			return;
		}
		gl(GenTextures(1, id));
		gl(BindTexture(GL_TEXTURE_2D, *id));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		gl(TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, *imW, *imH, 0, GL_RGBA, GL_UNSIGNED_BYTE, image));
		gl(BindTexture(GL_TEXTURE_2D, 0));

		stbi_image_free(image);
	}

	unsigned char * rgbaFromFile(const char * file, int * W, int * H)
	{
		int comp;
		unsigned char* image = stbi_load(file, W, H, &comp, STBI_rgb_alpha);
		if (image == NULL) {
			fprintf(stderr, "failed to load image\n");
			return nullptr;
		}
		return image;
	}

	unsigned char * rgbaFromTexture(GLuint ID, int w, int h)
	{
		unsigned char * data = new unsigned char[w *  h * 4];

		gl(PixelStorei(GL_PACK_ALIGNMENT,  4));
		gl(PixelStorei(GL_PACK_ROW_LENGTH, w));
		gl(BindTexture(GL_TEXTURE_2D, ID));
		gl(GetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data));

		return data;
	}

	unsigned char * grayscaleFromLuminance(unsigned char * img, int w, int h)
	{
		unsigned char * gray = new unsigned char[w * h];
		for (int i = 0; i < h; i++)
		{
			for (int j = 0; j < w; j++)
			{
				unsigned char r = img[(i * w + j) * 4];
				unsigned char g = img[(i * w + j) * 4 + 1];
				unsigned char b = img[(i * w + j) * 4 + 2];
				gray[i * w + j] = (0.2126*r + 0.7152*g + 0.0722*b);
			}
		}
		return gray;
	}

	unsigned char * grayscaleFromRed(unsigned char * img, int w, int h)
	{
		unsigned char * gray = new unsigned char[w * h];
		for (int i = 0; i < h; i++)
		{
			for (int j = 0; j < w; j++)
			{
				gray[i * w + j] = img[(i * w + j) * 4];
			}
		}
		return gray;
	}


	GLuint textureFromRGBA(unsigned char * img, int W, int H, bool free_STB_data)
	{
		GLuint ID;
		gl(GenTextures(1, &ID));
		gl(BindTexture(GL_TEXTURE_2D, ID));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		gl(TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, img));
		gl(BindTexture(GL_TEXTURE_2D, 0));

		if (free_STB_data)
			stbi_image_free(img);
		return ID;
	}

	GLuint textureFromGrayscale(unsigned char * img, int W, int H)
	{
		GLuint ID;
		gl(GenTextures(1, &ID));
		gl(BindTexture(GL_TEXTURE_2D, ID));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		gl(TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, W, H, 0, GL_RED, GL_UNSIGNED_BYTE, img));
		gl(BindTexture(GL_TEXTURE_2D, 0));
		return ID;
	}

	//template<typename T>
	//void  genStreamVBO(GLuint * ID, int vectorLength, int VBOLength)
	//{
	//	glGenBuffers(1, ID);
	//	glBindBuffer(GL_ARRAY_BUFFER, *ID);
	//	glBufferData(GL_ARRAY_BUFFER, VBOLength * vectorLength * sizeof(T), NULL, GL_STREAM_DRAW);
	//}
	void genStaticRectVBO(GLuint * ID)
	{
		gl(GenBuffers(1, ID));
		gl(BindBuffer(GL_ARRAY_BUFFER, *ID));
		gl(BufferData(GL_ARRAY_BUFFER, sizeof(streamDrawRectVertices), streamDrawRectVertices, GL_STATIC_DRAW));
	}
}