#pragma once
#include"GL_basics.h"
#include <Gcode/curve_tools.h>
#include "safe_que.h"

//gets the wave art from the image on one thread
vector<curve> calculateWaveArt(int w, int h, int lineDiv, uint8_t* waveData, float base, float freq, float amp, float cut, double timeLimit, int * progress);
int calculateWaveArt(vec4 * buffer, int bufferLength, int w, int h, int lineDiv, uint8_t* waveData, float base, float freq, float amp, float cut);

//same thing but in parallel
void calculateWaveArtParalell(int w, int h, int lineDiv, uint8_t* waveData, float base, float freq, float amp, float cut, SafeQueue<vec4> * que, int * remainingJobs);

//get's the visualization only but does so extremely quickly
GLuint renderWavesAsEquation(uint8_t* waveData, int w, int h, int lineDiv, float base, float freq, float amp, float cut, bool updateBuffers);

//dithers grayscale data and returns texture of image
void simpleDither(unsigned char * img, int w, int h);
GLuint ditherAccelerated(GLuint img, int w, int h, int steps, float brightness = 0, float downscale = 1);

void waveArtJoinThreads();

GLuint getWaveArtAveragedLines(uint8_t * img, int w, int h, float lineDiv, float smoothing);

unsigned char * getSpiralData(uint8_t* imageData, int w, int h, int rotations, float precision = 1.0f);

GLuint renderModel(int w, int h, GLFWwindow * win, mat4 rotation, vec3 scale, float lightPower);
GLuint renderModelDepth(int w, int h, GLFWwindow * win, mat4 rotation, vec3 scale, float lightPower);

GLuint visualizeDepthBuffer(GLuint fbo, int w, int h, float nearPlane, float farPlane);