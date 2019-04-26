#include"pch.h"
#define _CRT_SECURE_NO_WARNINGS
#include "image_processing.h"
#include <thread>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/quaternion.hpp>

using namespace GLBasics;

//#define uint8_t uint8_t
vector<curve> calculateWaveArt(int w, int h, int lineDiv, uint8_t* waveData, float base, float freq, float amp, float cut, double timeLimit, int* progress)
{
	vector<curve> curves;
	double timer = glfwGetTime();
	for (int i = *progress; i < h; i += lineDiv, *progress += lineDiv)
	{
		vec2 lastp(0, i);
		bool skip = false;
		float graphX = 0;
		for (int j = 1; j < w; j += 1)
		{
			uint8_t pix = waveData[(i / lineDiv) * w + j];
			float darkness = (255.0f - pix) / 255.0f;
			if (darkness < 0.05f)
				darkness = 0.0f;

			graphX += darkness * freq;
			vec2 nextp(j, i + cos((j + graphX) * base) * darkness * amp);
			curve line = cLine(lastp, nextp);
			lastp = nextp;
			curves.push_back(line);
		}
		curve line = cLine(vec2(lastp), vec2(w, lastp.y));
		curves.push_back(line);
		if (glfwGetTime() - timer > timeLimit)
			break;
	}
	return curves;
}

int calculateWaveArt(vec4 * buffer, int bufferLength, int w, int h, int lineDiv, uint8_t* waveData, float base, float freq, float amp, float cut)
{
	double timer = glfwGetTime();
	int arIndex = 0;
	for (int i = 0; i < h; i += lineDiv)
	{
		vec2 lastp(0, i);
		bool skip = false;
		float graphX = 0;
		for (int j = 1; j < w; j += 1)
		{
			uint8_t pix = waveData[(i / lineDiv) * w + j];
			float darkness = (255.0f - pix) / 255.0f;
			if (darkness < 0.05f)
				darkness = 0.0f;

			graphX += darkness * freq;
			vec2 nextp(j, i + cos((j + graphX) * base) * darkness * amp);
			buffer[arIndex] = packedLine(lastp, nextp);
			arIndex++;
			lastp = nextp;
		}
		buffer[arIndex] = packedLine(lastp, vec2(w, lastp.y));
		arIndex++;
		if (arIndex >= bufferLength)
			break;
	}
	return arIndex;
}

thread * wvthreads;
bool wvJobCancel = false;
int* wvrunningJobs;
int wvThreadCount;

void calculateWaveLine(int w, int y, uint8_t * waveData, float base, float freq, float amp, float cut, SafeQueue<vec4> * que) {
	vec2 lastp(0, y);
	bool skip = false;
	float graphX = 0;

	for (int j = 1; j < w && !wvJobCancel; j += 1)
	{

		uint8_t pix = waveData[j];
		float darkness = (255.0f - (int)pix) / 255.0f;
		if (darkness < cut)
			darkness = 0.0f;

		graphX += darkness * freq;
		vec2 nextp(j, y + cos((j + graphX) * base) * darkness * amp);
		que->enqueue(vec4(lastp, nextp));
		lastp = nextp;
		skip = false;
	}
	que->enqueue(vec4(lastp, w, y));
}

void waveArtThreadFunc(int w, int start, int end, int lineDiv, uint8_t * waveData, float base, float freq, float amp, float cut, SafeQueue<vec4> * que) {
	for (int i = start; i < end && !wvJobCancel; i++)
		calculateWaveLine(w, i * lineDiv, waveData + i * w, base, freq, amp, cut, que);
	*wvrunningJobs = (*wvrunningJobs) - 1;
}

void calculateWaveArtParalell(int w, int h, int lineDiv, uint8_t* waveData, float base, float freq, float amp, float cut, SafeQueue<vec4> * que, int* jobs)
{
	wvrunningJobs = jobs;
	//start threads
	wvThreadCount = std::thread::hardware_concurrency();
	*wvrunningJobs = wvThreadCount;
	wvthreads = new thread[wvThreadCount];
	int jobSize = h / lineDiv / wvThreadCount;
	for (int i = 0; i < wvThreadCount - 1; i++)
		wvthreads[i] = thread(waveArtThreadFunc, w, i * jobSize, (i + 1) * jobSize, lineDiv, waveData, base, freq, amp, cut, que);
	wvthreads[wvThreadCount - 1] = thread(waveArtThreadFunc, w, (wvThreadCount - 1) * jobSize, h / lineDiv, lineDiv, waveData, base, freq, amp, cut, que);
}

void waveArtJoinThreads() {
	while (*wvrunningJobs);
	for (int i = 0; i < wvThreadCount; i++)
		wvthreads[i].join();
	//	delete threads;
}
void waveArtCancelThreads() {
	if (!*wvrunningJobs)
		return;
	wvJobCancel = true;
	waveArtJoinThreads();
}


bool bufferAInit = false;
GLuint lineDivUniformLocationA;
basicIDs wvtIDs;

GLuint getWaveArtAveragedLines(uint8_t * data, int w, int h, float lineDiv, float smoothing) {

	GLuint img = GLBasics::makeTexture(data, w, h);

	if (!bufferAInit)
	{
		wvtIDs.ShaderProgram = loadShader("rectVert", "waveBuffer");
		simpleQuadVAO(wvtIDs);
		//get uniforms
		wvtIDs.getBasicUniforms();
		lineDivUniformLocationA = gl(GetUniformLocation(wvtIDs.ShaderProgram, "div"));

		genFrameBuffer(&wvtIDs.bufferTextID, &wvtIDs.fboID, w, h);
	}

	simpleShaderBinding(wvtIDs, w, h);
	gl(UseProgram(wvtIDs.ShaderProgram));


	mat4 m = mat4(1);

	gl(Uniform1f(wvtIDs.aspectUniformLocation, 1.0f));
	gl(Uniform2f(wvtIDs.imSizeUniformLocation, (float)w, (float)h));
	gl(Uniform1f(lineDivUniformLocationA, lineDiv));
	gl(BindTexture(GL_TEXTURE_2D, img));
	gl(Uniform1i(wvtIDs.textUniformLocation, 0));
	gl(UniformMatrix4fv(wvtIDs.xformUniformLocation, 1, GL_FALSE, value_ptr(m)));

	gl(DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0));

	bufferAInit = true;
	gl(BindFramebuffer(GL_FRAMEBUFFER, 0));

	gl(DeleteTextures(1, &img));
	return 	wvtIDs.bufferTextID;
}

uint8_t* getSpiralData(uint8_t * imageData, int w, int h, int rotations, float precision)
{
	int precalcIterations = 0;

	while (true) {

		precalcIterations++;
	}

	//uint8_t * data = new uint8_t[];
	//float spirtalAngle = 0;
	//float spirateRadius = 0;
	//while (true) {
	//	int x = (int)(cos())
	//	spiralAngle += precision;
	//}



	return nullptr;
}

bool bufferBInit = false;
GLuint wavedataText;
GLuint derivativesText;
GLuint wavedataUniformLocation;
GLuint baseUniformLocation;
GLuint darkFreqUniformLocation;
GLuint ampUniformLocation;
GLuint derivativesUniformLocation;
GLuint lineDivUniformLocationB;
basicIDs waveIDs;

GLuint renderWavesAsEquation(uint8_t * waveData, int w, int h, int lineDiv, float base, float freq, float amp, float cut, bool updateBuffers)
{
	if (!bufferBInit || updateBuffers)
	{
		if (!bufferBInit) {
			//compile the shaders
			waveIDs.ShaderProgram = loadShader("rectVert", "waveFrag");

			//create VAO for shape shaders
			simpleQuadVAO(waveIDs);

			//get uniforms
			waveIDs.getXFormLocation();
			waveIDs.getAspectLocation();
			waveIDs.getImSizeLocation();
			wavedataUniformLocation = gl(GetUniformLocation(waveIDs.ShaderProgram, "waveData"));
			derivativesUniformLocation = gl(GetUniformLocation(waveIDs.ShaderProgram, "derivatives"));
			baseUniformLocation = gl(GetUniformLocation(waveIDs.ShaderProgram, "base"));
			darkFreqUniformLocation = gl(GetUniformLocation(waveIDs.ShaderProgram, "darkFreq"));
			ampUniformLocation = gl(GetUniformLocation(waveIDs.ShaderProgram, "ampSetting"));
			lineDivUniformLocationB = gl(GetUniformLocation(waveIDs.ShaderProgram, "div"));
			genFrameBuffer(&waveIDs.bufferTextID, &waveIDs.fboID, w, h);
		}


		uint8_t* data = new uint8_t[w * h];
		float* derivatives = new float[w * h];

		for (int i = 0; i < h; i++) {
			float graphX = 0;
			int temp = i / lineDiv;
			for (int j = 0; j < w; j += 1) {
				uint8_t pix = waveData[(i / lineDiv) * w + j];// [i / (int)lineDiv][j];
				data[(i * w + j)] = pix;

				float darkness = (255.0f - (float)pix) / 255.0f;
				if (i == 300 && j == 400) {
					int t = 0;
				}
				if (darkness < 0.05f)
					darkness = 0.0f;
				graphX += darkness * 1.5;
				derivatives[(i * w + j)] = graphX;
			}
		}

		gl(GenTextures(1, &wavedataText));
		gl(BindTexture(GL_TEXTURE_2D, wavedataText));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		gl(TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, data));

		gl(GenTextures(1, &derivativesText));
		gl(BindTexture(GL_TEXTURE_2D, derivativesText));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		gl(TexImage2D(GL_TEXTURE_2D, 0, GL_R32F, w, h, 0, GL_RED, GL_FLOAT, derivatives));

		delete[] data;
		delete[] derivatives;
	}

	simpleShaderBinding(waveIDs, w, h, true, true, vec3(1));

	gl(UseProgram(waveIDs.ShaderProgram));

	mat4 m = mat4(1);

	gl(Uniform1f(waveIDs.aspectUniformLocation, 1.0f));
	gl(Uniform2f(waveIDs.imSizeUniformLocation, w, h));
	gl(Uniform1f(lineDivUniformLocationB, lineDiv));
	gl(Uniform1f(baseUniformLocation, base * w));
	gl(Uniform1f(darkFreqUniformLocation, freq / w));
	gl(Uniform1f(ampUniformLocation, amp / h));

	gl(Uniform1i(wavedataUniformLocation, 0));
	gl(Uniform1i(derivativesUniformLocation, 1));
	gl(ActiveTexture(GL_TEXTURE0 + 0));
	gl(BindTexture(GL_TEXTURE_2D, wavedataText));
	gl(ActiveTexture(GL_TEXTURE0 + 1));
	gl(BindTexture(GL_TEXTURE_2D, derivativesText));

	gl(UniformMatrix4fv(waveIDs.xformUniformLocation, 1, GL_FALSE, value_ptr(m)));

	gl(DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0));

	bufferBInit = true;
	gl(BindFramebuffer(GL_FRAMEBUFFER, 0));
	gl(ActiveTexture(GL_TEXTURE0));

	return waveIDs.bufferTextID;

}

uint8_t saturated_add(uint8_t val1, char val2)
{
	short val1_int = val1;
	short val2_int = val2;
	short tmp = val1_int + val2_int;

	if (tmp > 255)
		return (uint8_t)255;
	else if (tmp < 0)
		return 0;
	else
		return tmp;
}

void simpleDither(uint8_t* img, int w, int h)
{
	int err;
	uint8_t a, b, c, d;

	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < w; j++)
		{
			if (img[i * w + j] > 127)
			{
				err = img[i * w + j] - 255;
				img[i * w + j] = 255;
			}
			else
			{
				err = img[i * w + j] - 0;
				img[i * w + j] = 0;
			}

			a = (err * 7) / 16;
			b = (err * 1) / 16;
			c = (err * 5) / 16;
			d = (err * 3) / 16;

			if ((i != (h - 1)) && (j != 0) && (j != (w - 1)))
			{
				img[(i + 0) * w + j + 1] = saturated_add(img[(i + 0) * w + j + 1], a);
				img[(i + 1) * w + j + 1] = saturated_add(img[(i + 1) * w + j + 1], b);
				img[(i + 1) * w + j + 0] = saturated_add(img[(i + 1) * w + j + 0], c);
				img[(i + 1) * w + j + -1] = saturated_add(img[(i + 1) * w + j + -1], d);
			}
		}
	}
}









bool loadOBJ(
	const char* path,
	std::vector<glm::vec3> & out_vertices,
	std::vector<glm::vec2> & out_uvs,
	std::vector<glm::vec3> & out_normals
) {
	printf("Loading OBJ file %s...\n", path);

	std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;
	std::vector<glm::vec3> temp_vertices;
	std::vector<glm::vec2> temp_uvs;
	std::vector<glm::vec3> temp_normals;


	FILE* file = fopen(path, "r");
	if (file == NULL) {
		printf("Impossible to open the file ! Are you in the right path ? See Tutorial 1 for details\n");
		getchar();
		return false;
	}

	while (1) {

		char lineHeader[128];
		// read the first word of the line
		int res = fscanf(file, "%s", lineHeader);
		if (res == EOF)
			break; // EOF = End Of File. Quit the loop.

		// else : parse lineHeader

		if (strcmp(lineHeader, "v") == 0) {
			glm::vec3 vertex;
			fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z);
			temp_vertices.push_back(vertex);
		}
		else if (strcmp(lineHeader, "vt") == 0) {
			glm::vec2 uv;
			fscanf(file, "%f %f\n", &uv.x, &uv.y);
			uv.y = -uv.y; // Invert V coordinate since we will only use DDS texture, which are inverted. Remove if you want to use TGA or BMP loaders.
			temp_uvs.push_back(uv);
		}
		else if (strcmp(lineHeader, "vn") == 0) {
			glm::vec3 normal;
			fscanf(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z);
			temp_normals.push_back(normal);
		}
		else if (strcmp(lineHeader, "f") == 0) {
			std::string vertex1, vertex2, vertex3;
			unsigned int vertexIndex[3], uvIndex[3], normalIndex[3];
			int matches = fscanf(file, "%d/%d/%d %d/%d/%d %d/%d/%d\n", &vertexIndex[0], &uvIndex[0], &normalIndex[0], &vertexIndex[1], &uvIndex[1], &normalIndex[1], &vertexIndex[2], &uvIndex[2], &normalIndex[2]);
			if (matches != 9) {
				printf("File can't be read by our simple parser :-( Try exporting with other options\n");
				fclose(file);
				return false;
			}
			vertexIndices.push_back(vertexIndex[0]);
			vertexIndices.push_back(vertexIndex[1]);
			vertexIndices.push_back(vertexIndex[2]);
			uvIndices.push_back(uvIndex[0]);
			uvIndices.push_back(uvIndex[1]);
			uvIndices.push_back(uvIndex[2]);
			normalIndices.push_back(normalIndex[0]);
			normalIndices.push_back(normalIndex[1]);
			normalIndices.push_back(normalIndex[2]);
		}
		else {
			// Probably a comment, eat up the rest of the line
			char stupidBuffer[1000];
			fgets(stupidBuffer, 1000, file);
		}

	}

	// For each vertex of each triangle
	for (unsigned int i = 0; i < vertexIndices.size(); i++) {

		// Get the indices of its attributes
		unsigned int vertexIndex = vertexIndices[i];
		unsigned int uvIndex = uvIndices[i];
		unsigned int normalIndex = normalIndices[i];

		// Get the attributes thanks to the index
		glm::vec3 vertex = temp_vertices[vertexIndex - 1];
		glm::vec2 uv = temp_uvs[uvIndex - 1];
		glm::vec3 normal = temp_normals[normalIndex - 1];

		// Put the attributes in buffers
		out_vertices.push_back(vertex);
		out_uvs.push_back(uv);
		out_normals.push_back(normal);

	}
	fclose(file);
	return true;
}

GLuint 	ditherStepsUniformLocation;
GLuint 	ditherBrightnessUniformLocation;
GLuint 	ditherDownscaleUniformLocation;
basicIDs ditherIDs;
bool bufferCInit = false;

GLuint ditherAccelerated(GLuint img, int w, int h, int steps, float brightness, float downscale)
{
	if (!bufferCInit)
	{
		ditherIDs.ShaderProgram = loadShader("rectVert", "ditherFrag");
		simpleQuadVAO(ditherIDs);
		ditherIDs.getBasicUniforms();

		//get uniforms
		ditherStepsUniformLocation = gl(GetUniformLocation(ditherIDs.ShaderProgram, "steps"));
		ditherBrightnessUniformLocation = gl(GetUniformLocation(ditherIDs.ShaderProgram, "brightness"));
		ditherDownscaleUniformLocation = gl(GetUniformLocation(ditherIDs.ShaderProgram, "downscale"));
		genFrameBuffer(&ditherIDs.bufferTextID, &ditherIDs.fboID, w, h);
	}

	simpleShaderBinding(ditherIDs, w, h);

	mat4 m = mat4(1);

	gl(Uniform1f(ditherIDs.aspectUniformLocation, 1.0f));
	gl(Uniform2f(ditherIDs.imSizeUniformLocation, w, h));
	gl(Uniform1f(ditherBrightnessUniformLocation, brightness));
	gl(Uniform1f(ditherStepsUniformLocation, steps));
	gl(Uniform1f(ditherDownscaleUniformLocation, downscale));
	gl(ActiveTexture(GL_TEXTURE0 + 0));
	gl(BindTexture(GL_TEXTURE_2D, img));

	gl(UniformMatrix4fv(ditherIDs.xformUniformLocation, 1, GL_FALSE, value_ptr(m)));

	gl(DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0));

	bufferCInit = true;
	gl(BindFramebuffer(GL_FRAMEBUFFER, 0));
	gl(ActiveTexture(GL_TEXTURE0));

	return ditherIDs.bufferTextID;
}


GLuint obj_MatrixID;
GLuint obj_ViewMatrixID;
GLuint obj_ModelMatrixID;
GLuint obj_vertexbuffer;
GLuint obj_uvbuffer;
GLuint obj_normalbuffer;
GLuint obj_LightID;
GLuint obj_VertexArrayID;
GLuint obj_light;
basicIDs objIDs;
bool bufferDInit = false;
float ddddangle = 0;
std::vector<glm::vec3> obj_vertices;
std::vector<glm::vec2> obj_uvs;
std::vector<glm::vec3> obj_normals;

#include<ogl/controls.hpp>

GLuint renderModel(int w, int h, GLFWwindow * win, mat4 modelRotation, vec3 scale, float lightPower)
{
	gl(Disable(GL_BLEND));
	gl(ClearColor(0.0f, 0.0f, 0.0f, 0.0f));
	// Enable depth test
	gl(Enable(GL_DEPTH_TEST));
	// Accept fragment if it closer to the camera than the former one
	gl(DepthFunc(GL_LESS));
	// Cull triangles which normal is not towards the camera
	gl(Enable(GL_CULL_FACE));
	if (!bufferDInit)
		gl(GenVertexArrays(1, &obj_VertexArrayID));
	gl(BindVertexArray(obj_VertexArrayID));


	if (!bufferDInit) {

		objIDs.ShaderProgram = loadShader("StandardShadingVert", "StandardShadingFrag");

		obj_MatrixID = gl(GetUniformLocation(objIDs.ShaderProgram, "MVP"));
		obj_ViewMatrixID = gl(GetUniformLocation(objIDs.ShaderProgram, "V"));
		obj_ModelMatrixID = gl(GetUniformLocation(objIDs.ShaderProgram, "M"));

		bool res = loadOBJ("../Data/3D/donut.obj", obj_vertices, obj_uvs, obj_normals);


		gl(GenBuffers(1, &obj_vertexbuffer));
		gl(BindBuffer(GL_ARRAY_BUFFER, obj_vertexbuffer));
		gl(BufferData(GL_ARRAY_BUFFER, obj_vertices.size() * sizeof(glm::vec3), &obj_vertices[0], GL_STATIC_DRAW));

		gl(GenBuffers(1, &obj_uvbuffer));
		gl(BindBuffer(GL_ARRAY_BUFFER, obj_uvbuffer));
		gl(BufferData(GL_ARRAY_BUFFER, obj_uvs.size() * sizeof(glm::vec2), &obj_uvs[0], GL_STATIC_DRAW));

		gl(GenBuffers(1, &obj_normalbuffer));
		gl(BindBuffer(GL_ARRAY_BUFFER, obj_normalbuffer));
		gl(BufferData(GL_ARRAY_BUFFER, obj_normals.size() * sizeof(glm::vec3), &obj_normals[0], GL_STATIC_DRAW));

		gl(UseProgram(objIDs.ShaderProgram));
		obj_LightID = gl(GetUniformLocation(objIDs.ShaderProgram, "LightPosition_worldspace"));
		obj_light = gl(GetUniformLocation(objIDs.ShaderProgram, "lightPow"));

		genFrameBuffer(&objIDs.bufferTextID, &objIDs.fboID, w, h);
	}


	gl(BindBuffer(GL_ARRAY_BUFFER, 0));
	gl(BindFramebuffer(GL_FRAMEBUFFER, objIDs.fboID));
	gl(Viewport(0, 0, w, h));
	gl(Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

	gl(UseProgram(objIDs.ShaderProgram));

	computeMatricesFromInputs();
	mat4 ProjectionMatrix = getProjectionMatrix();
	mat4 ViewMatrix = getViewMatrix();
	mat4 ModelMatrix = glm::mat4(1.0);

	ModelMatrix = glm::scale(ModelMatrix, scale);

	ModelMatrix = modelRotation * ModelMatrix;

	mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

	gl(UniformMatrix4fv(obj_MatrixID, 1, GL_FALSE, &MVP[0][0]));
	gl(UniformMatrix4fv(obj_ModelMatrixID, 1, GL_FALSE, value_ptr(ModelMatrix)));
	gl(UniformMatrix4fv(obj_ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]));

	glm::vec3 lightPos = glm::vec3(2, -2, 4);
	gl(Uniform3f(obj_LightID, lightPos.x, lightPos.y, lightPos.z));
	gl(Uniform1f(obj_light, lightPower));

	// 1rst attribute buffer : vertices
	gl(EnableVertexAttribArray(0));
	gl(BindBuffer(GL_ARRAY_BUFFER, obj_vertexbuffer));
	gl(VertexAttribPointer(
		0,                  // attribute
		3,                  // size
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		0,                  // stride
		(void*)0            // array buffer offset
	));

	// 2nd attribute buffer : UVs
	gl(EnableVertexAttribArray(1));
	gl(BindBuffer(GL_ARRAY_BUFFER, obj_uvbuffer));
	gl(VertexAttribPointer(
		1,                                // attribute
		2,                                // size
		GL_FLOAT,                         // type
		GL_FALSE,                         // normalized?
		0,                                // stride
		(void*)0                          // array buffer offset
	));

	// 3rd attribute buffer : normals
	gl(EnableVertexAttribArray(2));
	gl(BindBuffer(GL_ARRAY_BUFFER, obj_normalbuffer));
	gl(VertexAttribPointer(
		2,                                // attribute
		3,                                // size
		GL_FLOAT,                         // type
		GL_FALSE,                         // normalized?
		0,                                // stride
		(void*)0                          // array buffer offset
	));

	gl(DrawArrays(GL_TRIANGLES, 0, obj_vertices.size()));

	gl(DisableVertexAttribArray(0));
	gl(DisableVertexAttribArray(1));
	gl(DisableVertexAttribArray(2));

	//// Cleanup VBO and shader
	//glDeleteBuffers(1, &vertexbuffer);
	//glDeleteBuffers(1, &uvbuffer);
	//glDeleteBuffers(1, &normalbuffer);
	//glDeleteProgram(programID);
	//glDeleteTextures(1, &Texture);
	//glDeleteVertexArrays(1, &VertexArrayID);

	bufferDInit = true;
	gl(BindFramebuffer(GL_FRAMEBUFFER, 0));
	return objIDs.bufferTextID;
}




GLuint depth_MatrixID;
GLuint depth_ViewMatrixID;
GLuint depth_ModelMatrixID;
GLuint depth_vertexbuffer;
GLuint depth_uvbuffer;
GLuint depth_normalbuffer;
GLuint depth_LightID;
GLuint depth_VertexArrayID;
GLuint depth_light;
basicIDs depthOnlyIDs;
bool bufferDepthOnlyInit = false;
std::vector<glm::vec3> depth_vertices;
std::vector<glm::vec2> depth_uvs;
std::vector<glm::vec3> depth_normals;

GLuint renderModelDepth(int w, int h, GLFWwindow* win, mat4 modelRotation, vec3 scale, float lightPower)
{
	gl(Disable(GL_BLEND));
	gl(ClearColor(0.0f, 0.0f, 0.0f, 1.0f));
	// Enable depth test
	gl(Enable(GL_DEPTH_TEST));
	// Accept fragment if it closer to the camera than the former one
	gl(DepthFunc(GL_LESS));
	// Cull triangles which normal is not towards the camera
	gl(Enable(GL_CULL_FACE));
	if (!bufferDepthOnlyInit)
		gl(GenVertexArrays(1, &depth_VertexArrayID));
	gl(BindVertexArray(depth_VertexArrayID));


	if (!bufferDepthOnlyInit) {

		depthOnlyIDs.ShaderProgram = loadShader("StandardShadingVert", "depthFrag");

		depth_MatrixID = gl(GetUniformLocation(depthOnlyIDs.ShaderProgram, "MVP"));
		depth_ViewMatrixID = gl(GetUniformLocation(depthOnlyIDs.ShaderProgram, "V"));
		depth_ModelMatrixID = gl(GetUniformLocation(depthOnlyIDs.ShaderProgram, "M"));

		bool res = loadOBJ("../Data/3D/donut.obj", depth_vertices, depth_uvs, depth_normals);


		gl(GenBuffers(1, &depth_vertexbuffer));
		gl(BindBuffer(GL_ARRAY_BUFFER, depth_vertexbuffer));
		gl(BufferData(GL_ARRAY_BUFFER, depth_vertices.size() * sizeof(glm::vec3), &depth_vertices[0], GL_STATIC_DRAW));

		gl(GenBuffers(1, &depth_uvbuffer));
		gl(BindBuffer(GL_ARRAY_BUFFER, depth_uvbuffer));
		gl(BufferData(GL_ARRAY_BUFFER, depth_uvs.size() * sizeof(glm::vec2), &depth_uvs[0], GL_STATIC_DRAW));

		gl(GenBuffers(1, &depth_normalbuffer));
		gl(BindBuffer(GL_ARRAY_BUFFER, depth_normalbuffer));
		gl(BufferData(GL_ARRAY_BUFFER, depth_normals.size() * sizeof(glm::vec3), &depth_normals[0], GL_STATIC_DRAW));

		gl(UseProgram(depthOnlyIDs.ShaderProgram));
		depth_LightID = gl(GetUniformLocation(depthOnlyIDs.ShaderProgram, "LightPosition_worldspace"));
		depth_light = gl(GetUniformLocation(depthOnlyIDs.ShaderProgram, "lightPow"));

		genFrameBuffer(&depthOnlyIDs.bufferTextID, &depthOnlyIDs.fboID, w, h);
	}


	gl(BindBuffer(GL_ARRAY_BUFFER, 0));
	gl(BindFramebuffer(GL_FRAMEBUFFER, depthOnlyIDs.fboID));
	gl(Viewport(0, 0, w, h));
	gl(Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

	gl(UseProgram(depthOnlyIDs.ShaderProgram));

	computeMatricesFromInputs();
	mat4 ProjectionMatrix = getProjectionMatrix();
	mat4 ViewMatrix = getViewMatrix();
	mat4 ModelMatrix = glm::mat4(1.0);

	ModelMatrix = glm::scale(ModelMatrix, scale);

	ModelMatrix = modelRotation * ModelMatrix;

	mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

	gl(UniformMatrix4fv(depth_MatrixID, 1, GL_FALSE, &MVP[0][0]));
	gl(UniformMatrix4fv(depth_ModelMatrixID, 1, GL_FALSE, value_ptr(ModelMatrix)));
	gl(UniformMatrix4fv(depth_ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]));

	glm::vec3 lightPos = glm::vec3(0, 0, 4);
	gl(Uniform3f(depth_LightID, lightPos.x, lightPos.y, lightPos.z));
	gl(Uniform1f(depth_light, lightPower));

	// 1rst attribute buffer : vertices
	gl(EnableVertexAttribArray(0));
	gl(BindBuffer(GL_ARRAY_BUFFER, depth_vertexbuffer));
	gl(VertexAttribPointer(
		0,                  // attribute
		3,                  // size
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		0,                  // stride
		(void*)0            // array buffer offset
	));

	// 2nd attribute buffer : UVs
	gl(EnableVertexAttribArray(1));
	gl(BindBuffer(GL_ARRAY_BUFFER, depth_uvbuffer));
	gl(VertexAttribPointer(
		1,                                // attribute
		2,                                // size
		GL_FLOAT,                         // type
		GL_FALSE,                         // normalized?
		0,                                // stride
		(void*)0                          // array buffer offset
	));

	// 3rd attribute buffer : normals
	gl(EnableVertexAttribArray(2));
	gl(BindBuffer(GL_ARRAY_BUFFER, depth_normalbuffer));
	gl(VertexAttribPointer(
		2,                                // attribute
		3,                                // size
		GL_FLOAT,                         // type
		GL_FALSE,                         // normalized?
		0,                                // stride
		(void*)0                          // array buffer offset
	));

	gl(DrawArrays(GL_TRIANGLES, 0, depth_vertices.size()));

	gl(DisableVertexAttribArray(0));
	gl(DisableVertexAttribArray(1));
	gl(DisableVertexAttribArray(2));

	//// Cleanup VBO and shader
	//glDeleteBuffers(1, &vertexbuffer);
	//glDeleteBuffers(1, &uvbuffer);
	//glDeleteBuffers(1, &normalbuffer);
	//glDeleteProgram(programID);
	//glDeleteTextures(1, &Texture);
	//glDeleteVertexArrays(1, &VertexArrayID);

	bufferDepthOnlyInit = true;
	gl(BindFramebuffer(GL_FRAMEBUFFER, 0));
	return depthOnlyIDs.bufferTextID;
}

basicIDs depthIDs;
GLuint clippingPlane;
bool bufferDepthInit = false;

GLuint visualizeDepthBuffer(GLuint fbo, int w, int h, float nearPlane, float farPlane)
{
	if (!bufferDepthInit)
	{
		depthIDs.ShaderProgram = loadShader("rectVert", "visualDepthFrag");
		simpleQuadVAO(depthIDs);
		depthIDs.getBasicUniforms();
		clippingPlane = gl(GetUniformLocation(depthIDs.ShaderProgram, "clippingPlane"));

		//get uniforms
		genFloatFrameBuffer(&depthIDs.bufferTextID, &depthIDs.fboID, w, h);
	}

	simpleShaderBinding(depthIDs, w, h, false, true, vec3(0));
	gl(Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

	mat4 m = mat4(1);

	gl(Uniform1f(depthIDs.aspectUniformLocation, 1.0f));
	gl(Uniform2f(depthIDs.imSizeUniformLocation, w, h));
	gl(Uniform2f(clippingPlane, nearPlane, farPlane));
	gl(ActiveTexture(GL_TEXTURE0 + 0));
	gl(BindTexture(GL_TEXTURE_2D, fbo));

	gl(UniformMatrix4fv(depthIDs.xformUniformLocation, 1, GL_FALSE, value_ptr(m)));

	gl(DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0));

	bufferDepthInit = true;
	gl(BindFramebuffer(GL_FRAMEBUFFER, 0));
	gl(ActiveTexture(GL_TEXTURE0));

	return depthIDs.bufferTextID;
}


#undef uint8_t;