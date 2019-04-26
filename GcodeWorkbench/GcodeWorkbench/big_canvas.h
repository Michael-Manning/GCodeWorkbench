#pragma once
#include <Gcode/curve_tools.h>
#include "GL_basics.h"
#include <queue>
#include "safe_que.h"

using namespace glm;
using namespace std;


class bigCanvas {

	queue<curve> lineQue;
	bool clearFlag = false;
	bool initFlag = false;
	vec2 canSize;


	bool bufferInit = false;
	bool streamBufferInit = false;

	basicIDs IDs;
	basicIDs streamIDs;

	//GLuint canvasShaderProgram;
	//GLuint VAO, VBO, EBO;
	//GLuint aspectUniformLocation;
	GLuint scaleUniformLocation;
	GLuint angleUniformLocation;
	GLuint posUniformLocation;
	GLuint colorUniformLocation;
	GLuint streamVBO;
	GLuint staticRectVBO;
	//	GLuint streamAspectVBO;
	//	GLuint streamShaderProgram;
	GLuint streamLineWidth;
	//GLuint bufferTextID, fboID;

	void drawLine(curve line) {

		float deltaY = (line.start.y - line.end.y);
		float deltaX = (line.start.x - line.end.x);
		float angle = atan(deltaY / deltaX);
		vec2 position = vec2((line.start.x + line.end.x) / 2, (line.start.y + line.end.y) / 2) / (canSize / 2.0f) + (-1.0f);
		vec2 scale = vec2(sqrt(deltaY * deltaY + deltaX * deltaX), lineWidth) / canSize;

		gl(Uniform1f(IDs.aspectUniformLocation, canSize.y / canSize.x));


		gl(Uniform2f(scaleUniformLocation, scale.x, scale.y));
		gl(Uniform2f(posUniformLocation, position.x, position.y));
		gl(Uniform1f(angleUniformLocation, angle));
		//	if (deltaY == 0) {
		///		gl(Uniform4f(colorUniformLocation, 0, 0, 1, lineColor.a));
		//	}
		//	else {
		gl(Uniform4f(colorUniformLocation, lineColor.r, lineColor.g, lineColor.b, lineColor.a));
		//	}

		gl(DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0));
	}

public:
	vec4 lineColor;
	vec3 backgroundColor;
	float lineWidth;
	SafeQueue<vec4> * parallelQue = NULL; //will draw lines from if set

	bigCanvas(vec2 size = vec2(), float _lineWidth = 2.0f, vec4 lineCol = vec4(0, 0, 0, 1), vec3 bgColor = vec4(1.0f)) {
		canSize = size;
		lineColor = lineCol;
		backgroundColor = bgColor;
		lineWidth = _lineWidth;
	}

	int queLength() {
		return lineQue.size() + parallelQue != NULL ? parallelQue->size() : 0;
	}
	void addToBuffer(curve c) {
		lineQue.push(c);
	};
	void addToBuffer(vec2 A, vec2 B) {
		addToBuffer(cLine(A, B));
	};
	void addToBuffer(vector<curve> curves) {
		for (int i = 0; i < curves.size(); i++)
			lineQue.push(curves[i]);
	}
	void addToBuffer(vector<vector<curve>> loops) {
		for (int i = 0; i < loops.size(); i++)
		{
			for (int j = 0; j < loops[i].size(); j++)
				lineQue.push(loops[i][j]);
			breakLoop();
		}
	}
	void breakLoop() {
		curve breakCurve;
		breakCurve.type = -1;
		lineQue.push(breakCurve);
	}

	void clearCanvas() {
		clearFlag = true;
		while (!lineQue.empty())
			lineQue.pop();
		while (parallelQue->size())
			parallelQue->dequeue();
		bufferInit = false;
		streamBufferInit = false;
	}
	void resetFramBuffer() {
		bufferInit = false;
	}


	//renders as many lines to the buffer as possible within a time limit and returns progress
	GLuint render(double timeLimit = -1) {

		if (!bufferInit)
		{
			IDs.ShaderProgram = loadShader("lineVert", "lineFrag");
			simpleQuadVAO(IDs);

			//get uniforms
			IDs.getAspectLocation();
			angleUniformLocation = gl(GetUniformLocation(IDs.ShaderProgram, "angle"));
			scaleUniformLocation = gl(GetUniformLocation(IDs.ShaderProgram, "scale"));
			posUniformLocation = gl(GetUniformLocation(IDs.ShaderProgram, "pos"));
			colorUniformLocation = gl(GetUniformLocation(IDs.ShaderProgram, "color"));
			genFrameBuffer(&IDs.bufferTextID, &IDs.fboID, (int)canSize.x, (int)canSize.y);
		}

		simpleShaderBinding(IDs, (int)canSize.x, (int)canSize.y, true, clearFlag || !bufferInit);
		clearFlag = false;
		bufferInit = true;

		gl(UseProgram(IDs.ShaderProgram));
		double timer = glfwGetTime();
		int renderCount = 0;

		while (lineQue.size() > 0) {
			curve line = lineQue.front();
			lineQue.pop();
			drawLine(line);
			renderCount++;
			if (timeLimit != -1 && renderCount % 50) {
				if (glfwGetTime() - timer > timeLimit)
					break;
			}
		}
		if (parallelQue != NULL) {
			while (parallelQue->size()) {
				vec4 data = parallelQue->dequeue();
				curve line = cLine(vec2(data.x, data.y), vec2(data.z, data.w));
				drawLine(line);
				renderCount++;
				if (timeLimit != -1 && renderCount % 50) {
					if (glfwGetTime() - timer > timeLimit)
						break;
				}
			}
		}

		gl(BindFramebuffer(GL_FRAMEBUFFER, 0));
		return 	IDs.bufferTextID;
	}



	//////batch


	GLuint quadVAO, quadVBO;
	GLuint instanceVBO;
	GLuint renderBatch(vec4 * transforms, int streamBatchSize) {

		float quadVertices[] = {
	-0.5f,  0.5f,
	 0.5f, -0.5f,
	-0.5f, -0.5f,

	-0.5f,  0.5f,
	 0.5f, -0.5f,
	 0.5f,  0.5f,
		};

		if (!streamBufferInit) {
			streamIDs.ShaderProgram = loadShader("batchLineVert", "lineFrag");
			streamIDs.getAspectLocation();
			streamIDs.getImSizeLocation();
			streamLineWidth = gl(GetUniformLocation(streamIDs.ShaderProgram, "width"));

			genFrameBuffer(&streamIDs.bufferTextID, &streamIDs.fboID, (int)canSize.x, (int)canSize.y);
			gl(GenBuffers(1, &instanceVBO));
			gl(GenVertexArrays(1, &quadVAO));
			gl(GenBuffers(1, &quadVBO));
		}

		gl(Disable(GL_DEPTH_TEST));
		gl(Disable(GL_CULL_FACE));

		gl(BindFramebuffer(GL_FRAMEBUFFER, streamIDs.fboID));

		gl(Viewport(0, 0, (int)canSize.x, (int)canSize.y));
		//vertex
		gl(BindVertexArray(quadVAO));

		gl(BindBuffer(GL_ARRAY_BUFFER, quadVBO));
		gl(BufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW));

		//vertex
		gl(EnableVertexAttribArray(1));
		gl(VertexAttribPointer(
			1,
			2,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0)
		);

		//instance
		
		gl(BindBuffer(GL_ARRAY_BUFFER, instanceVBO));
		gl(BufferData(GL_ARRAY_BUFFER, sizeof(vec4) * streamBatchSize, transforms, GL_STREAM_DRAW)); //GL_STATIC_DRAW));
		gl(BindBuffer(GL_ARRAY_BUFFER, 0));

		//instance
		gl(EnableVertexAttribArray(2));
		gl(BindBuffer(GL_ARRAY_BUFFER, instanceVBO));
		gl(VertexAttribPointer(
			2,
			4,
			GL_FLOAT,
			GL_TRUE,
			0,
			(void*)0));


		gl(BindBuffer(GL_ARRAY_BUFFER, 0));
		gl(VertexAttribDivisor(2, 1));


		gl(ClearColor(1, 1, 1, 1.0f));
		gl(Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));


		gl(UseProgram(streamIDs.ShaderProgram));
		gl(BindVertexArray(quadVAO));

		gl(Uniform1f(streamIDs.aspectUniformLocation, (float)canSize.y / (float)canSize.x));
		gl(Uniform2f(streamIDs.imSizeUniformLocation, canSize.x, canSize.y));
		gl(Uniform1f(streamLineWidth, lineWidth));


		gl(DrawArraysInstanced(GL_TRIANGLES, 0, 6, streamBatchSize));


	//	gl(DisableVertexAttribArray(1));
	//	gl(DisableVertexAttribArray(2));
		gl(BindVertexArray(0));
		gl(BindFramebuffer(GL_FRAMEBUFFER, 0));
		streamBufferInit = true;

		return 	streamIDs.bufferTextID;
	}
};









