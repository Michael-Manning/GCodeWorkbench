#pragma once
#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
//#include <fileapi.h>
#include "stitching.h"

#ifdef __OPENCV_INSTALLED__



#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/calib3d.hpp"
#include "opencv2/highgui.hpp"

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

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif 
//#include <stb/stb_image.h> 



using namespace glm;
using namespace std;
using namespace cv;


//GLuint getWaveArt(const char * file, float lineDiv, float smoothing);

//everything in the file is meant for use C# but some may be used from a C++ entry point



//used to effectively construct native classes using functions with low level types
vector<stitchImg> internalSet;
GLFWwindow * internalContext;
stitcherSettings internalStitchingSettings;

static stitcherSettings loadStitcherSettings(cv::String filepath);
static void loadStitcherSettings(string  filepath, stitcherSettings settings);
static vector<unsigned char*> imageBuffer;

Mat internalMap1, internalMap2;
Mat intrinsicMatrixLoaded, distortionCoeffsLoaded;

extern "C" {

	//load matrix once for subsequent, individual use at any time
	__declspec(dllexport) void loadCalibrationMatrixInternal(const char * matrixLocation = "../../../data/Pi_camera_calibration_matrix.xml") {
		//load calibration settings
		FileStorage fs(matrixLocation, FileStorage::READ);
		fs["camera_matrix"] >> intrinsicMatrixLoaded;
		fs["distortion_coefficients"] >> distortionCoeffsLoaded;

		//create rectification map
		initUndistortRectifyMap(intrinsicMatrixLoaded, distortionCoeffsLoaded, Mat(), intrinsicMatrixLoaded, Size(2592, 1944), CV_16SC2, internalMap1, internalMap2);
	}

	//undistort single image from preloaded distortion map
	__declspec(dllexport) void undistortImageInternal(const char * InputFile, const char * outputFile) {
		Rect crop(245, 22, 2335, 1899);

		Mat orig = imread(string(InputFile), IMREAD_GRAYSCALE);
		Mat fixed, shrunk, mid;
		resize(orig, mid, Size(2592, 1944));
		remap(mid, fixed, internalMap1, internalMap2, INTER_LINEAR);
		resize(fixed(crop), shrunk, Size(648, 486));
		imwrite(string(outputFile), shrunk);

	}

	__declspec(dllexport) void undistortImageSet(const char * InputFolder, const char * outputFolder, int imageCount, const char * matrixLocation = "../../../data/Pi_camera_calibration_matrix.xml") {

		//load calibration settings
		FileStorage fs(matrixLocation, FileStorage::READ);
		Mat intrinsicMatrix, distortionCoeffs;
		fs["camera_matrix"] >> intrinsicMatrix;
		fs["distortion_coefficients"] >> distortionCoeffs;

		//create rectification map
		Mat map1, map2;
		initUndistortRectifyMap(intrinsicMatrix, distortionCoeffs, Mat(), intrinsicMatrix, Size(2592, 1944), CV_16SC2, map1, map2);

		Rect crop(245, 22, 2335, 1899);

		for (int i = 0; i < imageCount; i++)
		{
			Mat orig = imread(string(InputFolder) + "/pano" + to_string(i) + ".jpg", IMREAD_GRAYSCALE);
			Mat fixed, shrunk, mid;
			resize(orig, mid, Size(2592, 1944));
			remap(mid, fixed, map1, map2, INTER_LINEAR);
			resize(fixed(crop), shrunk, Size(648, 486));
			imwrite(outputFolder + string("/fixed_pano") + to_string(i) + ".jpg", shrunk);
		}
	}

	//x and y are position. w and h is for rendering scale
	__declspec(dllexport) void addToStitchingSet(const char * imagePath, float x, float y, float w, float h) {

		int comp, imW, imH;
		GLuint ID;

		stbi_set_flip_vertically_on_load(false);
		unsigned char* image = stbi_load(imagePath, &imW, &imH, &comp, STBI_rgb_alpha);
		if (image == NULL) {
			fprintf(stderr, "failed to load image\n");
			return;
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

		stitchImg im(ID, vec2(x, y), vec2(w, h));
		internalSet.push_back(im);
	}

	//x and y are position. w and h is for rendering scale
	__declspec(dllexport) void preLoadImage(const char * imagePath, float x, float y, float w, float h) {

		int comp, imW, imH;

		stbi_set_flip_vertically_on_load(false);
		unsigned char* image = stbi_load(imagePath, &imW, &imH, &comp, STBI_rgb_alpha);
		if (image == NULL) {
			fprintf(stderr, "failed to load image\n");
			return;
		}

		//create stitch image now without data. Texture needs to be created on main thread
		stitchImg im(0, vec2(x, y), vec2(w, h));
		internalSet.push_back(im);
		imageBuffer.push_back(image);
	}

	__declspec(dllexport) void initializePreloadedImages() {
		for (int i = 0; i < imageBuffer.size(); i++)
		{
			GLuint ID;
			unsigned char* image = imageBuffer[i];

			gl(GenTextures(1, &ID));
			gl(BindTexture(GL_TEXTURE_2D, ID));
			gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
			gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
			gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
			gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
			gl(TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 648, 486, 0, GL_RGBA, GL_UNSIGNED_BYTE, image));
			gl(BindTexture(GL_TEXTURE_2D, 0));

			internalSet[i].Id = ID;
			stbi_image_free(image);
		}
		imageBuffer.clear();
	}

	__declspec(dllexport) void clearStitchingSet() {
		internalSet.clear();
	}

	__declspec(dllexport) void initOpenGLContext() {

		//init opengl context
		int glfwInitResult = glfwInit();
		if (glfwInitResult != GLFW_TRUE)
		{
			fprintf(stderr, "glfwInit returned false\n");
			return;
		}

		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

		internalContext = glfwCreateWindow(480, 480, "", NULL, NULL);

		if (!internalContext)
		{
			fprintf(stderr, "failed to open glfw window. is opengl 3.2 supported?\n");
			return;
		}

		glfwPollEvents();
		glfwMakeContextCurrent(internalContext);

		int gl3wInitResult = gl3wInit();
		if (gl3wInitResult != 0)
		{
			fprintf(stderr, "gl3wInit returned error code %d\n", gl3wInitResult);
			return;
		}

		gl(Enable(GL_BLEND));
		gl(BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

		glfwSwapInterval(1);
	}

	__declspec(dllexport) void runStitcher(int canvasW, int canvasH, const char * saveTo = NULL) {
	//	//dataPath = "../../../data/";
	////	GLuint ID = simpleStitch(internalSet, canvasW, canvasH, internalStitchingSettings);

	//	glfwMakeContextCurrent(internalContext);

	//	cv::Mat out = cv::Mat(canvasW, canvasW, CV_8UC4);

	//	//use fast 4-byte alignment (default anyway) if possible
	//	gl(PixelStorei(GL_PACK_ALIGNMENT, (out.step & 3) ? 1 : 4));

	//	//set length of one complete row in destination data (doesn't need to equal out.cols)
	//	gl(PixelStorei(GL_PACK_ROW_LENGTH, out.step / out.elemSize()));

	//	gl(BindTexture(GL_TEXTURE_2D, ID));
	//	gl(GetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, out.data));
	//	cv::imshow("result", out);
	//	if (saveTo != NULL)
	//		cv::imwrite(String(saveTo), out);

	//	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//	vector<vector<cv::Point> > contours;
	//	vector<cv::Vec4i> hierarchy;
	//	cv::RNG rng(12345);

	//	Mat input;
	//	cv::cvtColor(out, input, CV_BGR2GRAY);

	//	//increase the dynamic range of the image to full
	//	double min, max;
	//	minMaxLoc(input, &min, &max);
	//	float dif = (255 / (max - min));
	//	Mat dynamic = input * dif - min;

	//	//blur the image
	//	Mat blurred;
	//	GaussianBlur(dynamic, blurred, Size(9, 9), 0);

	//	//binary threshold
	//	Mat threshed;
	//	threshold(blurred, threshed, 0, 255, THRESH_BINARY + THRESH_OTSU);
	//	//threshold(blurred, threshed, 100, 255, THRESH_BINARY);
	//	//adaptiveThreshold(blurred, threshed, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 951, 0);

	//	//smooth the edges
	//	GaussianBlur(threshed, blurred, Size(5, 5), 0);

	//	imshow("binary", blurred);

	//	findContours(blurred, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));

	//	// Draw contours
	//	Mat drawing = Mat::zeros(blurred.size(), CV_8UC3);
	//	for (int i = 0; i < contours.size(); i++)
	//	{
	//		if (contours[i].size() > 50) {
	//			Scalar color = Scalar(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255));
	//			drawContours(drawing, contours, i, color, 2, 8, hierarchy, 0, Point());
	//		}

	//	}
	//	imshow("contours", drawing);




	//	waitKey();
	}

	__declspec(dllexport) void loadStitcherSettingsInternal(const char * filepath) {
		internalStitchingSettings = loadStitcherSettings(String(filepath));
	};
}

void write(FileStorage& fs, const std::string&, const stitchImg& x)
{
	x.write(fs);
};

void write(FileStorage& fs, const std::string&, const stitcherSettings& x)
{
	x.write(fs);
};

static void read(const FileNode& node, stitcherSettings& x, const stitcherSettings& default_value = stitcherSettings())
{
	if (node.empty())
		x = default_value;
	else
		x.read(node);
};


static stitcherSettings loadStitcherSettings(String filepath) {
	FileStorage fs(filepath, cv::FileStorage::READ);
	stitcherSettings settings;
	fs["settings"] >> settings;
	return settings;
};

static void saveStitcherSettings(String filepath, stitcherSettings settings) {
	FileStorage fs(filepath, cv::FileStorage::WRITE);
	fs << "settings" << settings;
	fs.release();
};

#endif