#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include "vision.h"

#include <vector>
#include <iostream>
#include <string>

#include <stdio.h>
#include <tchar.h>


#include <math.h>
#include <assert.h>
#include <algorithm>
#include <functional>

#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include <opencv2/opencv.hpp>
#include <glm/common.hpp>

#include <tesseract/baseapi.h>
#include <tesseract/genericvector.h>

#include<Gcode/curve_tools.h>

//#include <leptonica/allheaders.h>

using namespace std;
using namespace glm;
using namespace cv;


#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#pragma comment(lib, "WS2_32.lib")

#include <glm/common.hpp>
#define PI 3.14159265359f
#define DEGREES 57.2957795131f

void gridDetect(int dimx, int dimy, cv::Mat original,
	int* cellSize,
	int* xCells,
	int* yCells,
	int* top,
	int* bottom,
	int* left,
	int* right,
	bool drawLines) {

	cv::Mat gray;
	cv::Mat edges;

	Canny(original, edges, 50, 200, 3);
	cvtColor(edges, gray, CV_GRAY2BGR);

	vector<cv::Vec4i> lines;
	vector<cv::Vec4i> filtered;
	vector<cv::Vec4i> vertical;
	vector<cv::Vec4i> horizontal;
	vector<cv::Vec4i> Hlines;
	cv::HoughLines(edges, Hlines, 1, CV_PI / 180, 200);


	//detect lines
	for (size_t i = 0; i < Hlines.size(); i++)
	{
		float rho = Hlines[i][0], theta = Hlines[i][1];
		cv::Point pt1, pt2;
		double a = cos(theta), b = sin(theta);
		double x0 = a * rho, y0 = b * rho;
		pt1.x = cvRound(x0 + 1000 * (-b));
		pt1.y = cvRound(y0 + 1000 * (a));
		pt2.x = cvRound(x0 - 1000 * (-b));
		pt2.y = cvRound(y0 - 1000 * (a));
		//line(original, pt1, pt2, Scalar(0, 0, 255), 3, CV_AA);
		lines.push_back(cv::Vec4i(pt1.x, pt1.y, pt2.x, pt2.y));
	}

	//filter similar lines
	int thresh = 50;
	for (int i = 0; i < lines.size(); i++)
	{
		bool valid = true;
		for (int j = 0; j < filtered.size(); j++) {
			if ((abs(lines[i][0] - filtered[j][0]) < thresh) &&
				(abs(lines[i][1] - filtered[j][1]) < thresh) &&
				(abs(lines[i][2] - filtered[j][2]) < thresh) &&
				(abs(lines[i][3] - filtered[j][3]) < thresh))
				valid = false;
		}
		if (valid)
			filtered.push_back(lines[i]);
	}

	//sort lines into vertical / horizontal
	int axiThresh = 15;
	for (int i = 0; i < filtered.size(); i++) {
		if (abs(filtered[i][0] - filtered[i][2]) < axiThresh)
			vertical.push_back(cv::Vec4i(filtered[i][0], filtered[i][1], filtered[i][0], filtered[i][3]));
		else
			horizontal.push_back(cv::Vec4i(filtered[i][0], filtered[i][1], filtered[i][2], filtered[i][1]));
	}

	//get grid dimentions within image
	*top = 0;
	*bottom = 1000;
	*left = 1000;
	*right = 0;
	*cellSize = 0;

	for (int i = 0; i < vertical.size(); i++) {
		*left = glm::min(vertical[i][0], *left);
		*right = glm::max(vertical[i][2], *right);
	}
	for (int i = 0; i < horizontal.size(); i++) {
		*bottom = glm::min(horizontal[i][1], *bottom);
		*top = glm::max(horizontal[i][3], *top);
	}
	*cellSize = (*right - *left) / (vertical.size() - 1); //cellcount = linecount - 1;

	//draw found lines
	if (drawLines) {
		for (int i = 0; i < vertical.size(); i++)
			line(original, cv::Point(vertical[i][0], *bottom), cv::Point(vertical[i][2], *top), cv::Scalar(0, 0, 255), 3, CV_AA);

		for (int i = 0; i < horizontal.size(); i++)
			line(original, cv::Point(*left, horizontal[i][1]), cv::Point(*right, horizontal[i][3]), cv::Scalar(255, 0, 0), 3, CV_AA);

	}
}

tesseract::TessBaseAPI* ocr;

bool tesseractInit = false;
void initTesseract() {
	if (tesseractInit)
		return;
	tesseractInit = true;

	ocr = new tesseract::TessBaseAPI();
	ocr->Init(NULL, "eng", tesseract::OEM_LSTM_ONLY);
	ocr->SetPageSegMode(tesseract::PSM_SINGLE_CHAR);
}

char recognizeLetter(cv::Mat img) {

	initTesseract();
	// Open input image using OpenCV

	//cv::imwrite("img.png", img);
	//img = cv::imread("img.png");

	// Set image data
	ocr->SetImage(img.data, img.cols, img.rows, 3, img.step);
	// Run Tesseract OCR on image
	auto text = std::string(ocr->GetUTF8Text());
	std::cout << text << std::endl; // Destroy used object and release memory ocr->End();
	//ocr->End();
	return text[0];
}

#include <Gcode/curve_tools.h>

vec4 getPixelBoundingBox(cv::Mat img, int threshold) {
	int minX = img.cols, minY = img.rows, maxX = 0, maxY = 0;
	cv::Mat gray;
	cvtColor(img, gray, cv::COLOR_BGR2GRAY);

	for (int i = 0; i < gray.rows; i++)
	{
		for (int j = 0; j < gray.cols; j++)
		{
			if (gray.at<uchar>(i, j) < threshold)
			{
				minX = glm::min(minX, j);
				minY = glm::min(minY, i);
				maxX = glm::max(maxX, j);
				maxY = glm::max(maxY, i);
			}
		}
	}
	return vec4(minX, minY, maxX - minX, maxY - minY);
}

void setColor(int id) {
	/*char buf[12];
	sprintf(buf, "Color 0%c", 65 + (uchar)id);
	system(buf);*/
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), id);
}

bool solveWordSearch(cv::Mat img, vector<wordLocation>* words, std::vector<std::string>* retSerachlist, vec4* searchCrop, int* cols, int* rows, glm::vec2* cellSize, curve* nameLine, curve* dateLine, float* angle, bool thresholdRequired)
{
	curve separator;
	int imw = img.cols, imh = img.rows;
	cv::Mat threshed, edges;

	if (thresholdRequired)
	{
		cout << "performing threshold" << endl;


		Mat gray;
		cvtColor(img, gray, CV_BGR2GRAY);
		Mat binary;
		cv::threshold(gray, binary, 0, 255, CV_THRESH_BINARY | cv::THRESH_OTSU);
		GaussianBlur(binary, threshed, Size(5, 5), 0);
		//imshow("thresh, blurred", threshed);


		//Mat gray;
		//cvtColor(img, gray, CV_BGR2GRAY);
		//Mat blur;
		//Mat binary;
		//GaussianBlur(gray, blur, Size(5, 5), 0);
		//cv::threshold(blur, binary, 0, 255, CV_THRESH_BINARY | cv::THRESH_OTSU);
		//GaussianBlur(binary, threshed, Size(5, 5), 0);
		////cv::threshold(blur, threshed, 170, 255, CV_THRESH_BINARY);
		//imshow("thresh, blurred", threshed);


	}
	else
		threshed = img;


	Canny(threshed, edges, 50, 200, 3);
	//imshow("edges", edges);

	//correct angle
	{
#if 1
		cout << "correcting image angle" << endl;
		vector<curve> allLines;
		vector<cv::Vec4i> linesP; // will hold the results of the detection
		HoughLinesP(edges, linesP, 1, CV_PI / 180, 100, 100, 10); // runs the actual detection
		for (size_t i = 0; i < linesP.size(); i++)
		{
			cv::Vec4i l = linesP[i];
			//	line(img, cv::Point(l[0], l[1]), cv::Point(l[2], l[3]), cv::Scalar(0, 0, 255), 3, cv::LINE_AA);
				//separator = cLine(vec2(l[0], l[1]), vec2(l[2], l[3]));
			allLines.push_back(cLine(vec2(l[0], l[1]), vec2(l[2], l[3])));
		}

		//find 10 longest lines to average angles from
		std::sort(allLines.begin(), allLines.end());
		float accumulatedAngle = 0;
		int sampleCount = glm::min(10, (int)allLines.size());

		if (!sampleCount) {
			cout << "zero lines detected when correcting angle. aborting" << endl;
			return false;
		}

		for (int i = 0; i < sampleCount; i++)
		{
			//line(img, Point(allLines[i].start.x, allLines[i].start.y), Point(allLines[i].end.x, allLines[i].end.y), Scalar(0, 0, 255), 3, LINE_AA);
			curve line = allLines[i];
			if (!line.start.y > line.end.y)
				line = flipLine(line);

			float angle = atan((line.end.x - line.start.x) / (line.end.y - line.start.y));

			if (angle > PI / 4)
				angle -= PI / 2;
			else if (angle < -PI / 4)
				angle += PI / 2;

			accumulatedAngle += angle;
		}
		accumulatedAngle /= sampleCount;
	//	accumulatedAngle = -0.0122173f;
		*angle = accumulatedAngle;
		
		cout << "averaged " << sampleCount << " samples to " << accumulatedAngle * DEGREES << " degrees" << endl;

		cv::Mat rot_mat = cv::getRotationMatrix2D(cv::Point2f(imw / 2, imh / 2), -accumulatedAngle * DEGREES, 1);
		cv::Mat rotated;

		cv::warpAffine(threshed, rotated, rot_mat, threshed.size(), cv::INTER_CUBIC, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
		rotated.copyTo(threshed);

		cv::warpAffine(img, rotated, rot_mat, img.size(), cv::INTER_CUBIC, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
		rotated.copyTo(img);

		cv::warpAffine(edges, rotated, rot_mat, threshed.size(), cv::INTER_CUBIC, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
		rotated.copyTo(edges);
		//imshow("corrected rotation", img);
#else
		cout << "Skipping rotation correction due to preprocessor block settings" << endl;
#endif
	}

	// find seperator line
	float serperatorAngle;
	{
		cout << "finding seperator line..." << endl;
		vector <curve> allLines;
		vector<cv::Vec4i> linesP; // will hold the results of the detection`
		HoughLinesP(edges, linesP, 1, (CV_PI * 4) / 180, 15, 100, 0); // runs the actual detection
		//HoughLinesP(edges, linesP, 1, CV_PI / 180, imh / 2, imh / 2, 10); // runs the actual detection
		//HoughLinesP(edges, linesP, 1, CV_PI / 180, 50, 50, 10); // runs the actual detection
		for (size_t i = 0; i < linesP.size(); i++)
		{
			cv::Vec4i l = linesP[i];
			curve line = cLine(vec2(l[0], l[1]), vec2(l[2], l[3]));
			if (abs(line.end.x - line.start.x) < abs(line.end.y - line.start.y))
				allLines.push_back(line);
			//cv::line(threshed, cv::Point(l[0], l[1]), cv::Point(l[2], l[3]), cv::Scalar(0, 0, 255), 12, cv::LINE_AA);
		}
		//imshow("all detected seperators", threshed);
		if (!allLines.size()) {
			cout << "failed to detect separator line" << endl;
			return false;
		}

		vector<curve> filteredSeparators;
		for (int i = 0; i < allLines.size(); i++)
		{
			curve line = allLines[i];
			vec2 halfwaydown = getLinearPoint(0.5f, line.start, line.end);
			int scanner = (int)halfwaydown.y;
			int scanX = (int)halfwaydown.x;
			while (scanner < threshed.rows) {
				for (int i = -15; i < 15; i++)
				{
					if (scanner < 5 || scanner > imh - 5 || i + scanX < 5 || i + scanX > imw - 5)
						goto skip;
					if (threshed.at<uchar>(scanner, i + scanX) < (uchar)250)
						break;
					else if (i == 14)
						goto next;
				}
				scanner++;
			}
		next:;
			line.start.y = scanner;
			scanner = (int)halfwaydown.y;
			while (scanner > 0) {
				for (int i = -15; i < 15; i++)
				{
					if (scanner < 5 || scanner > imh - 5 || i + scanX < 5 || i + scanX > imw - 5)
						goto skip;
					if (threshed.at<uchar>(scanner, i + scanX) < (uchar)250)
						break;
					else if (i == 14)
						goto done;
				}
				scanner--;
			}
		done:;
			line.end.y = scanner;
			filteredSeparators.push_back(line);
		skip:;
		}

		curve longest = longestLine(filteredSeparators);
		separator = longest;

		//at this point it is usually fine, but the hough lines can cut it short, so it is refined here

	//	vec2 halfwaydown = getLinearPoint(0.5f, separator.start, separator.end);

	//	int scanner = (int)halfwaydown.y;
	//	int scanX = (int)halfwaydown.x;
	//	while (scanner < threshed.rows) {
	//		for (int i = -15; i < 15; i++)
	//		{
	//			if (threshed.at<uchar>(scanner, i + scanX) < (uchar)250)
	//				break;
	//			else if (i == 14)
	//				goto next;
	//		}
	//		scanner++;
	//	}
	//next:;
	//	separator.start.y = scanner;
	//	scanner = (int)halfwaydown.y;
	//	while (scanner > 0) {
	//		for (int i = -15; i < 15; i++)
	//		{
	//			if (threshed.at<uchar>(scanner, i + scanX) < (uchar)250)
	//				break;
	//			else if (i == 14)
	//				goto done;
	//		}
	//		scanner--;
	//	}
	//done:;
	//	separator.end.y = scanner;
		line(threshed, Point(separator.start.x, separator.start.y), Point(separator.end.x, separator.end.y), Scalar(0, 0, 255), 12, LINE_AA);
	}

#if 0
	{
		cout << "correcting rotation with the seperator as the only reference angle" << endl;

		curve line = rawSeperator;

		if (!line.start.y > line.end.y)
			line = flipLine(line);

		float angle = atan((line.end.x - line.start.x) / (line.end.y - line.start.y));

		if (angle > PI / 4)
			angle -= PI / 2;
		else if (angle < -PI / 4)
			angle += PI / 2;

		cout << "correcting angle of " << angle * DEGREES << "degrees" << endl;

		cv::Mat rot_mat = cv::getRotationMatrix2D(cv::Point2f(imw / 2, imh / 2), -angle * DEGREES, 1);
		cv::Mat rotated;

		cv::warpAffine(threshed, rotated, rot_mat, threshed.size(), cv::INTER_CUBIC, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
		rotated.copyTo(threshed);

		cv::warpAffine(img, rotated, rot_mat, img.size(), cv::INTER_CUBIC, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
		rotated.copyTo(img);

		cv::warpAffine(edges, rotated, rot_mat, threshed.size(), cv::INTER_CUBIC, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
		rotated.copyTo(edges);
		imshow("corrected rotation", img);
	}
#endif

	//imshow("Detected separator", threshed);

	cv::Rect rcrop, lcrop, tcrop;

	//now assuming seperator is correct
	rcrop = cv::Rect(
		separator.start.x + 20,
		glm::min(separator.start.y, separator.end.y),
		imw - (separator.start.x + 20) - 1,
		abs(separator.start.y - separator.end.y) - 1
	);

	lcrop = cv::Rect(
		10,
		glm::min(separator.start.y - 20, separator.end.y - 20),
		separator.start.x - 40,
		abs(separator.start.y - separator.end.y) + 30
	);
	tcrop = Rect(
		10,
		10,
		imw - 10,
		glm::min(separator.start.y, separator.end.y) - 20
	);

	//imshow("Rcrop", img(rcrop));
	//imshow("Lcrop", img(lcrop));
	//imshow("Tcrop", img(tcrop));

#if 0

	//find name and date lines
	Mat nameDate;
	curve inameLine = cLine(vec2(0), vec2(0)), idateLine = cLine(vec2(0), vec2(0));
	{
		cout << "finding name and date lines" << endl;

		vector<cv::Vec4i> linesP;
		threshed(tcrop).copyTo(nameDate);
		Canny(nameDate, edges, 50, 200, 3);

		//for the name and date
		linesP.clear();
		HoughLinesP(edges, linesP, 1, CV_PI / 180, 50, 50, 10);
		vector<curve> allLines;
		for (size_t i = 0; i < linesP.size(); i++)
		{
			Vec4i l = linesP[i];
			//line(nameDate, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(0, 0, 255), 3, LINE_AA);
			allLines.push_back(cLine(vec2(l[0], l[1]), vec2(l[2], l[3])));
		}
		//imshow("name date", nameDate);

		for (int i = 0; i < allLines.size(); i++)
			if (abs(allLines[i].start.x - allLines[i].end.x) > abs(inameLine.start.x - inameLine.end.x))
				inameLine = allLines[i]; //line(nameDate, Point(allLines[i].start.x, allLines[i].start.y), Point(allLines[i].end.x, allLines[i].end.y), Scalar(255, 0, 0), 3, LINE_AA);

		for (int i = 0; i < allLines.size(); i++)
			if (
				allLines[i].start.x > inameLine.start.x &&
				allLines[i].end.x > inameLine.start.x &&
				allLines[i].start.x > inameLine.end.x &&
				allLines[i].end.x > inameLine.end.x &&

				abs(allLines[i].start.x - allLines[i].end.x) > abs(idateLine.start.x - idateLine.end.x))
				idateLine = allLines[i];
	}

	line(nameDate, Point(inameLine.start.x, inameLine.start.y), Point(inameLine.end.x, inameLine.end.y), Scalar(0, 0, 255), 3, LINE_AA);
	line(nameDate, Point(idateLine.start.x, idateLine.start.y), Point(idateLine.end.x, idateLine.end.y), Scalar(0, 0, 255), 3, LINE_AA);
	//imshow("name date", nameDate);

	*nameLine = cLine(vec2(glm::min(inameLine.start.x, inameLine.end.x), inameLine.start.y), vec2(glm::max(inameLine.start.x, inameLine.end.x), inameLine.start.y)) + 10;
	*dateLine = cLine(vec2(glm::min(idateLine.start.x, idateLine.end.x), idateLine.start.y), vec2(glm::max(idateLine.start.x, idateLine.end.x), idateLine.start.y)) + 10;

#endif

	vector<string> searchList;
	vector<wordLocation> foundWords;
	int dimx, dimy;


	bool okay = true;

	cout << endl << "starting up tesseract..." << endl;

	//settings
	GenericVector<STRING>  pars_vec;
	pars_vec.push_back("load_system_dawg");
	pars_vec.push_back("load_freq_dawg");
	pars_vec.push_back("load_punc_dawg");
	pars_vec.push_back("load_number_dawg");
	pars_vec.push_back("load_unambig_dawg");
	pars_vec.push_back("load_bigram_dawg");

	GenericVector<STRING> pars_values;
	pars_values.push_back("F");
	pars_values.push_back("F");
	pars_values.push_back("F");
	pars_values.push_back("F");
	pars_values.push_back("F");
	pars_values.push_back("F");

	ocr = new tesseract::TessBaseAPI();
	okay &= 0 == ocr->Init(NULL, "eng", tesseract::OEM_TESSERACT_ONLY, NULL, 0, &pars_vec, &pars_values, false);

	okay &= ocr->SetVariable("tessedit_char_blacklist", "abcdefghijklmnopqrstuvwxyz';!/.,[]{}@#$%^&*()");
	//	okay &= ocr->SetVariable("classify_bln_numeric_mode", "0");

	if (!okay) {
		cout << "tesseract failed to initialize with settings" << endl;
		return false;
	}

	ocr->SetPageSegMode(tesseract::PSM_SINGLE_BLOCK);


	cv::Mat searchListImg;
	img(rcrop).copyTo(searchListImg);

	cv::Mat searchImg;
	img(lcrop).copyTo(searchImg);

	//Detect searchlist words Note: Sometimes there empty lines are detected

	cout << "looking for some letters..." << endl << endl;

	ocr->SetImage(searchListImg.data, searchListImg.cols, searchListImg.rows, 3, searchListImg.step);
	auto text = string(ocr->GetUTF8Text());
	std::cout << "Detected Text: " << std::endl << text << std::endl;

	int length = text.length();
	int wordEnd = 0;
	for (int i = 0; i < length; i++)
	{
		if (text[i] == '\n')
		{
			string word = text.substr(wordEnd, i - wordEnd);
			if (word.length() > 1)
				searchList.push_back(word);
			wordEnd = i + 1;
		}
	}
	if (searchList.size() < 1)
	{
		cout << "No words in search list detected" << endl;
		return false;
	}

	std::cout << "Searh list detected with " << searchList.size() << " entries" << endl << "reading word search text..." << std::endl << std::endl;

	ocr->SetImage(searchImg.data, searchImg.cols, searchImg.rows, 3, searchImg.step);
	text = string(ocr->GetUTF8Text());
	std::cout << "Detected Text: " << std::endl << text << std::endl;

	//remove empty lines
	length = text.length();
	string filteredText;
	for (int i = 0; i < length; i++)
	{
		if (text[i] == '\n' && text[i + 1] == '\n')
			i++;
		filteredText += text[i];
	}
	text = filteredText;
	length = text.length();

	//find rows and colums
	dimy = 0;
	dimx = -1;
	for (int i = 0; i < length; i++)
	{
		if (text[i] == '\n') {
			dimy++; // count rows
			if (dimx == -1)
				dimx = i; //figure out first row length
			else if ((i - dimy + 1) % dimx) //detects failure when row lengths are different
			{
				cout << "Artifacts detected in letter grid" << endl;
				return false;
			}
		}
	}
	std::cout << "Rows: " << dimx << " Colums: " << dimy << endl << endl;

	Grid<char> lets = Grid<char>(dimx, dimy);

	for (int i = 0; i < dimy; i++)
		for (int j = 0; j < dimx; j++)
			lets.Data[dimx * i + j] = text[i * dimx + i + j];

	//find words single core
	for (int c = 0; c < searchList.size(); c++)
	{
		string word = searchList[c];
		int wordLen = word.length();
		char firstLetter = word[0];
		if (word == "MAY") {
			int temp = 93;
		}
		for (int i = 0; i < dimy; i++) {
			for (int j = 0; j < dimx; j++) {
				if (lets[i][j] != firstLetter)
					continue;

				//right
				if (j + wordLen <= dimx) {
					for (int k = 1; k < wordLen; k++)
					{
						if (k == wordLen - 1 && lets[i][j + k] == word[k]) {
							foundWords.push_back(wordLocation{ j, i, 0 });
							goto nextletter;
						}
						if (lets[i][j + k] != word[k])
							break;
					}
				}
				//left
				if (j - wordLen >= -1) {
					for (int k = 1; k < wordLen; k++)
					{
						if (k == wordLen - 1 && lets[i][j - k] == word[k]) {
							foundWords.push_back(wordLocation{ j, i, 1 });
							goto nextletter;
						}
						if (lets[i][j - k] != word[k])
							break;
					}
				}
				//up
				if (i - wordLen >= -1) {
					for (int k = 1; k < wordLen; k++)
					{
						if (k == wordLen - 1 && lets[i - k][j] == word[k]) {
							foundWords.push_back(wordLocation{ j, i, 2 });
							goto nextletter;
						}
						if (lets[i - k][j] != word[k])
							break;
					}
				}
				//down
				if (i + wordLen <= dimy) {
					for (int k = 1; k < wordLen; k++)
					{
						if (k == wordLen - 1 && lets[i + k][j] == word[k]) {
							foundWords.push_back(wordLocation{ j, i, 3 });
							goto nextletter;
						}
						if (lets[i + k][j] != word[k])
							break;
					}
				}
				//top right
				if (i - wordLen >= -1 && j + wordLen <= dimx) {
					for (int k = 1; k < wordLen; k++)
					{
						if (k == wordLen - 1 && lets[i - k][j + k] == word[k]) {
							foundWords.push_back(wordLocation{ j, i, 4 });
							goto nextletter;
						}
						if (lets[i - k][j + k] != word[k])
							break;
					}
				}
				//bottom right
				if (i + wordLen <= dimy && j + wordLen <= dimx) {
					for (int k = 1; k < wordLen; k++)
					{
						if (k == wordLen - 1 && lets[i + k][j + k] == word[k]) {
							foundWords.push_back(wordLocation{ j, i, 5 });
							goto nextletter;
						}
						if (lets[i + k][j + k] != word[k])
							break;
					}
				}
				//top left
				if (i - wordLen >= -1 && j - wordLen >= -1) {
					for (int k = 1; k < wordLen; k++)
					{
						if (k == wordLen - 1 && lets[i - k][j - k] == word[k]) {
							foundWords.push_back(wordLocation{ j, i, 6 });
							goto nextletter;
						}
						if (lets[i - k][j - k] != word[k])
							break;
					}
				}
				//bottom left
				if (i + wordLen <= dimy && j - wordLen >= -1) {
					for (int k = 1; k < wordLen; k++)
					{
						if (k == wordLen - 1 && lets[i + k][j - k] == word[k]) {
							foundWords.push_back(wordLocation{ j, i, 7 });
							goto nextletter;
						}
						if (lets[i + k][j - k] != word[k])
							break;
					}
				}
			}
		}
		//no letter found
		foundWords.push_back(wordLocation{ 0, 0, -1 });
	nextletter:; // using goto label to break out of 3 nested loops at once
	}


	for (int i = 0; i < foundWords.size(); i++)
	{
		if (foundWords[i].directionType != -1)
			std::cout << "found word: " << searchList[i] << " at position " << foundWords[i].x << ", " << foundWords[i].y << " with a type of " << foundWords[i].directionType << std::endl;
		else
			std::cout << "FAILED to find word: " << searchList[i] << std::endl;
	}

	*words = foundWords;
	*retSerachlist = searchList;

	*searchCrop = getPixelBoundingBox(searchImg, 100) + vec4(lcrop.x, lcrop.y, 0, 0);

	*cols = dimx;
	*rows = dimy;
	*cellSize = vec2(0);

	cv::Mat gray;
	cvtColor(img, gray, cv::COLOR_BGR2GRAY);
	gray(cv::Rect(searchCrop->x, searchCrop->y, searchCrop->z, searchCrop->w)).copyTo(gray);
	bool lastScan = false;
	for (int i = (int)searchCrop->z - 1; i > 10; i--)
	{
		bool thisScan = false;
		for (int j = 0; j < searchCrop->w; j++)
		{
			if (gray.at<uchar>(j, i) < (uchar)100) {
				thisScan = true;
				break;
			}
		}
		if (lastScan && !thisScan) {
			cellSize->x = (float)(i) / (float)(dimx - 1.0f);
			break;
		}
		lastScan = thisScan;
	}
	lastScan = false;
	for (int i = (int)searchCrop->w - 1; i > 10; i--)
	{
		bool thisScan = false;
		for (int j = 0; j < searchCrop->z; j++)
		{
			if (gray.at<uchar>(i, j) < (uchar)100) {
				thisScan = true;
				break;
			}
		}
		if (lastScan && !thisScan) {
			cellSize->y = (float)(i) / (float)(dimy - 1.0f);
			break;
		}
		lastScan = thisScan;
	}
	return true;
}



