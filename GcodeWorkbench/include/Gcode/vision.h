#pragma once
#include<Gcode/curve_tools.h>
#include <vector>
#include <iostream>
#include <string>

#include <stdio.h>
#include <tchar.h>


#include <math.h>
#include <assert.h>

#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include <opencv2/opencv.hpp>
#include <glm/common.hpp>



// only works with 1000 by 1000 px right now
void gridDetect(int dimx, int dimy, cv::Mat original,
	int* cellSize,
	int* xCells,
	int* yCells,
	int* top,
	int* bottom,
	int* left,
	int* right,
	bool drawLines = false);

char recognizeLetter(cv::Mat img);
//
// enum order: notfound = -1, right, left, up, down, topright, bottomright, topleft, bottomleft
struct wordLocation {
	int x, y, directionType;
	glm::vec2 direction() {
		return glm::vec2(
			1 * !(directionType == 2 || directionType == 3) - 2 * (directionType == 1 || directionType >= 6),
			1 * !(directionType < 2) - 2 * (directionType == 3 || directionType == 5 || directionType == 7));
	}
};

bool solveWordSearch(cv::Mat img, std::vector<wordLocation>* words, std::vector<std::string>* serachlist, glm::vec4* searchCrop, int* cols, int* rows, glm::vec2* cellSize, curve* nameLine, curve* dateLine, float* angle, bool thresholdRequired);

template<class T>
class Grid {
	int cols, rows;
public:
	T* Data;
	Grid(int Cols, int Rows) {
		cols = Cols;
		rows = Rows;
		Data = new T[cols * rows];
	};
	T at(int x, int y) {
		return Data[cols * y + x];
	};
	T* operator[](int y) {
		return Data + cols * y;
	}
	~Grid() {
		delete[] Data;
	}
};

