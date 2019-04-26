#include"pch.h"
#define _CRT_SECURE_NO_WARNINGS

#define STB_TRUETYPE_IMPLEMENTATION 

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "font_tools.h"

using namespace std;
using namespace glm;

void typeWriter::initFont(float height, const char * fontFilepath)
{
	fread(buffer, 1, 1000000, fopen(fontFilepath, "rb"));
	stbtt_InitFont(&font, buffer, 0);
	scaleFactor = stbtt_ScaleForPixelHeight(&font, height);
	stbtt_GetFontVMetrics(&font, &ascent, 0, 0);
	baseline = (int)(ascent*scaleFactor);
	letterHeight = height;
}


void typeWriter::nextLetter(char letter) {

	if (letter == ' ') {
		float spaceWidth = letterHeight / 4;

		indexes.push_back(runningCounter);
		runningCounter += 1;
		xAdvances.push_back(0);
		curveCounts.push_back(1);
		letterIndex++;
		previousLetter = letter;
		letterCurves.push_back(cLine(vec2(spaceWidth, 0), vec2(spaceWidth, 0)) + cursor);
		moveTypes.push_back(1);

		cursor.x += spaceWidth;

		return;
	}

	if (letterIndex > 1)
		cursor.x += scaleFactor * stbtt_GetCodepointKernAdvance(&font, previousLetter, letter);

	for (int i = 0; i < 30; i++)
		vertices[i] = new stbtt_vertex[200];

	int advance, lsb;
	float x_shift = cursor.x - (float)floor(cursor.x);
	stbtt_GetCodepointHMetrics(&font, letter, &advance, &lsb);

	int glyph = stbtt_FindGlyphIndex(&font, letter);
	int vCount = stbtt_GetGlyphShape(&font, glyph, vertices);
	if (vCount > 200) {
		cout << "font vertex buffer exceeded!\n";
	}

	for (int i = 1; i < vCount && i < 200; i++) {

		stbtt_vertex vert = vertices[0][i];
		curve bl;
		if (vert.type == 3) {
			bl = cBezier(
				vec2(vertices[0][i - 1].x, vertices[0][i - 1].y),
				vec2(vert.cx, vert.cy),
				vec2(vert.x, vert.y)
			);
		}
		else {
			bl = cLine(
				vec2(vertices[0][i - 1].x, vertices[0][i - 1].y),
				vec2(vert.x, vert.y)
			);
		}
		letterCurves.push_back(bl * scaleFactor + cursor);
		moveTypes.push_back(vert.type);
	}
	indexes.push_back(runningCounter);
	runningCounter += vCount - 1;
	xAdvances.push_back(cursor.x);
	curveCounts.push_back(vCount);

	cursor.x += (advance * scaleFactor);

	for (int i = 0; i < 30; i++)
		delete vertices[i];

	letterIndex++;
	previousLetter = letter;
}

vector<Gcommand> typeWriter::getLetterGcode(int letterIndex, float speed, int dwellLength, int arcSubdivision, bool filledIn)
{
	//filledIn = true;
	vector<Gcommand> buff;
	char dwellBuff[64];
	dwell(dwellBuff, dwellLength);

	//initial move needs to be to the START of the first curve, and subsquent moves only use the ends
	{
		char smallbuff[64];
		curve c = letterCurves[indexes[letterIndex]];

		sprintf(smallbuff, "G1 X%.1f Y%.1f F%.1f", c.start.x, c.start.y, speed);
		buff.push_back(Gcommand(pen_up_cmd));
		buff.push_back(Gcommand(dwellBuff));
		buff.push_back(Gcommand(smallbuff));
	}

	for (int i = indexes[letterIndex]; i < indexes[letterIndex] + curveCounts[letterIndex] - 1; i++)
	{
		if (i == indexes[letterIndex]) {
			buff.push_back(Gcommand(pen_down_cmd));
			buff.push_back(Gcommand(dwellBuff));
		}

		char smallbuff[64];
		curve c = letterCurves[i];

		//none draw movement
		if (moveTypes[i] == 1) {
			getGcode(smallbuff, c, speed);
			buff.push_back(Gcommand(pen_up_cmd));
			buff.push_back(Gcommand(dwellBuff));
			buff.push_back(Gcommand(smallbuff));
			buff.push_back(Gcommand(pen_down_cmd));
			buff.push_back(Gcommand(dwellBuff));
		}
		//straight line
		if (moveTypes[i] == 2) {
			getGcode(smallbuff, c, speed);
			buff.push_back(Gcommand(smallbuff));
		}
		//arc/bezier
		if (moveTypes[i] == 3) {
			for (float j = 0; j < arcSubdivision; j++)
			{
				float t0 = (1.0f / arcSubdivision) * j;
				float t1 = t0 + (1.0f / arcSubdivision);
				detailedArc ar = getArcApproximateDetailed(c, t0, t1, true);
				curve other = fromArc(ar);
				getGcode(smallbuff, other, speed);
				buff.push_back(Gcommand(smallbuff));
			}
		}
	}
	buff.push_back(Gcommand(pen_up_cmd));
	//buff.clear();
//	buff.push_back(Gcommand(";temp"));

	if (filledIn) {

		int bezierDivisions = 6;
		vector<curve> loop;

		for (int i = indexes[letterIndex]; i < indexes[letterIndex] + curveCounts[letterIndex] - 1; i++) {
			if (moveTypes[i] == 2) {
				loop.push_back(letterCurves[i]);
			}
			else if ( moveTypes[i] == 3) {

				bezier3 b3 = bezier3{ letterCurves[i].points[0], letterCurves[i].points[1], letterCurves[i].points[2] };
		//		loop.push_back(cLine(letterCurves[i].p0, getBezierPoint(1.0f / bezierDivisions, b3)));

				for (int j = 1; j < bezierDivisions +1; j++)
				{
					loop.push_back(cLine(getBezierPoint((1.0f / bezierDivisions) * (j - 1), b3), getBezierPoint((1.0f / bezierDivisions) * (j), b3)));
				}

				//loop.push_back(cLine(letterCurves[i].p0, getBezierPoint((1.0f / bezierDivisions) * (bezierDivisions - 1), b3)));

				//vec2 mid = getBezierPoint(0.5, bezier3{ letterCurves[i].points[0], letterCurves[i].points[1], letterCurves[i].points[2] });
				//loop.push_back(cLine(letterCurves[i].p0, mid));
				//loop.push_back(cLine(mid, letterCurves[i].p2));
			}
		}
	//	if (letterCurves[indexes[letterIndex] + curveCounts[letterIndex] - 2].type == 3) 
	//		loop.push_back(cLine(loop[loop.size() -1].end, letterCurves[letterIndex].start));
	
		if (filledIn) {
			vector<vector<curve>> loops = getRectillinearInfill(loop, 1, 0.785398f);
			vector<Gcommand>infillCommands = createScript(loops, speed);

			for (int i = 0; i < infillCommands.size(); i++)
				buff.push_back(infillCommands[i]);
		}


		buff.push_back(Gcommand(pen_up_cmd));
	}

	return buff;
}

void typeWriter::clear()
{
	letterCurves.clear();
	curveCounts.clear();
	indexes.clear();
	xAdvances.clear();
	moveTypes.clear();
	offsets.clear();
	setCursor(0, 0);
	runningCounter = 0;
	letterIndex = 0;
}
