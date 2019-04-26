#pragma once

#include "curve_tools.h"
#include <stb/stb_truetype.h>
#include <vector>
using namespace std;

#ifndef pen_up_cmd
#define pen_up_cmd "M42 P60 S0"
#endif 
#ifndef pen_down_cmd
#define pen_down_cmd "M42 P60 S1"
#endif

class typeWriter {
	unsigned char buffer[24 << 20];
	stbtt_vertex * vertices[30];
	char previousLetter;
	int letterIndex = 0;
	int runningCounter = 0;
	stbtt_fontinfo font;
	vec2 cursor;


public:
	vector<curve> letterCurves; //actual curve data
	vector<int> moveTypes; //weather each curve is letter data or a reposition 
	vector<int> curveCounts; //curves in each letter
	vector<int> indexes; //initial index of each letter
	vector<float> xAdvances;
	vector<vec2> offsets;

	int ascent;
	int baseline;
	float letterHeight;
	float scaleFactor; // "C:/Users/Micha/Documents/fonts/Birds of Paradise.ttf");  //"c:/windows/fonts/ArialNova-Bold.ttf"  "c:/windows/fonts/ArialNova-Bold.ttf"

	void initFont(float height = 50, const char * fontFilepath = "c:/windows/fonts/ArialNB.TTF");
	void nextLetter(char letter);
	vector<Gcommand> getLetterGcode(int letterIndex, float speed, int dwellLength = 175, int arcSubdivision = 1, bool filledIn = false);
	void clear();
	void setCursor(vec2 pos) { cursor = pos; };
	void setCursor(float x, float y) { setCursor(vec2(x, y)); };
	void reScale(float height = 50) {
		scaleFactor = stbtt_ScaleForPixelHeight(&font, height);
	}
	int letterCount() {
		return indexes.size();
	};
};