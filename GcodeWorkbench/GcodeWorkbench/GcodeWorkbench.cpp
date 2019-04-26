#include"pch.h"

#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING

#include <stdio.h>
#include <tchar.h>
#include <math.h>
#include <assert.h> 

#include <stb/stb_image.h> //loads textures for opengl
#include <gl3w/gl3w.h> //opengl profile loader
#include <GL/GLU.h> //need for opengl
#include <GLFW/glfw3.h> //opengl context

#include <glm/glm.hpp> //math
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <agg/agg_curves.h> //used for optimized bezier line aproximation
#include <simpleIni/SimpleIni.h>

#include <iostream>
#include <filesystem>
#include <iomanip>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <fileapi.h>
#include <thread>
#include <ctime>

#include <imgui/imconfig.h> //user interface
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#define pen_up_cmd "M42 P60 S0"
#define pen_down_cmd "M42 P60 S1"

//#include "Gcode/bezier_tools.h" //custom math tools
#include "GcodeWorkbench.h"
#include "TCP_connection.h"
#include "GL_basics.h"
#include "Gcode/font_tools.h" //font spesific tools 

/*
If you don't have opencv at the `moment, this macro will
compile the document to not use the features that require OpenCV
*/
#if __has_include(<opencv2/opencv.hpp>)
#define __OPENCV_INSTALLED__
#endif

#define __TESSERACT_INSTALLED__

#ifdef __OPENCV_INSTALLED__
//#include <opencv2/ccalib/omnidir.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#endif //__OPENCV_INSTALLED__

//#ifdef __OPENCV_TEXT_INSTALLED__
#include "Gcode/vision.h"
#include "external_link.h"
#include "image_processing.h"
#include "big_canvas.h"
//#include"temphead.h"
//#endif // __OPENCV_TEXT_INSTALLED__





using namespace std;
using namespace glm;
using namespace GLBasics;

//All these macros effect how code is compiled in this document
#pragma region macros

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

#define asizei(a) (int)(sizeof(a)/sizeof(a[0]))
#define PI 3.14159265359
#pragma endregion

const vec4 backCol(glm::vec3(0.15f), 1); //back color of the window
const char* glsl_version = "#version 330 core";

int resWidth = 2000, resHeight = 1200; //initial and current window size
GLFWwindow* window;
float aspect; //current aspect ratio
float windowDiv;
ImFont* canvasFont;
float canvasZoomAmount = 1.5f;
float zoomSpeed = 0.02f;
char labelBuff[10]; //general use
typeWriter writer;
CSimpleIniA ini;

//UI functions
void showUI();
void LeftWindow();
void curveEditorWindow();
void bezierEditorWindow();
void textToolsWindow();
void textToolsCanvas(bool aproximate, const char* canID);
void vectorTraceWindow();
void traceSettingsWindow();
void letterRecognitionWindow();
void rectilinearWindow();
void imgStitchWindow();
void lineArtWindow();
void generateScriptWindow();
void ditheringWindow();
void spiralWindow();
void objWindow();
void cameraCalibrationWindow();

//other functions
void render();
void drawBezier4(bezier4 b4, ImDrawList* drawList, vec2 canPos, ImColor col = ImColor(vec4(1)));
void drawBezier3(bezier3 b3, ImDrawList* drawList, vec2 canPos, ImColor col = ImColor(vec4(1)));
void drawArc(detailedArc ar, ImDrawList* drawList, vec2 canPos);
void drawLine(curve Curve, ImDrawList* drawList, vec2 canPos, ImColor col = ImColor(vec4(1)), float width = 2);
void drawCircleChords(detailedArc ar, ImDrawList* drawList, vec2 canPos);
void saveScript(vector<Gcommand> commands, const char* file = "../data/contourScript.gcode");

//actual code starts here:



struct program_settings {

};
program_settings settings;



#ifdef __OPENCV_INSTALLED__
//converts a cv::Mat into a usable OpenGL Texture
void loadTexture(cv::Mat cvimage, GLuint* id, int w, int h) {
	gl(GenTextures(1, id));
	gl(BindTexture(GL_TEXTURE_2D, *id));
	gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
	gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
	gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	gl(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	if (cvimage.type() == 0) {
		gl(TexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, cvimage.ptr()));
	}
	else if (cvimage.channels() == 4) {
		gl(TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, cvimage.ptr()));
	}
	else {
		gl(TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_BGR, GL_UNSIGNED_BYTE, cvimage.ptr()));
	}
	gl(BindTexture(GL_TEXTURE_2D, 0));
}

//converts an openGL texture to and openCV Mat
cv::Mat loadTexture(GLuint id, int w, int h) {
	cv::Mat out = cv::Mat(h, w, CV_8UC4);

	//use fast 4-uint8_t alignment (default anyway) if possible
	gl(PixelStorei(GL_PACK_ALIGNMENT, (out.step & 3) ? 1 : 4));

	//set length of one complete row in destination data (doesn't need to equal out.cols)
	gl(PixelStorei(GL_PACK_ROW_LENGTH, out.step / out.elemSize()));

	gl(BindTexture(GL_TEXTURE_2D, id));
	gl(GetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, out.data));
	return out;
}

#endif 

void saveScript(vector<Gcommand> commands, const char* file) {
	ofstream scriptFile;
	scriptFile.open(file);
	for (int i = 0; i < commands.size(); i++)
		scriptFile << commands[i].cmd << "\n";
	
	scriptFile.close();
}


//used an image that can be used with opengl
struct imgAsset {
	vec2 size;
	const char* filePath;
	bool init = false;
	GLuint ID;
	imgAsset() {}
	imgAsset(const char* filepath, bool flip = false) {
		int w, h;
		loadTexture(filepath, &ID, &w, &h, flip);
		size = vec2(w, h);
		init = true;
	}

	//OpenCV Mats cannot be used an opngl texture, but they can be converted
#ifdef __OPENCV_INSTALLED__
	imgAsset(cv::Mat image) {
		size.x = image.cols;
		size.y = image.rows;
		loadTexture(image, &ID, size.x, size.y);
		init = true;
	}
#endif 

	void dispose() {
		//glDeleteTextures(1, &ID); //{
	//		int potentialGLError = glGetError(); 
	//			if (potentialGLError != GL_NO_ERROR) 
	//			{ 
	//				//fprintf(stderr, "OpenGL Error '%s' (%d) gl" __FILE__ ":%d\n", (const char*)gluErrorString(potentialGLError), potentialGLError, __LINE__); 
	//				//assert(potentialGLError == GL_NO_ERROR && "gl" #OPENGL_CALL); 
	//			} 
	//	}
	}
};

//imgui doesn't give have much for scroll input, so it's kept track of here 
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	canvasZoomAmount += yoffset * zoomSpeed;
	canvasZoomAmount = glm::clamp(canvasZoomAmount, 0.3f, 8.0f);
}

#include <commdlg.h>
OPENFILENAME ofn;       // common dialog box structure
//LPWSTR wideStr = L"Some message";
TCHAR szFile[1024];// = "testset";// TEXT("");       // buffer for file name
HWND hwnd;              // owner window
HANDLE hf;              // file handle

bool showWindowsOpenFileDialog(char* fileName) {
	// Initialize OPENFILENAME
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = szFile; //(LPWSTR)
	// Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
	// use the contents of szFile to initialize itself.
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = 1024;//sizeof(szFile);
	//	ofn.lpstrFilter = "All\0*.*\0Text\0*.TXT\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	// Display the Open dialog box. 
	bool result = GetOpenFileName(&ofn);
	if (result)
		wcstombs(fileName, szFile, wcslen(szFile) + 1);
	return result;
}

int main() {

	//init opengl context
	int glfwInitResult = glfwInit();
	if (glfwInitResult != GLFW_TRUE)
	{
		fprintf(stderr, "glfwInit returned false\n");
		return 1;
	}

	//glfwWindowHint(GLFW_DECORATED, borderd);
	//glfwWindowHint(GLFW_FLOATING, !borderd);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	window = glfwCreateWindow(resWidth, resHeight, "Capstone Workbench", NULL, NULL);

	if (!window)
	{
		fprintf(stderr, "failed to open glfw window. is opengl 3.2 supported?\n");
		return 1;
	}

	glfwSetScrollCallback(window, scroll_callback);
	glfwPollEvents();
	glfwMakeContextCurrent(window);


	int gl3wInitResult = gl3wInit();
	if (gl3wInitResult != 0)
	{
		fprintf(stderr, "gl3wInit returned error code %d\n", gl3wInitResult);
		return 1;
	}

	gl(Enable(GL_BLEND));
	gl(BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

	//vsync
	glfwSwapInterval(1);

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	canvasFont = io.FontDefault;

	ImGui_ImplGlfw_InitForOpenGL(window, true);

	ImGui_ImplOpenGL3_Init(glsl_version);
	ImGui::StyleColorsDark();

	writer.initFont();
	writer.setCursor(20, 300);

	ini.SetUnicode();
	SI_Error er = ini.LoadFile("myfile.ini");
	const char* pVal = ini.GetValue("section", "key", "default");
	ini.SetValue("section", "key", "newvalue");

	while (!glfwWindowShouldClose(window))
		render();
	return 0;
}

//opengl context and window managing busywork
bool firstFrame = true;
double currentTime = 0;
double currentFrameTime() {
	return glfwGetTime() - currentTime;
}

void render() {
	glfwMakeContextCurrent(window);
	currentTime = glfwGetTime();

	glfwPollEvents();
	if (glfwGetKey(window, GLFW_KEY_ESCAPE))
		glfwSetWindowShouldClose(window, true);

	glfwGetWindowSize(window, &resWidth, &resHeight);
	vec2 resolutionV2f = vec2((float)resWidth, (float)resHeight);
	aspect = resolutionV2f.y / resolutionV2f.x;


	gl(Viewport(0, 0, resWidth, resHeight));
	gl(ClearColor(backCol.r, backCol.g, backCol.b, 1.0f));
	gl(Clear(GL_COLOR_BUFFER_BIT));



	if (firstFrame) {
#if 0
		vector<stitchImg> imgs;
		for (int i = 0; i < 1; i++)
		{
			stitchImg im;
			GLuint id;
			int w, h;
			loadTexture("C:/Users/Micha/Documents/cool images/test_cat.png", &id, &w, &h);
			im.Id = id;
			im.position = vec2(i * 50);
			im.scale = vec2(1000);
			im.angle = 0;
			imgs.push_back(im);
		}
		GLuint testIm = simpleStitch(imgs, 1000, 1000);

		cv::Mat out = loadTexture(testIm, 1000, 1000);


		cv::imshow("stitch", out);
		cv::waitKey();
		cin.get();
		gl(BindFramebuffer(GL_FRAMEBUFFER, 0));
#endif
	}




	gl(UseProgram(0));
	gl(BindVertexArray(0));

	windowDiv = 300;//resWidth / 5;
	showUI(); //workhorse that runs the UI (and the whole program)

	glfwSwapBuffers(window);
	firstFrame = false;
}



//imgui things
#define white  (ImColor)IM_COL32(255, 255, 255, 255)
#define red  (ImColor)IM_COL32(255, 0, 0, 255)
#define black  (ImColor)IM_COL32(0, 0, 0, 255)
#define orange  (ImColor)IM_COL32(239, 136, 95, 255)
#define blue  (ImColor)IM_COL32(25, 104, 232, 255)
#define yellow  (ImColor)IM_COL32(255, 255, 0, 255)
#define transparent  (ImColor)IM_COL32(255, 255, 255, 100)
#define invisible  (ImColor)IM_COL32(0, 0, 0, 0)

//window opened flags
bool editorTab = true;
bool textTab = true;
bool curveTab = false;
bool traceTab = false;
bool ditherTab = false;
bool showoTraceSettings = false;
bool showGenerateScriptWindow = false;
bool showGcodePreviewWindow = false;
bool previewInit = false;
bool showCameraCalibrationWindow = false;

vec2 prevMousePos;

ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

using namespace ImGui;

void showUI() {

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	//if (BeginMainMenuBar()) {
	//	if (BeginMenu("File"))
	//	{
	//		//ShowExampleMenuFile();
	//		ImGui::EndMenu();
	//	}
	//}

	LeftWindow();

	ImGuiWindowFlags window_flags = 0;
	window_flags |= ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoResize;
	window_flags |= ImGuiWindowFlags_NoCollapse;
	window_flags |= ImGuiWindowFlags_NoNav;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	window_flags |= ImGuiWindowFlags_NoTitleBar;

	Begin("empty", NULL, window_flags);

	SetWindowSize(vec2(resWidth - windowDiv, resHeight));
	SetWindowPos(vec2(windowDiv, 0));

	ImGuiStyle& style = ImGui::GetStyle();
	//style.WindowPadding = vec2(25);
	style.WindowRounding = 0;

	ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
	if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
	{
		if (ImGui::BeginTabItem("Bezier Editor"))
		{
			bezierEditorWindow();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Text Tools"))
		{
			textToolsWindow();
			ImGui::EndTabItem();
		}
		/*	if (ImGui::BeginTabItem("Curve Fitting"))
			{
				curveEditorWindow();
				ImGui::EndTabItem();
			}*/
		if (ImGui::BeginTabItem("Rectilinear Infill"))
		{
			rectilinearWindow();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Vector Tracing"))
		{
			vectorTraceWindow();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Line Art"))
		{
			lineArtWindow();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Dithering"))
		{
			ditheringWindow();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Spiral Art"))
		{
			spiralWindow();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("3D Model"))
		{
			objWindow();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Image Stitching"))
		{
			imgStitchWindow();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Text Recognition"))
		{
			letterRecognitionWindow();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	End();
	if (showGenerateScriptWindow)
		generateScriptWindow();
	else
		previewInit = false;
	// Rendering
	ImGui::Render();
	int display_w, display_h;
	glfwMakeContextCurrent(window);
	glfwGetFramebufferSize(window, &display_w, &display_h);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


static char inputText[32] = "";
static char CmdText[64] = "";
static char previousinputText[32];
static char eeetText[32];
vector<bezierLetter> _letters;
static char Gbuf[60000] = "";
static char ipBuff[16] = "192.168.1.75";//"192.168.1.82"; 5;
static char portBuff[16] = "22000";
int selectedPort = 22000;
bool filledInText = false;
bool liveTyping = false;

//char spindleOnCmd[10] = "M4";
//char spindleOffCmd[10] = "M3";
int feedRate = 4500;

//line settings
float custLimit = 0;
float aproxScale = 1.0f;
float angleTol = 15;

//arc settings
bool showCircleChords = true;
bool showFullArcCircle = false;
bool showArc = true;
bool showArcCenters = true;
bool aproxArcs = true;
bool showCorrection = true;
float amin = 0;
float amax = 0;
int arcNum = 1;

int gTextIndex = 0;
void appendGText(const char* text) {
	int ch = 0;
	while (text[ch]) {
		Gbuf[gTextIndex] = text[ch];
		gTextIndex++;
		ch++;
	}
	Gbuf[gTextIndex] = '\n';
	gTextIndex++;
}

float drawThickness = 6;

bool aproxLines = false;

bool recalcLines = true;
bool recalcArcs = true;

//preview textbox input
#if 0
void textCallback(void*, bool val) {
	//only update with setText if there was a change
	if (!strcmp(previousinputText, inputText))
		return;

	strcpy(previousinputText, inputText);

	writer.clear();
	writer.setCursor(20, 20);
	int ch = 0;
	while (inputText[ch]) {
		writer.nextLetter(inputText[ch]);
		ch++;
	}
}
#else
void textCallback(void*, bool val) {
	//only update with setText if there was a change
	if (!strcmp(previousinputText, inputText))
		return;

	strcpy(previousinputText, inputText);

	//writer.clear();

	int len = strlen(inputText);
	if (len == 0) {
		writer.setCursor(20, 20);
		return;
	}
	writer.nextLetter(inputText[len - 1]);

	if (liveTyping) {
		vector<Gcommand> letterBuff = writer.getLetterGcode(len - 1, feedRate, 175, 1, filledInText);
		for (int j = 0; j < letterBuff.size(); j++)
			sendCommand(letterBuff[j]);
	}

}
#endif

char scriptFilepath[1024] = "../data/contourScript.gcode";
int feedSetting = 3000;
int movespeedSetting = 5000;
int dwellLengthSetting = 150;
float scriptSaveProgress = 0;
vector<vector<curve>>* scriptGenTarget;
bool scriptSaving = false;
vector<Gcommand> scriptWindowCommands;
thread scriptGenThread;
bigCanvas prevCan;
GLuint previewID;

void scriptGenThreadFunc() {
	scriptWindowCommands = createScript(*scriptGenTarget, feedSetting, movespeedSetting, dwellLengthSetting, &scriptSaveProgress);
}
void generateScriptWindow() {
	ImGuiWindowFlags window_flags = 0;
	window_flags |= ImGuiWindowFlags_NoCollapse;
	window_flags |= ImGuiWindowFlags_NoNav;
	window_flags |= ImGuiWindowFlags_NoDocking;
	Begin("Generate G-Code Script", &showGenerateScriptWindow, window_flags);
	ImGui::SetWindowFocus();

	InputText("Script save location", scriptFilepath, IM_ARRAYSIZE(scriptFilepath));
	InputInt("Feed Rate", &feedSetting);
	InputInt("None-draw travel speed", &movespeedSetting);
	InputInt("Dwell time", &dwellLengthSetting);
	ProgressBar(scriptSaveProgress);

	showGcodePreviewWindow |= Button("Preview");

	if (showGcodePreviewWindow) {
		ImGuiWindowFlags window_flags = 0;
		window_flags |= ImGuiWindowFlags_NoCollapse;
		window_flags |= ImGuiWindowFlags_NoNav;
		window_flags |= ImGuiWindowFlags_NoDocking;
		Begin("G-code Preview", &showGcodePreviewWindow, window_flags);
		ImGui::SetWindowFocus();

		if (!previewInit) {
			prevCan = bigCanvas(vec2(400));
			auto ar = *scriptGenTarget;

			for (int i = 0; i < ar.size(); i++)
			{
				for (int j = 0; j < ar[i].size(); j++)
				{
					prevCan.addToBuffer(ar[i][j]);
				}
			}

		}
		previewID = prevCan.render();
		Image((ImTextureID)previewID, GetContentRegionAvail());
		previewInit = true;
		End();
	}
	if (Button("Generate and save") && !scriptSaving) {
		scriptSaving = true;
		scriptWindowCommands.clear();
		scriptGenThread = thread(scriptGenThreadFunc);
	}
	if (scriptSaving) {
		if (scriptSaveProgress >= 1)
		{
			scriptGenThread.join();
			saveScript(scriptWindowCommands, scriptFilepath);
			scriptSaving = false;
			showGenerateScriptWindow = false;
			previewInit = false;
			scriptGenTarget = NULL;
			scriptSaveProgress = 0;
		}
	}
	End();
}

void LeftWindow() {
	ImGuiWindowFlags window_flags = 0;
	window_flags |= ImGuiWindowFlags_NoTitleBar;
	window_flags |= ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoResize;
	window_flags |= ImGuiWindowFlags_NoCollapse;
	window_flags |= ImGuiWindowFlags_NoNav;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	window_flags |= ImGuiWindowFlags_NoDocking;

	Begin("nothing", NULL, window_flags);

	SetWindowSize(vec2(windowDiv, resHeight));
	SetWindowPos(vec2(0));

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding = vec2(25);
	style.WindowRounding = 0;

	//int length = 0;
	//for (int i = 0; i < _letters.size(); i++)
	//	length += _letters[i].count;

	//if (firstFrame)
	//	setText(inputText);

	PushItemWidth(100);
	InputText("IP", ipBuff, IM_ARRAYSIZE(ipBuff), ImGuiInputTextFlags_CharsDecimal);
	SameLine();
	if (Button("Connect")) {
		connectTCP(ipBuff, std::stoi(portBuff));
	}
	SameLine();
	if (Button("Pen Up")) {
		sendCommand(Gcommand(pen_up_cmd));
	}

	//if (Button("Download img")) 
		//	downloadImg();


	InputText("Send Command", CmdText, IM_ARRAYSIZE(CmdText));
	SameLine();

	if (Button("Send")) {
		sendCommand(CmdText);
	}



	if (Button("Stream"))
		streamFile("../data/contourScript.gcode");
	SameLine();
	if (Button("Pause"))
		pauseStream();
	SameLine();
	if (Button("Resume"))
		resumeStream();
	ProgressBar(getStreamProgress());
	ImGui::InputTextMultiline("", Gbuf, IM_ARRAYSIZE(Gbuf), vec2(400, 800));


	End();
}

//move the tool head and block until position is reached (or 10 seconds passes in case of com error)
void moveAndWait(vec2 position) {
	char mbuf[64];
	sprintf(mbuf, "G1 X%.2f Y%.2f F5000", position.x, position.y);
	auto timer = glfwGetTime();
	sendCommand(Gcommand(mbuf));
	while (getPosition() != position && glfwGetTime() - timer < 7)
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void moveNow(vec2 position, float speed = 4000) {
	char mbuf[64];
	sprintf(mbuf, "G1 X%.2f Y%.2f F%.2f", position.x, position.y, speed);
	sendCommand(Gcommand(mbuf));
}

void penUpNow() {
	sendCommand(pen_up_cmd);
}
void penDownNow() {
	sendCommand(pen_down_cmd);
}

vector<curve> makeCirc(float x, float y, float rad) {
	vector<curve> circ;
	vec2 center = vec2(x, y);
	//vec2 seam = center + vec2(cos(0), (sin(0) * rad));
	//circ.push_back(cur);
	//result[1] = G2(seam, center - seam, speed);
	curve circle;
	circle.start = center + rad;
	circle.end = center + rad;
	circle.center = center;
	circle.type = 2;
	circ.push_back(circle);
	return circ;
}
vector<curve> makeCirc(vec2 center, float rad) {
	return makeCirc(center.x, center.y, rad);
}

bool showQuadratic = false;
bool showTrace = false;
float traceDistance = 0;

//bezier editor window
vec2 bpoints[] = { vec2(100, 400),
vec2(300, 200),
vec2(400, 600),
vec2(600, 400) };
static vector<vec2> linePoints;
vec2* bDragPoint;
bool bezierDrag = false;

void bezierEditorWindow() {
	Columns(3);

	showTrace = SliderFloat("Trace", &traceDistance, 0, 1, "%.3f%") || (showTrace && IsMouseDown(0));
	recalcLines |= Checkbox("Quadratic", &showQuadratic);


	//SameLine();
	//Checkbox("Aproximate Lines", &aproxLines);
	NextColumn();

	//line controlls
	Checkbox("Aproximate Lines", &aproxLines);
	recalcLines |= SliderFloat("Cusp Limit", &custLimit, 0, 90, "%.3f deg");
	recalcLines |= SliderFloat("Aproximation Scale", &aproxScale, 0.1f, 5);
	recalcLines |= SliderFloat("Angle Tolerance", &angleTol, 0, 90, "%.3f deg");
	NextColumn();



	Checkbox("Aproximate Arcs", &aproxArcs);
	Checkbox("Show Full Arc Circles", &showFullArcCircle);
	SameLine();
	Checkbox("Show Circle Chords", &showCircleChords);
	showArcCenters = showCircleChords;
	Checkbox("Show Arc", &showArc);


	//Checkbox("Show Arc Center", &showArcCenters);
	//Checkbox("Show Correction", &showCorrection);
	//	Columns(1);
	SliderInt("Number of arcs", &arcNum, 1, 10);
	//	Separator();

	//SliderFloat("min off", &amin, -PI * 2, PI * 2);
	//SliderFloat("max off", &amax, -PI * 2, PI * 2);

	SliderFloat("Line/Curve Width", &drawThickness, 4, 50);
	Columns(1);


	ImDrawList* drawList = ImGui::GetWindowDrawList();
	vec2 canPos = ImGui::GetCursorScreenPos();

	ImVec2 canvas_size = ImGui::GetContentRegionAvail();
	if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
	if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;
	drawList->AddRectFilledMultiColor(canPos, ImVec2(canPos.x + canvas_size.x, canPos.y + canvas_size.y), IM_COL32(50, 50, 50, 255), IM_COL32(50, 50, 60, 255), IM_COL32(60, 60, 70, 255), IM_COL32(50, 50, 60, 255));
	drawList->AddRect(canPos, ImVec2(canPos.x + canvas_size.x, canPos.y + canvas_size.y), white);

	ImGui::InvisibleButton("canvas", canvas_size);
	vec2 mpos = ImVec2(ImGui::GetIO().MousePos.x - canPos.x, ImGui::GetIO().MousePos.y - canPos.y);

	float circRad = 15;
	int hovIndex = -1;

	//mouse input
	if (ImGui::IsItemHovered()) {
		hovIndex = -1;
		for (int i = 0; i < 4; i++)
		{
			if (showQuadratic && i == 1)
				continue;
			if (distance((bpoints[i]), mpos) < circRad) {
				hovIndex = i;
				break;
			}
		}
		if (ImGui::IsMouseClicked(0) && hovIndex >= 0)
		{
			bDragPoint = bpoints + hovIndex;
			recalcLines = true;
			bezierDrag = true;
		}
		if (!IsMouseDown(0))
			bezierDrag = false;

		if (bezierDrag) {
			*bDragPoint = mpos;
			recalcLines = true;
		}

	}

	//store points as bezier types for conveniance
	bezier3 b3{ bpoints[0], bpoints[2], bpoints[3] };
	bezier4 b4{ bpoints[0], bpoints[1], bpoints[2], bpoints[3] };

	//compute line aproximation 
	if (recalcLines && aproxLines) {
		using namespace agg;

		if (showQuadratic) {
			curve3 curve(
				b3.p0.x, b3.p0.y,
				b3.p1.x, b3.p1.y,
				b3.p2.x, b3.p2.y);

			curve.angle_tolerance(angleTol);
			curve.approximation_scale(aproxScale);
			curve.cusp_limit(custLimit);
			curve.init(
				b3.p0.x, b3.p0.y,
				b3.p1.x, b3.p1.y,
				b3.p2.x, b3.p2.y);

			linePoints.clear();
			double x, y;
			while (curve.vertex(&x, &y))
				linePoints.push_back(vec2(x, y));
		}
		else {
			curve4 curve(
				b4.v[0].x, b4.v[0].y,
				b4.v[1].x, b4.v[1].y,
				b4.v[2].x, b4.v[2].y,
				b4.v[3].x, b4.v[3].y);

			curve.angle_tolerance(angleTol);
			curve.approximation_scale(aproxScale);
			curve.cusp_limit(custLimit);

			curve.init(
				b4.v[0].x, b4.v[0].y,
				b4.v[1].x, b4.v[1].y,
				b4.v[2].x, b4.v[2].y,
				b4.v[3].x, b4.v[3].y);

			linePoints.clear();
			double x, y;
			while (curve.vertex(&x, &y))
				linePoints.push_back(vec2(x, y));
		}
		recalcLines = false;
	}

	drawList->PushClipRect(canPos, ImVec2(canPos.x + canvas_size.x, canPos.y + canvas_size.y), true);

	//draw line aproximation 
	if (aproxLines) {
		int pLength = linePoints.size();
		for (int i = 0; i < pLength - 1; i++)
		{
			drawList->AddLine(linePoints[i] + canPos, linePoints[i + 1] + canPos, white, drawThickness);
			drawList->AddLine(linePoints[i] + canPos, linePoints[i + 1] + canPos, black, 2);
			drawList->AddCircleFilled(linePoints[i] + canPos, 4, black);
		}
	}
	else {
		//draw bezier (quadratic converted to cubix)
		if (showQuadratic)
			drawBezier3(b3, drawList, canPos);
		//draw bezier (cubic)
		else
			drawBezier4(b4, drawList, canPos);

	}

	//draw traceing
	if (showTrace) {
		if (showQuadratic) {
			bezier3 b{ bpoints[0], bpoints[2] ,bpoints[3] };
			vec2 tPoint = getBezierPoint(traceDistance, b) + canPos;
			drawList->AddCircle(tPoint, 12, orange, 12, 2);

			vec2 A = getLinearPoint(traceDistance, bpoints[0], bpoints[2]) + canPos;
			drawList->AddCircle(A, 8, orange);

			vec2 B = getLinearPoint(traceDistance, bpoints[2], bpoints[3]) + canPos;
			drawList->AddCircle(B, 8, orange);

			drawList->AddLine(A, B, orange);
		}
		else {
			bezier4 b{ bpoints[0], bpoints[1] ,bpoints[2], bpoints[3] };
			vec2 tPoint = getBezierPoint(traceDistance, b) + canPos;
			drawList->AddCircle(tPoint, 12, orange, 12, 2);

			vec2 A = getLinearPoint(traceDistance, bpoints[0], bpoints[1]) + canPos;
			drawList->AddCircle(A, 8, orange);

			vec2 B = getLinearPoint(traceDistance, bpoints[1], bpoints[2]) + canPos;
			drawList->AddCircle(B, 8, orange);

			vec2 C = getLinearPoint(traceDistance, bpoints[2], bpoints[3]) + canPos;
			drawList->AddCircle(C, 8, orange);

			vec2 D = getLinearPoint(traceDistance, A, B);
			drawList->AddCircle(D, 8, orange);

			vec2 E = getLinearPoint(traceDistance, B, C);
			drawList->AddCircle(E, 8, orange);

			drawList->AddLine(A, B, orange);
			drawList->AddLine(B, C, orange);
			drawList->AddLine(D, E, orange);
		}
	}

	//draw arc stuff
	if (aproxArcs && showQuadratic) {

		detailedArc* arcs = new detailedArc[arcNum];

		for (float i = 0; i < arcNum; i++)
		{
			float t0 = (1.0f / arcNum) * i;
			float t1 = t0 + (1.0f / arcNum);
			detailedArc ar = getArcApproximateDetailed(b3, t0, t1, true);
			arcs[(int)i] = ar;
		}

		if (showFullArcCircle)
			for (int i = 0; i < arcNum; i++)
				drawList->AddCircle(arcs[i].intersectAB + canPos, arcs[i].radius, blue, 128, 3);
		for (int i = 0; i < arcNum; i++)
			drawArc(arcs[i], drawList, canPos);
	}



	//draw control points
	for (int i = 0; i < 4; i++)
	{
		if (showQuadratic && i == 1)
			continue;
		drawList->AddCircle(bpoints[i] + canPos, circRad, i == hovIndex ? red : white, 12, 2);
		if (i < 3) {
			if (!showQuadratic)
				drawList->AddLine(bpoints[i] + canPos, bpoints[i + 1] + canPos, IM_COL32(200, 200, 200, 255));
			else {
				if (i == 0)
					drawList->AddLine(bpoints[i] + canPos, bpoints[i + 2] + canPos, IM_COL32(200, 200, 200, 255));
				else
					drawList->AddLine(bpoints[i] + canPos, bpoints[i + 1] + canPos, IM_COL32(200, 200, 200, 255));
			}
		}
	}
	drawList->PopClipRect();
}


//text editor window
vec2 CanvasPan = vec2(0, 100);
vec2 textEditorSize;

void textToolsWindow() {
	InputText("Display Text", inputText, IM_ARRAYSIZE(inputText), ImGuiInputTextFlags_CallbackAlways, (ImGuiTextEditCallback)textCallback);
	SameLine();
	InputInt("Feedrate", &feedRate);
	SameLine();
	if (Button("Send")) {
		sendCommand(CmdText);
	}
	if (Button("Reset cursor")) {
		writer.setCursor(vec2(20, 300));
	}
	if (Button("New line")) {
		writer.setCursor(vec2(20, 300 - writer.letterHeight * 0.9f));
	}
	Checkbox("Filled in", &filledInText);
	Checkbox("Live Typing", &liveTyping);

	textEditorSize = GetContentRegionAvail();
	textEditorSize.y /= 2;
	if (textEditorSize.x < 50.0f) textEditorSize.x = 50.0f;
	if (textEditorSize.y < 50.0f) textEditorSize.y = 50.0f;
	textToolsCanvas(false, "canvasA");
	textToolsCanvas(true, "canvasB");
}
//needs its own function because there are two canvas'
void textToolsCanvas(bool aproximate, const char* canID) {

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	vec2 canPos = ImGui::GetCursorScreenPos();

	drawList->AddRectFilledMultiColor(canPos, ImVec2(canPos.x + textEditorSize.x, canPos.y + textEditorSize.y), IM_COL32(50, 50, 50, 255), IM_COL32(50, 50, 60, 255), IM_COL32(60, 60, 70, 255), IM_COL32(50, 50, 60, 255));
	drawList->AddRect(canPos, ImVec2(canPos.x + textEditorSize.x, canPos.y + textEditorSize.y), white);

	ImGui::InvisibleButton(canID, textEditorSize);
	vec2 mpos = ImVec2(ImGui::GetIO().MousePos.x - canPos.x, ImGui::GetIO().MousePos.y - canPos.y);
	drawList->PushClipRect(canPos, ImVec2(canPos.x + textEditorSize.x, canPos.y + textEditorSize.y), true);

	bool zoomEnabled = IsItemHovered();

	float startX = -200;
	float letterSpacing = 15;
	float scaler = writer.scaleFactor * 20 * canvasZoomAmount;
	int accumulativeX = 0;

	//panning
	vec2 delta = prevMousePos - mpos;
	prevMousePos = mpos;
	if (IsMouseClicked(1))
		delta = vec2(0);
	if (IsMouseDown(1))
		CanvasPan -= delta;

	canPos += CanvasPan;

	//grid
	float gridScale = 50;
	ImColor gridCol = ImColor(255, 255, 255, 90);
	for (int j = 0; j < 20; j++)
	{
		drawList->AddLine(vec2(-textEditorSize.x * 2, j * gridScale) * canvasZoomAmount + canPos, vec2(textEditorSize.x * 2, j * gridScale) * canvasZoomAmount + canPos, gridCol, 4);
		drawList->AddLine(vec2(j * gridScale, -textEditorSize.y * 2) * canvasZoomAmount + canPos, vec2(j * gridScale, textEditorSize.y * 2) * canvasZoomAmount + canPos, gridCol, 4);
	}

	int j = 0;
	for (int i = 0; i < writer.curveCounts.size(); i++)
	{
		for (int mid = 0; mid < writer.curveCounts[i] - 1; mid++, j++)
		{
			curve c = writer.letterCurves[j];
			vec2 offset((writer.xAdvances[i] / writer.scaleFactor) * scaler + accumulativeX * canvasZoomAmount, (200 - writer.baseline / 2) * canvasZoomAmount);

			c *= canvasZoomAmount;


			if (writer.moveTypes[j] == 1)
				drawList->AddLine(c.start + canPos, c.end + canPos, red, drawThickness);

			else if (writer.moveTypes[j] == 2)
				drawList->AddLine(c.start + canPos, c.end + canPos, blue, drawThickness);

			else if (writer.moveTypes[j] == 3) {
				if (aproximate) {
					detailedArc* arcs = new detailedArc[arcNum];
					defer(delete arcs;);

					for (float j = 0; j < arcNum; j++)
					{
						float t0 = (1.0f / arcNum) * j;
						float t1 = t0 + (1.0f / arcNum);
						detailedArc ar = getArcApproximateDetailed(c, t0, t1, true);
						arcs[(int)j] = ar;
					}

					if (showFullArcCircle)
						for (int j = 0; j < arcNum; j++)
							drawList->AddCircle(arcs[j].intersectAB + canPos, arcs[j].radius, ImColor(255, 255, 0, 50), 128, 3);

					for (int j = 0; j < arcNum; j++)
						drawArc(arcs[j], drawList, canPos);
				}
				else
					drawBezier3(bezier3{ c.p0, c.p1, c.p2 }, drawList, canPos, white);
			}
		}
		accumulativeX += letterSpacing;
	}
	drawList->PopClipRect();
}

// Rectilinear infillf
vector<vec2> RIcontrolPoints;
vector<int> loopIndexes;
bool polyPointDragging = false;
int RIDragIndex = -1;
bool GenerateSample = false; //button flag
float infillSpacing = 15;
float infillLineWidth = 5;
float infillAngle = 0.785398f;
float rectPadding = 0;
float infillPadding = 20;

void rectilinearWindow() {

	Columns(2);
	SliderFloat("Infill Spacing", &infillSpacing, 1, 100);
	SliderFloat("Line Width", &infillLineWidth, 1, 100);
	SliderFloat("Line Angle", &infillAngle, 0, PI / 2);
	NextColumn();
	SliderFloat("Bounding box Padding", &rectPadding, -100, 100);
	SliderFloat("Infill Padding", &infillPadding, -100, 100);
	Columns(1);
	if (Button("Clear Points")) {
		RIcontrolPoints.clear();
		loopIndexes.clear();
		loopIndexes.push_back(0);
	}
	SameLine();
	if (Button("Start new Loop"))
		loopIndexes.push_back(RIcontrolPoints.size());
	//only on the first run
	if (loopIndexes.size() == 0)
		loopIndexes.push_back(0);
	if (Button("Generate G-Code"))
		GenerateSample = true;

	//GenerateSample = true;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	vec2 canPos = ImGui::GetCursorScreenPos();
	vec2 canSize = GetContentRegionAvail();

	drawList->AddRectFilledMultiColor(canPos, ImVec2(canPos.x + canSize.x, canPos.y + canSize.y), IM_COL32(50, 50, 50, 255), IM_COL32(50, 50, 60, 255), IM_COL32(60, 60, 70, 255), IM_COL32(50, 50, 60, 255));
	drawList->AddRect(canPos, ImVec2(canPos.x + canSize.x, canPos.y + canSize.y), white);

	ImGui::InvisibleButton("cool", canSize);
	vec2 mpos = ImVec2(ImGui::GetIO().MousePos.x - canPos.x, ImGui::GetIO().MousePos.y - canPos.y);
	drawList->PushClipRect(canPos, ImVec2(canPos.x + canSize.x, canPos.y + canSize.y), true);


	float circRad = 15;
	int hovIndex = -1;

	////mouse input
	if (ImGui::IsItemHovered()) {
		for (int i = 0; i < RIcontrolPoints.size(); i++)
		{
			if (distance((RIcontrolPoints[i]), mpos) < circRad + 10) {
				hovIndex = i;
				break;
			}
		}
		if (ImGui::IsMouseClicked(0) && hovIndex >= 0)
		{
			RIDragIndex = hovIndex;
			polyPointDragging = true;
		}
		else if (IsMouseClicked(0) && hovIndex == -1) {
			RIcontrolPoints.push_back(vec2(mpos));
		}
		if (!IsMouseDown(0))
			polyPointDragging = false;

		if (polyPointDragging) {
			RIcontrolPoints[RIDragIndex] = mpos;
			recalcLines = true;
		}
		curve t;
	}


	//bounding rect
	float W, H;
	vec2 cent;
	getBoundingBox(RIcontrolPoints, &W, &H, &cent);
	vec2 off = vec2(W, H) / 2.0f;
	//drawList->AddRectFilled(cent - off + canPos, cent + off + canPos, transparent);

	//convert control points to a collectin of lines for convenience
	vector<curve> polyLines;

	//connects the points into lines, skips a line every loop break index
	for (int l = 0; l < loopIndexes.size(); l++)
	{
		int loopStart = loopIndexes[l];
		int loopEnd = l + 1 < loopIndexes.size() ? loopIndexes[l + 1] - 1 : RIcontrolPoints.size() - 1;

		if (loopEnd - loopStart <= 0)
			break;

		for (int i = loopStart + 1; i < loopEnd + 1; i++)
			polyLines.push_back(cLine(RIcontrolPoints[i - 1], RIcontrolPoints[i]));
		polyLines.push_back(cLine(RIcontrolPoints[loopStart], RIcontrolPoints[loopEnd]));
	}


	//initial and trimmed segments
	vector<vector<curve>> trimmedLines;
	if (RIcontrolPoints.size() > 2) {
		vector<curve> fullSegments = fillRectWithLines(W, H, cent, infillSpacing, infillAngle, rectPadding);

		//display the filled rect with low opacity
		//for (int i = 0; i < fullSegments.size(); i++)
		//	drawLine(fullSegments[i], drawList, canPos, IM_COL32(255, 255, 0, 50), infillLineWidth);

		trimmedLines = getInfillBooleanOp(fullSegments, polyLines, infillPadding);

		//for (int i = 0; i < trimmedLines.size(); i++) {
		//	vector<curve> subdivisions = trimmedLines[i];
		//	for (int j = 0; j < subdivisions.size(); j++)
		//		drawLine(subdivisions[j], drawList, canPos, blue, infillLineWidth);
		//}
	}

	int listLength = trimmedLines.size();
	int networkCount = 0;
	if (listLength)
	{

		linkedLineSegment* segmentEntries = linkSegments(trimmedLines, infillAngle, infillSpacing);

		//organize forks amd draw debug info
		vector<rectilinearNetwork> networks = findNetworks(segmentEntries, listLength);

		networkCount = networks.size();

		vector<vector<curve>> loops;

		if (GenerateSample)
			loops.push_back(polyLines);

		for (int i = 0; i < networkCount; i++)
		{
			rectilinearNetwork net = networks[i];
			vector<curve> path = traverseRectillinearNetwork(net);
			for (int i = 0; i < path.size(); i++) {
				drawLine(path[i], drawList, canPos, ImColor(255, 137, 137, 255), infillLineWidth);
			}
			if (GenerateSample)
				loops.push_back(path);
		}
		//single network
		if (networkCount == 0) {

			rectilinearNetwork net;
			net.endB = &segmentEntries[0];
			net.endA = &segmentEntries[listLength - 1];
			net.count = listLength;

			vector<curve> path = traverseRectillinearNetwork(net);
			for (int i = 0; i < path.size(); i++)
				drawLine(path[i], drawList, canPos, ImColor(255, 137, 137, 255), infillLineWidth);
			if (GenerateSample)
				loops.push_back(path);
		}
		if (GenerateSample) {
			vector<vec2> flatArray;

			for (int i = 0; i < loops.size(); i++)
			{

				for (int j = 0; j < loops[i].size(); j++) {
					flatArray.push_back(loops[i][j].start);
					flatArray.push_back(loops[i][j].end);
				}
			}
			float W, H;
			vec2 center;
			getBoundingBox(flatArray, &W, &H, &center);
			vec2 offset = vec2(W, H) / 2.0f - center;
			float top = center.y + offset.y + H / 2;

			for (int i = 0; i < loops.size(); i++)
			{

				for (int j = 0; j < loops[i].size(); j++) {
					loops[i][j] = loops[i][j] + offset;
					loops[i][j].start.y = top - loops[i][j].start.y;
					loops[i][j].end.y = top - loops[i][j].end.y;
					loops[i][j] = loops[i][j] / 10.0f;
				}
			}

			vector<Gcommand> commands = createScript(loops);
			saveScript(commands);
		}
		GenerateSample = false;
		//for (int i = 0; i < listLength; i++)
		//{
		//	linkedLineSegment * LSeg = segmentEntries + i;
		//	do {
		//		//drawLine(lSeg->seg, drawList, canPos, red, 10);
		//		LSeg = LSeg->next;
		//	} while (LSeg);
		//}


		for (int i = 0; i < listLength; i++)
			if (segmentEntries[i].next)
				deleteSegmentsUp(segmentEntries[i].next);
		delete segmentEntries;
	}

	//draw polygonlines
	for (int i = 0; i < polyLines.size(); i++)
		drawLine(polyLines[i], drawList, canPos, white, infillLineWidth);

	//draw control points
	for (int i = 0; i < RIcontrolPoints.size(); i++) {
		drawList->AddCircleFilled(RIcontrolPoints[i] + canPos, 4, black, 8);
		drawList->AddCircle(RIcontrolPoints[i] + canPos, circRad, i == hovIndex ? red : white, 12, 1);
	}

	drawList->PopClipRect();

	Text("Networks: %d", networkCount);
}



//curve editor window

vector<vec2> CFcontrolPoints; //points of the polygon
vector<detailedArc> corners; //rounded corners

vector<float> angles;
bool cornerUpdate = false;
int pDragIndex = -1;
bool curvePointDragging = false;

float minAngle = PI / 2; //smallest angle to be concidered for the aproximation process
int minGroup = 4; //smallest collection of similar lines required to be aprixmated as an arc
float angleThreshold = PI / 2; //largest difference in neighboring angles when part of a chain
float centerThreshold = 50; //largest distance between arc centers to merge a chain
float radiusThreshold = 50; //largest difference in arc radius to merge a chain

/*
	Given a polygon or a series of lines/points, portions of them might closly fit a circular arc.
	These sections of lines should be simplified as an aproximated arc wherever possible as large number
	of lines can be represented with one arc. It also allows the Gcode interpreter to run faster (and draw smoother)
*/

void curveEditorWindow() {

	if (Button("Clear")) {
		CFcontrolPoints.clear();
		cornerUpdate = true;
	}

	SliderFloat("Min Angle", &minAngle, PI / 2, PI);
	SliderInt("Min Group", &minGroup, 3, 20);
	SliderFloat("Angle Threshold", &angleThreshold, 0, PI);
	SliderFloat("Arc Center Threshold", &centerThreshold, 0, PI);
	SliderFloat("Radius Threshold", &radiusThreshold, 0, PI);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	vec2 canPos = ImGui::GetCursorScreenPos();
	vec2 canSize = GetContentRegionAvail();

	drawList->AddRectFilledMultiColor(canPos, ImVec2(canPos.x + canSize.x, canPos.y + canSize.y), IM_COL32(50, 50, 50, 255), IM_COL32(50, 50, 60, 255), IM_COL32(60, 60, 70, 255), IM_COL32(50, 50, 60, 255));
	drawList->AddRect(canPos, ImVec2(canPos.x + canSize.x, canPos.y + canSize.y), white);

	ImGui::InvisibleButton("cool", canSize);
	vec2 mpos = ImVec2(ImGui::GetIO().MousePos.x - canPos.x, ImGui::GetIO().MousePos.y - canPos.y);
	drawList->PushClipRect(canPos, ImVec2(canPos.x + canSize.x, canPos.y + canSize.y), true);


	float circRad = 15;
	int hovIndex = -1;

	//mouse input
	if (ImGui::IsItemHovered()) {
		for (int i = 0; i < CFcontrolPoints.size(); i++)
		{
			if (distance((CFcontrolPoints[i]), mpos) < circRad) {
				hovIndex = i;
				break;
			}
		}
		if (ImGui::IsMouseClicked(0) && hovIndex >= 0)
		{
			pDragIndex = hovIndex;
			curvePointDragging = true;
		}
		else if (IsMouseClicked(0) && hovIndex == -1) {
			CFcontrolPoints.push_back(vec2(mpos));
			cornerUpdate = true;
		}
		if (!IsMouseDown(0))
			curvePointDragging = false;

		if (curvePointDragging) {
			CFcontrolPoints[pDragIndex] = mpos;
			recalcLines = true;
			cornerUpdate = true;
		}
	}



	//lines
	for (int i = 1; i < CFcontrolPoints.size(); i++)
		drawList->AddLine(CFcontrolPoints[i - 1] + canPos, CFcontrolPoints[i] + canPos, white, 2);
	//	if(pPoints.size() > 1)
	//		drawList->AddLine(pPoints[0] + canPos, pPoints[pPoints.size()-1] + canPos, white, 2);

		//dots
	for (int i = 0; i < CFcontrolPoints.size(); i++) {
		drawList->AddCircleFilled(CFcontrolPoints[i] + canPos, 4, black, 8);
		drawList->AddCircle(CFcontrolPoints[i] + canPos, circRad, i == hovIndex ? red : white, 12, 1);
	}

	/*
		For next time I'm working on this:

		Given the minimum chain length, seperate the algorithem into a series of jobs
		where each job has a start and end index. wherever there is a stretch of valid chains
		is a job.
		Worry about the merging operation within a job by job bases where the nearest break indexes are
		already known about when choosing merges and looking for conflicts A and B
	*/

	//maybe replace this conditional with a dirty flag for only recalculating when the points are moved
	if (true) {
		corners.clear();
		angles.clear();
		vector<indexRange> jobs; //chains of valid candidates

		//first we need the dot angles of from every 3 points to filter out bad candidates
		for (int i = 2; i < CFcontrolPoints.size(); i++)
			angles.push_back(get3PointAngleAcute(CFcontrolPoints[i - 2], CFcontrolPoints[i - 1], CFcontrolPoints[i]));

		//get all corners
		for (int i = 2; i < CFcontrolPoints.size(); i++)
			corners.push_back(getRoundedCorner(CFcontrolPoints[i - 2], CFcontrolPoints[i - 1], CFcontrolPoints[i], 0.5));

		//for the first pass, compair each corner pair for similarity and split into tasks
		int chainLength = 0;
		for (int i = 0; i < CFcontrolPoints.size() - 1; i++) {
			detailedArc arcA = corners[i];
			detailedArc arcB = corners[i + 1];

			//find how different the pair of corners are
			float difCent = distance(arcA.intersectAB, arcB.intersectAB);
			float difRad = abs(arcA.radius - arcB.radius);
			float difAngle = abs(angles[i] - angles[i + 1]);

			//skip if it's a break
			if (difAngle > angleThreshold ||
				difRad > radiusThreshold ||
				difCent > centerThreshold ||
				(arcA.determinant > 0) == (arcB.determinant > 0)) {
				if (chainLength > minGroup)
					jobs.push_back({ i, i + chainLength });
				chainLength = 0;
				continue;
			}
			chainLength++;
		}


		for (int i = 0; i < jobs.size(); i++)
		{
			vector<indexRange> chains;
			int mergeLevel = 0;
			indexRange currChain = { jobs[i].a, jobs[i].a };

			float smallestAngle = PI * 2;
			float biggestAngle = 0;
			int jobLength = jobs[i].b - jobs[i].a;

			for (int j = jobs[i].a; j < jobs[i].b; j++)
			{
				smallestAngle = glm::min(angles[j], smallestAngle);
				biggestAngle = glm::max(angles[j], biggestAngle);

				//if()

				//vec2 avCent = (arcA.intersectAB + arcB.intersectAB) / 2.0f;
				//float avRad = (arcA.radius + arcB.radius) / 2;
			}
		}

#if 0
		//you need 3 points to even create a triangle, so the calcuations are simple here
		if (minGroup == 3) {
			for (int i = 2; i < controlPoints.size(); i++) {
				//float angle = get3PointAngleAcute(pPoints[i - 2], pPoints[i - 1], pPoints[i]);


				float angle = angles[i - 2];
				if (angle < minAngle)
					continue;

				corners.push_back(getRoundedCorner(controlPoints[i - 2], controlPoints[i - 1], controlPoints[i], 0.5));

				sprintf(labelBuff, "%1.2f", angle);
				drawList->AddText(canvasFont, 25, controlPoints[i - 1] + canPos + 30.0f, white, labelBuff);
			}
		}
		else {
			for (int i = 3; i < controlPoints.size(); i++) {
				float angleA = angles[i - 3];
				float angleB = angles[i - 2];
				if (abs(angleA - angleB) < angleThreshold) {

					detailedArc arcA = getRoundedCorner(controlPoints[i - 3], controlPoints[i - 2], controlPoints[i - 1], 0.5);
					detailedArc arcB = getRoundedCorner(controlPoints[i - 2], controlPoints[i - 1], controlPoints[i], 0.5);
					corners.push_back(arcA);
					corners.push_back(arcB);

					vec2 avCent = (arcA.intersectAB + arcB.intersectAB) / 2.0f;
					float avRad = (arcA.radius + arcB.radius) / 2;
					drawList->AddCircle(avCent + canPos, avRad, blue, 128, 3);

					sprintf(labelBuff, "%1.2f A", angleA);
					drawList->AddText(canvasFont, 25, controlPoints[i - 2] + canPos + 30.0f, white, labelBuff);
					sprintf(labelBuff, "%1.2f B", angleB);
					drawList->AddText(canvasFont, 25, controlPoints[i - 1] + canPos + 30.0f, white, labelBuff);
				}
			}
		}
#endif
	}
	for (int i = 0; i < corners.size(); i++)
	{
		drawArc(corners[i], drawList, canPos);
	}

	drawList->PopClipRect();
}

void drawLine(curve line, ImDrawList* drawList, vec2 canPos, ImColor col, float width) {
	drawList->AddLine(line.start + canPos, line.end + canPos, col, width);
}

void drawBezier4(bezier4 b4, ImDrawList* drawList, vec2 canPos, ImColor col) {
	drawList->AddBezierCurve(
		ImVec2(bpoints[0].x + canPos.x, bpoints[0].y + canPos.y),
		ImVec2(bpoints[1].x + canPos.x, bpoints[1].y + canPos.y),
		ImVec2(bpoints[2].x + canPos.x, bpoints[2].y + canPos.y),
		ImVec2(bpoints[3].x + canPos.x, bpoints[3].y + canPos.y),
		col, drawThickness);
}

void drawBezier3(bezier3 b3, ImDrawList* drawList, vec2 canPos, ImColor col) {
	vec2 c1 = b3.P0 + (2 / 3.0f) * (b3.C0 - b3.P0) + canPos;
	vec2 c2 = b3.P1 + (2 / 3.0f) * (b3.C0 - b3.P1) + canPos;

	drawList->AddBezierCurve(
		b3.P0 + canPos,
		c1,
		c2,
		b3.P1 + canPos,
		col, drawThickness);
}

void drawCircleChords(detailedArc ar, ImDrawList* drawList, vec2 canPos) {
	drawList->AddLine(ar.curveMiddle + canPos, ar.start + canPos, orange);
	drawList->AddLine(ar.curveMiddle + canPos, ar.end + canPos, orange);

	//A
	drawList->AddCircle(ar.A0 + canPos, 8, orange);
	drawList->AddLine(ar.A0 + canPos, ar.A1 + canPos, orange);

	//B
	drawList->AddCircle(ar.B0 + canPos, 8, orange);
	drawList->AddLine(ar.B0 + canPos, ar.B1 + canPos, orange);

	//C
	drawList->AddLine(ar.start + canPos, ar.end + canPos, orange);
	drawList->AddCircle(ar.C0 + canPos, 8, orange);
	drawList->AddLine(ar.C0 + canPos, ar.C1 + canPos, orange);
}

void drawArc(detailedArc ar, ImDrawList* drawList, vec2 canPos) {
	float lineLength = 600;

	if (showCircleChords) {
		drawCircleChords(ar, drawList, canPos);
	}

	if (showArcCenters)
		drawList->AddCircle(ar.intersectAB + canPos, 8, orange);

	if (showArc) {

		bool endsFlip = false;
		if (ar.start.x > ar.end.x) {
			vec2 temp = ar.start;
			ar.start = ar.end;
			ar.end = temp;
			endsFlip = true;
		}

		float min = atan2(ar.intersectAB.y - ar.start.y, ar.intersectAB.x - ar.start.x);
		float max = atan2(ar.intersectAB.y - ar.end.y, ar.intersectAB.x - ar.end.x);

		//start pos
		drawList->PathLineTo(ar.start + canPos);
		drawList->PathStroke(invisible, false, 0);


		bool flip = false;

		if (showCorrection && !endsFlip) {
			flip = (ar.end.y > ar.intersectAB.y && ar.determinant > 0 ||
				ar.end.y < ar.intersectAB.y && ar.determinant < 0);
		}
		else if (showCorrection && endsFlip) {
			flip = (ar.end.y > ar.intersectAB.y && ar.determinant < 0 ||
				ar.end.y < ar.intersectAB.y && ar.determinant > 0);
		}


		if (flip) {
			if (endsFlip) {
				if (flip && ar.determinant > 0) {
					max = -max;
					drawList->PathArcTo(ar.intersectAB + canPos, ar.radius, min + PI + amin, max - (2 * PI + max * 2) + PI + amax, 32);
				}
				else if (flip) {
					max = -max;
					drawList->PathArcTo(ar.intersectAB + canPos, ar.radius, min + amin + PI, max + (2 * PI - max * 2) + amax + PI, 32);
				}
			}
			else {
				if (flip && ar.determinant < 0) {
					max = -max;
					drawList->PathArcTo(ar.intersectAB + canPos, ar.radius, min + PI + amin, max - (2 * PI + max * 2) + PI + amax, 32);
				}
				else if (flip) {
					max = -max;
					drawList->PathArcTo(ar.intersectAB + canPos, ar.radius, min + amin + PI, max + (2 * PI - max * 2) + amax + PI, 32);
				}
			}
		}
		else
			drawList->PathArcTo(ar.intersectAB + canPos, ar.radius, min + amin + PI, max + amax + PI, 64);

		//if(flip)
		//	drawList->PathStroke(black, false, 4);
		//else
		drawList->PathStroke(yellow, false, 4);

		//drawList->AddCircleFilled(ar.start + canPos, 10, red);
		//drawList->AddCircleFilled(ar.end + canPos, 10, black);
	}
}

//////image stitching

#ifndef __OPENCV_INSTALLED__
void imgStitchWindow() {
	Text("\n\
		You are seeing this message because OpenCV is not installed or was not found.\n\
		OpenCV is required here to load and pare XML files.\n\
		Please check your source includes before recompiling.\n\
		The features of this window will become available when OpenCV is successfully compiled.");
}
#else
char imSetDataBuff[256] = "../data/piPics/work_set/calib/";
char stitchSettingsBuff[256] = "../data/stitch_settings.xml";
bool scanLoaded = false, updateStitch = false;
float skew = 1.0f;
stitcherSettings workSettings;
GLuint scanTexture;
vector<stitchImg> stitchImgs;
vec2** imagePositions = NULL;
vector<vec2> positionOffsets;
vec2** imageOffsets;
bool offsetDragging = false;
int dragIndexX, dragIndexY;
int imcols = 9, imrows = 9;
thread scanThread;
bool scanning = false, scanFinished = false;
int totalImgs = 0;
volatile int correctedCount = -1; //for loading portions of the image as it's downloaded
int loadedCount = -1;
vec2 currentCamOffset = vec2(167.693, 296.906);  // in mm
vec2 currentPixelScale = vec2(7.03628, 9.04207); // in pix/mm
float stitchAspect;

vec2 mapScannedPixelToSurfaceLocation(vec2 targetPixelCoord) {
	vec2 target = targetPixelCoord / -currentPixelScale; 
	return vec2(target.y, target.x) + currentCamOffset;
}

cv::Mat getCameraImage() {
	sendCommand("O"); //start camera

	//take the picture
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	sendCommand("Itemp.jpg");
	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	//correct lense distortion
	auto matrixLocation = "../data/Pi_camera_calibration_matrix.xml";
	auto InputFolder = "../data/piPics/";

	//load calibration settings
	cout << "loading distortion matrix" << endl;
	FileStorage fs(matrixLocation, FileStorage::READ);
	Mat intrinsicMatrix, distortionCoeffs;
	fs["camera_matrix"] >> intrinsicMatrix;
	fs["distortion_coefficients"] >> distortionCoeffs;

	//create rectification map
	Mat map1, map2;
	initUndistortRectifyMap(intrinsicMatrix, distortionCoeffs, Mat(), intrinsicMatrix, Size(2592, 1944), CV_16SC2, map1, map2);
	Rect crop(245, 22, 2335, 1899);

	char callbuff[1024];
	auto dataPath = std::filesystem::absolute("../data/piPics/").generic_string();
	auto executablePath = std::filesystem::absolute("../SFTP_download/SFTP_download/bin/Debug/").generic_string();


	sprintf(callbuff, "%sSFTP_download.exe %s download /home/pi/imgs/temp.jpg %stemp.jpg", executablePath.c_str(), ipBuff, dataPath.c_str());
	int retCode = system(callbuff);
	if (retCode) {
		cout << "error calling download exe" << endl;
		return cv::Mat();
	}

	cout << "correcting image" << endl;
	Mat orig = imread(string(InputFolder) + "temp.jpg", IMREAD_GRAYSCALE);
	Mat fixed, shrunk, mid;
	resize(orig, mid, Size(2592, 1944));
	remap(mid, fixed, map1, map2, INTER_LINEAR);
	resize(fixed(crop), shrunk, Size(648, 486));
	imwrite(string(InputFolder) + "fixed_temp.jpg", shrunk);

	sendCommand("S"); //stop camera
	return shrunk;
}

//orders by y then by x
bool operator < (stitchImg& a, stitchImg& b) {
	return a.position.y < b.position.y || (a.position.y == b.position.y && a.position.x < b.position.x);
}

void downloadThreadFunc(int total, volatile int* reached, volatile bool* stopDLThread) {
	cout << "Starting download thread" << endl;

	//create undistorted set
	auto matrixLocation = "../data/Pi_camera_calibration_matrix.xml";
	auto InputFolder = "../data/piPics/work_set";
	auto outputFolder = "../data/piPics/work_set/calib";

	//load calibration settings
	cout << "loading distortion matrix" << endl;
	FileStorage fs(matrixLocation, FileStorage::READ);
	Mat intrinsicMatrix, distortionCoeffs;
	fs["camera_matrix"] >> intrinsicMatrix;
	fs["distortion_coefficients"] >> distortionCoeffs;

	//create rectification map
	Mat map1, map2;
	initUndistortRectifyMap(intrinsicMatrix, distortionCoeffs, Mat(), intrinsicMatrix, Size(2592, 1944), CV_16SC2, map1, map2);
	Rect crop(245, 22, 2335, 1899);

	char callbuff[1024];
	auto dataPath = std::filesystem::absolute("../data/piPics/work_set/").generic_string();
	auto executablePath = std::filesystem::absolute("../SFTP_download/SFTP_download/bin/Debug/").generic_string();

	correctedCount = 0;
	loadedCount = 0;

	for (int i = 0; i < total; i++)
	{
		if (*stopDLThread)
			return;
		while (i >= *reached) {
			if (*stopDLThread)
				return;
			std::this_thread::yield();
		}


		sprintf(callbuff, "%sSFTP_download.exe %s download /home/pi/imgs/pano%d.jpg %spano%d.jpg", executablePath.c_str(), ipBuff, i, dataPath.c_str(), i);
		int retCode = system(callbuff);
		if (retCode) {
			cout << "error calling download exe" << endl;
			return;
		}
		else {
			cout << "correcting image " << i << endl;
			Mat orig = imread(string(InputFolder) + "/pano" + to_string(i) + ".jpg", IMREAD_GRAYSCALE);
			Mat fixed, shrunk, mid;
			resize(orig, mid, Size(2592, 1944));
			remap(mid, fixed, map1, map2, INTER_LINEAR);
			resize(fixed(crop), shrunk, Size(648, 486));
			imwrite(outputFolder + string("/fixed_pano") + to_string(i) + ".jpg", shrunk);
			correctedCount++;
		}
	}
}

void scanThreadFunc(int xA = 0, int yA = 0, int xB = 180, int yB = 260) {
	sendCommand("O"); //start camera

	//int xA = 0, yA = 0, xB = 180, yB = 260, imageDensity = 20;
	int imageDensity = 20;


	int xRange = ((xB / 20) - (xA / 20)) + 1;
	int yRange = ((yB / 20) - (yA / 20)) + 1;
	int total = xRange * yRange;
	int picNumber = 0;
	imcols = xRange;
	imrows = yRange;

	//wait for position or wait for 10 seconds (in case of error readings)
	auto timer = glfwGetTime();
	sendCommand(Gcommand("G1 X0 Y0 F5000"));
	while (getPosition() != vec2(0) && glfwGetTime() - timer < 10)
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	auto ttt = getPosition();
	auto tmep = glfwGetTime() - timer;
	volatile int reached = 0;
	volatile bool stopDownload = false;
	thread downloadThread(downloadThreadFunc, total, &reached, &stopDownload);

	for (int i = 0; i < total; i++, picNumber++)
	{
		float x = (i / yRange) * 20 + xA, y = yA;
		if ((i / yRange) % 2 == 0)
			y += (i % yRange) * 20;
		else
			y += ((yRange - 1) - i % yRange) * 20;



		char buffer[64];
		sprintf(buffer, "G1 X%.2f Y%.2f F5000", x, y);
		sendCommand(Gcommand(buffer));
		std::this_thread::sleep_for(std::chrono::milliseconds(700));

		sprintf(buffer, "Ipano%d.jpg", picNumber);
		sendCommand(Gcommand(buffer));
		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		stitchImgs.push_back(stitchImg("pano" + to_string(i) + ".jpg", vec2(y, x), vec2(584, 475)));
		reached++;
		//std::sort(stitchImgs.begin(), stitchImgs.end());
	}
	reached++;

	sendCommand("S"); //stop camera

	//char callbuff[1024];
	auto dataPath = std::filesystem::absolute("../data/piPics/work_set/").generic_string();
	auto executablePath = std::filesystem::absolute("../SFTP_download/SFTP_download/bin/Debug/").generic_string();

	downloadThread.join();

	std::sort(stitchImgs.begin(), stitchImgs.end());

	//save scan data to xml in case it's needed later
	{
		cout << "saving scan metadata" << endl;
		FileStorage fs(dataPath + "/calib/scan_data.xml", cv::FileStorage::WRITE);
		fs <<
			"colums" << xRange <<
			"rows" << yRange;
		for (int i = 0; i < total; i++)
			fs << "stitchImg" + to_string(i) << stitchImgs[i];
		fs.release();
	}

	////create undistorted set
	//{
	//	auto matrixLocation = "../data/Pi_camera_calibration_matrix.xml";
	//	auto InputFolder = "../data/piPics/work_set";
	//	auto outputFolder = "../data/piPics/work_set/calib";

	//	//load calibration settings
	//	cout << "loading distortion matrix" << endl;
	//	FileStorage fs(matrixLocation, FileStorage::READ);
	//	Mat intrinsicMatrix, distortionCoeffs;
	//	fs["camera_matrix"] >> intrinsicMatrix;
	//	fs["distortion_coefficients"] >> distortionCoeffs;

	//	//create rectification map
	//	Mat map1, map2;
	//	initUndistortRectifyMap(intrinsicMatrix, distortionCoeffs, Mat(), intrinsicMatrix, Size(2592, 1944), CV_16SC2, map1, map2);
	//	Rect crop(245, 22, 2335, 1899);

	//	for (int i = 0; i < total; i++)
	//	{
	//		cout << "correcting image " << i << endl;
	//		Mat orig = imread(string(InputFolder) + "/pano" + to_string(i) + ".jpg", IMREAD_GRAYSCALE);
	//		Mat fixed, shrunk, mid;
	//		resize(orig, mid, Size(2592, 1944));
	//		remap(mid, fixed, map1, map2, INTER_LINEAR);
	//		resize(fixed(crop), shrunk, Size(648, 486));
	//		imwrite(outputFolder + string("/fixed_pano") + to_string(i) + ".jpg", shrunk);
	//	}
	//}
	scanFinished = true;
	totalImgs = total;
}

void imgStitchWindow() {
	InputText("image source folder", imSetDataBuff, IM_ARRAYSIZE(imSetDataBuff));
	if (Button("Load Data")) {
		scanLoaded = true;
		cv::FileStorage fs(String(imSetDataBuff) + "scan_data.xml", cv::FileStorage::READ);
		fs["colums"] >> imcols;
		fs["rows"] >> imrows;
		for (int i = 0; i < imcols * imrows; i++)
		{
			stitchImg im;
			fs["stitchImg" + to_string(i)] >> im;
			im.scale = vec2(584, 475); //arbitrary
			stitchImgs.push_back(im);
		}
		fs.release();
		for (int i = 0; i < imcols * imrows; i++)
		{
			GLuint id;
			int unusedX, unusedY;
			loadTexture((String(imSetDataBuff) + "fixed_" + stitchImgs[i].file).c_str(), &id, &unusedX, &unusedY);
			stitchImgs[i].Id = id;
		}
		updateStitch = true;

		//imageOffsets = new vec2 * [imcols];
		//for (int i = 0; i < imcols; i++)
		//	imageOffsets[i] = new vec2[imrows];
		//for (int i = 0; i < imcols; i++)
		//	for (int j = 0; j < imrows; j++)
		//		imageOffsets[i][j] = vec2(0);
	}
	SameLine();
	InputText("Scan data filepath", stitchSettingsBuff, IM_ARRAYSIZE(stitchSettingsBuff));
	SameLine();
	bool loadSettings = Button("Load Settings");

	SameLine();
	if (Button("Save")) {
		saveStitcherSettings(cv::String(stitchSettingsBuff), workSettings);
	}

	if (Button("ScanSurface")) {
		loadSettings = true;
		scanThread = thread(scanThreadFunc, 0, 0, 180, 260);
		scanning = true;
	}

	showCameraCalibrationWindow |= Button("Calibrate Camera");

	if (correctedCount > loadedCount)
	{
		//load images into memory
		cout << "loading corrected images into memory" << endl;
		for (int unused = 0; loadedCount < correctedCount; loadedCount++)
		{
			GLuint id;
			int unusedX, unusedY;
			loadTexture((String(imSetDataBuff) + "fixed_" + stitchImgs[loadedCount].file).c_str(), &id, &unusedX, &unusedY);
			stitchImgs[loadedCount].Id = id;
		}
		scanLoaded = true;
		updateStitch = true;
	}

	if (scanning && scanFinished) {
		scanning = false;
		scanFinished = false;
		//-+loadedCount = -1;
	}

	//settings bar
	Columns(2);
	updateStitch |= SliderFloat("Spread X", &workSettings.spreadX, 0, 15);
	updateStitch |= SliderFloat("Ladder X", &workSettings.ladderX, -20, 20);
	//updateStitch |= SliderFloat("Offset X", &workSettings.offsetX, -1000, 1000);
	updateStitch |= SliderFloat("Crop X", &workSettings.cropX, 0, 0.5f);
	workSettings.featherX = glm::max(workSettings.cropX + 0.01f, workSettings.featherX);
	updateStitch |= SliderFloat("Feather X", &workSettings.featherX, workSettings.cropX + 0.01f, 1);
	NextColumn();
	updateStitch |= SliderFloat("Spread Y", &workSettings.spreadY, 0, 15);
	updateStitch |= SliderFloat("Ladder Y", &workSettings.ladderY, -20, 20);
	//updateStitch |= SliderFloat("Offset Y", &workSettings.offsetY, -1000, 1000);
	updateStitch |= SliderFloat("Crop Y", &workSettings.cropY, 0, 0.5f);
	workSettings.featherY = glm::max(workSettings.cropY + 0.01f, workSettings.featherY);
	updateStitch |= SliderFloat("Feather Y", &workSettings.featherY, workSettings.cropY + 0.01f, 1);
	Columns(1);
	updateStitch |= SliderFloat("Angle", &workSettings.imAngle, -PI, PI);
	updateStitch |= SliderFloat("Threshold", &workSettings.binaryThresh, 0, 1);
	updateStitch |= SliderFloat("Skew", &workSettings.skew, -1, 1);
	updateStitch |= SliderFloat("Orbit", &workSettings.orbit, -PI, PI);

	if (scanLoaded) {
		vec2 canSize = vec2(ImGui::GetContentRegionAvail().y);
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		vec2 canPos = ImGui::GetCursorScreenPos();
		vec2 mpos = ImVec2(ImGui::GetIO().MousePos.x - canPos.x, ImGui::GetIO().MousePos.y - canPos.y);
		//ImGui::InvisibleButton("cool", canSize);
		vec2 canOff = canPos + canSize / 2.0f;
		float ratio = (canSize.y / 1700);

		stitchAspect = ((float)imrows / (float)imcols);
		vec2 viewSize = vec2(stitchAspect * canSize.y, canSize.y);

		if (updateStitch) {
			if (imagePositions != NULL)
				delete imagePositions;

			imagePositions = new vec2 * [imcols];
			for (int i = 0; i < imcols; i++)
				imagePositions[i] = new vec2[imrows];

			//stitch and get the texture
			scanTexture = simpleStitch(stitchImgs, viewSize.y / viewSize.x, workSettings, imagePositions, imageOffsets, loadedCount);
			gl(BindFramebuffer(GL_FRAMEBUFFER, 0));
			updateStitch = false;
		}

		Image((ImTextureID)scanTexture, viewSize);

		float circRad = 15;
		int hoverX = -1, hoverY = -1;

		////mouse input
		//if (ImGui::IsItemHovered()) {
		//	for (int i = 0; i < imcols; i++)
		//	{
		//		for (int j = 0; j < imrows; j++)
		//		{
		//			if (distance((canOff + imagePositions[i][j] * ratio + imageOffsets[i][j] * ratio), canPos + mpos) < circRad + 10) {
		//				hoverX = i;
		//				hoverY = j;
		//				break;
		//			}
		//		}

		//	}
		//	if (ImGui::IsMouseClicked(0) && hoverX >= 0)
		//	{
		//		dragIndexX = hoverX;
		//		dragIndexY = hoverY;
		//		offsetDragging = true;
		//	}
		//	if (!IsMouseDown(0))
		//		offsetDragging = false;

		//	if (offsetDragging) {
		//		imageOffsets[dragIndexX][dragIndexY] = -((canOff + imagePositions[dragIndexX][dragIndexY] * ratio) - (canPos + mpos));
		//		updateStitch = true;
		//	}
		//	curve t;
		//}

		drawList->PushClipRect(canPos, ImVec2(canPos.x + canSize.x, canPos.y + canSize.y), true);

		for (int i = 0; i < imcols; i++) {
			for (int j = 0; j < imrows; j++)
			{
				ImColor col = (i == hoverX && j == hoverY) ? red : white;
				//	if (i % 2 == 0 && j % 2 == 0)
				//		drawList->AddCircleFilled(canOff + imagePositions[i][j] * ratio + imageOffsets[i][j] * ratio, 8, col);
					//drawList->AddCircleFilled(canPos + mpos, 4, red);
					//if (i > 0)
				//		drawList->AddLine(canOff + imagePositions[i - 1][j] * ratio, canOff + imagePositions[i][j] * ratio, white);
				//	if (j > 0)
				//		drawList->AddLine(canOff + imagePositions[i][j - 1] * ratio, canOff + imagePositions[i][j] * ratio, white);
			}
		}


		if (canSize.x < 50.0f)
			canSize.x = 50.0f;

		//	cv::Mat out = loadTexture(scanTexture, 1700, 1700);
		//	cv::imshow("out", out);
		//	cv::waitKey();
		drawList->PopClipRect();
	}

	if (loadSettings) {
		workSettings = loadStitcherSettings(cv::String(stitchSettingsBuff));
		updateStitch = true;
	}

	if (showCameraCalibrationWindow) {
		cameraCalibrationWindow();
	}
}

float initialCamOffsetX = 30, initialCamOffsetY = -30;
float initialPixelScaleX = 7.23f, initialPixelScaleY = 7.23f;
GLuint XimID;
bool XimLoaded = false;
vec2 XimDims;

vec2 findXCent(Mat im) {
	//find X center
	//Mat colX = im; //need for red lines
//	cvtColor(im, colX, CV_8U);
	//cvtColor(im, im, COLOR_BGR2GRAY);

	Mat edges, gray;
	Canny(im, edges, 50, 200, 3);

	vector<Vec2f> lines;
	vector<curve> Xlines;
	HoughLines(edges, lines, 1, (3.5 * CV_PI) / 180, 100, 0, 0);

	//filter bullshit duplicate lines
	for (size_t i = 0; i < lines.size(); i++)
	{
		Point pt1, pt2;
		{

			float rho = lines[i][0], theta = lines[i][1];
			double a = cos(theta), b = sin(theta);
			double x0 = a * rho, y0 = b * rho;
			pt1.x = cvRound(x0 + 1000 * (-b));
			pt1.y = cvRound(y0 + 1000 * (a));
			pt2.x = cvRound(x0 - 1000 * (-b));
			pt2.y = cvRound(y0 - 1000 * (a));
		}

		for (int j = i; j < lines.size(); j++)
		{
			if (j == i)
				continue;

			float rho = lines[j][0], theta = lines[j][1];
			Point ppt1, ppt2;
			double a = cos(theta), b = sin(theta);
			double x0 = a * rho, y0 = b * rho;
			ppt1.x = cvRound(x0 + 1000 * (-b));
			ppt1.y = cvRound(y0 + 1000 * (a));
			ppt2.x = cvRound(x0 - 1000 * (-b));
			ppt2.y = cvRound(y0 - 1000 * (a));

			if (abs(pt1.x - ppt1.x) < 50)
				goto skip;
		}

		//	line(colX, pt1, pt2, Scalar(0, 0, 255), 1, CV_AA);
		Xlines.push_back(cLine(vec2(pt1.x, pt1.y), vec2(pt2.x, pt2.y)));
	skip:;
	}
	if (Xlines.size() != 2)
	{
		cout << "Error detecting calibration mark" << endl;
		return vec2(-1);
	}
	vec2 cent = getLinearIntersection(Xlines[0], Xlines[1]);

	circle(im, cv::Point(cent.x, cent.y), 10, Scalar(255, 0, 0), 3);
	return cent;
}

void cameraCalibrationWindow() {

	ImGuiWindowFlags window_flags = 0;
	window_flags |= ImGuiWindowFlags_NoCollapse;
	window_flags |= ImGuiWindowFlags_NoNav;
	window_flags |= ImGuiWindowFlags_NoDocking;
	Begin("Calibrate Camera", &showCameraCalibrationWindow, window_flags);
	ImGui::SetWindowFocus();

	if (Button("Preview Camera")) {
		Mat im = getCameraImage();
		imshow("Camera preview", im);
	}

	vec2 pixScale(initialPixelScaleX, initialPixelScaleY);
	if (Button("Run self calibration")) {
		vec2 camOffset(initialCamOffsetX, initialCamOffsetY);

#if 1
		penUpNow();
		moveAndWait(vec2(50));

		//draw a big X
		vector<vector<curve>> loops;
		{
			vector<curve> loop;
			loop.push_back(cLine(vec2(50), vec2(110)));
			loops.push_back(loop);
		}
		{
			vector<curve> loop;
			loop.push_back(cLine(vec2(50, 110), vec2(110, 50)));
			loops.push_back(loop);
		}
		auto commands = createScript(loops);
		for (int i = 0; i < commands.size(); i++)
			sendCommand(commands[i]);
		penUpNow();

		//take a picture
		//moveAndWait(vec2(80) + camOffset);
		moveNow(vec2(80) + camOffset);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		Mat im = getCameraImage();
		//imshow("Recent image", im);
		penUpNow();

#else
		Mat im = imread("C:/Git/Capstone/GcodeWorkbench/data/piPics/fixed_temp.jpg");
#endif

		cout << "finding X center" << endl;
		vec2 xCent = findXCent(im);
		if (xCent == vec2(-1))
		{
			End();
			return;
		}
		circle(im, cv::Point(xCent.x, xCent.y), 10, Scalar(255, 0, 0), 1);
		if (xCent.x > 100 && xCent.y > 100) {
			cout << "refining X center" << endl;
			Mat cropped = im(Rect(xCent.x - 100, xCent.y - 100, 200, 200));
			vec2 newcent = findXCent(cropped);
			if (newcent == vec2(-1))
				cout << "refining failed.. continuing anyways" << endl;
			else {
				xCent = xCent - 100.0f + newcent;
				circle(im, cv::Point(xCent.x, xCent.y), 10, Scalar(255, 0, 0), 3);

			}
		}

		XimDims = vec2(im.cols, im.rows);
		vec2 pixelOffset = xCent - (XimDims / 2.0f); //x is flipped and y is automatically flipped
		currentCamOffset = (pixelOffset * vec2(-1, 1)) / pixScale + camOffset;

		loadTexture(im, &XimID, im.cols, im.rows);
		XimLoaded = true;
	}

	if (XimLoaded && Button("Test calibration")) {

		penUpNow();
		//take a picture
		vec2 captureLocation = vec2(80) + currentCamOffset;
		//moveAndWait(captureLocation); 
		moveNow(captureLocation);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		Mat im = getCameraImage();

		penUpNow();

		cout << "finding X center" << endl;
		vec2 xCent = findXCent(im);
		if (xCent == vec2(-1))
		{
			End();
			return;
		}
		circle(im, cv::Point(xCent.x, xCent.y), 10, Scalar(255, 0, 0), 1);
		if (xCent.x > 100 && xCent.y > 100) {
			cout << "refining X center" << endl;
			Mat cropped = im(Rect(xCent.x - 100, xCent.y - 100, 200, 200));
			vec2 newcent = findXCent(cropped);
			if (newcent == vec2(-1))
				cout << "refining failed.. continuing anyways" << endl;
			else {
				xCent = xCent - 100.0f + newcent;
				circle(im, cv::Point(xCent.x, xCent.y), 10, Scalar(255, 0, 0), 3);

			}
		}
		imshow("Recent image", im);

		vec2 XimCent = vec2(im.cols, im.rows) / 2.0f;
		vec2 pixelsFromImageCent = xCent - XimCent;
		vec2 realLocation = captureLocation - currentCamOffset + (pixelsFromImageCent * vec2(-1, 1)) / pixScale;

		penUpNow();
		moveNow(realLocation);
		penDownNow();

		//auto circ = makeCirc(realLocation, 5);
		//vector < vector<curve>> loops;		
		//loops.push_back(circ);
		//auto commands = createScript(loops);
		//for (int i = 0; i < commands.size(); i++)
		//{
		//	sendCommand(commands[i]);
		//}
	}


	if (Button("Draw Stitching calibration marks")) {
#if 1
		penUpNow();
		moveNow(vec2(0, 50));
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		moveAndWait(vec2(0, 20));

		//draw two targets
		vec2 padding(0, 50);
		vector<vector<curve>> loops;
		{
			vector<curve> loop;
			loop.push_back(cLine(vec2(0, 0), vec2(50, 50)) + padding);
			loops.push_back(loop);
		}
		{
			vector<curve> loop;
			loop.push_back(cLine(vec2(0, 50), vec2(50, 0)) + padding);
			loops.push_back(loop);
		}
		{
			vector<curve> loop;
			loop.push_back(cLine(vec2(0, 0), vec2(50, 50)) + vec2(100, 190) + padding);
			loops.push_back(loop);
		}
		{
			vector<curve> loop;
			loop.push_back(cLine(vec2(0, 50), vec2(50, 0)) + vec2(100, 190) + padding);
			loops.push_back(loop);
		}

		auto commands = createScript(loops);
		for (int i = 0; i < commands.size(); i++)
			sendCommand(commands[i]);
		penUpNow();
#endif

#if 0
		moveAndWait(vec2(0, 50));
		scanThread = thread(scanThreadFunc, 0, 0, 180, 260);
		scanning = true;
#endif
	}
	if(Button("Calculate stitching location mapping")){

	//	Mat orig = imread("../data/piPics/stitchcalib.PNG");
		Mat orig = loadTexture(scanTexture, 2300, 2300);
		Mat crop;
		//imshow("original", orig);
		orig(Rect(250, 250, 1800, 1800)).copyTo(crop);
		//imshow("crop", crop);
		Mat left, right;
		crop(Rect(0, 0, 900, 1800)).copyTo(left);
		crop(Rect(900, 0, 900, 1800)).copyTo(right);
		vec2 centLeft = findXCent(left);
		vec2 centRight = findXCent(right) + vec2(900, 0);

		if (centLeft == vec2(-1) || centRight == vec2(-1)) {
			End();
			return;
		}

		cout << "detected calibration marks successfully" << endl;

		//imshow("left", left);
		//imshow("right", right);

		vec2 centDelta = vec2(abs(centLeft.x - centRight.x), abs(centLeft.y - centRight.y));

		vec2 realCentA(25, 75);
		vec2 realCentB(125, 265);
		
		currentPixelScale = vec2(centDelta.x / 190.0f, centDelta.y / 100.0f);

		vec2 leftOffmm = (centLeft / currentPixelScale);
		currentCamOffset = -(-vec2(leftOffmm.y, leftOffmm.x) - realCentB);

		cout << "Pixel scale set to " << currentPixelScale.x << ", " << currentPixelScale.y << endl;
		cout << "camera offset set to " << currentCamOffset.x << ", " << currentCamOffset.y << " mm" << endl;

	//	vec2 target = ( vec2(1567, 1294)) / -currentPixelScale ; //vec2(1064, 1391) + 0.0f)

		//moveNow(vec2(target.y, target.x) + +currentCamOffset);
	}
	if (XimLoaded)
		Image((ImTextureID)XimID, XimDims);


	End();
}

#endif


//vector trace window (if OpenCV is installed)
#ifndef __OPENCV_INSTALLED__
void vectorTraceWindow() {
	Text("\n\
		You are seeing this message because OpenCV is not installed or was not found.\n\
		Please check your source includes before recompiling.\n\
		The features of this window will become available when OpenCV is successfully compiled.");
}
void traceSettingsWindow() {}
#else
using namespace cv;

char picPathBuff[256] = "../data/trace1.png";
//char picPathBuff[256] = "../data/trace1.png";
bool picLoaded = false;
bool updateResult = false;
bool traceWindow = false;
int remainingJobs = 0;
bool contoursCreated = false;
volatile bool sorted = false;
bool polysCreated = false;
bool updatePolys = true;
int thresh = 100;
int displayedLines = 0, totalLines = 0;
int totalLoops = 0;
//int sortedLoops = 0;
int contourCount = 0;
float pEpsilon = 2.0f;
float traceWidth = 170.0f;
float traceMaxX = 0;
float traceMaxY = 0;
bool traceInfill = true;
float traceInfillWidth = 5.0f;
cv::RNG rng(12345);
cv::Mat cvsourceImg;
cv::Mat cvsourceImgGray;
cv::Mat cannyResult;
cv::Mat cvResult;
imgAsset sourceImg;
imgAsset grayImg;
imgAsset cannyImg;
imgAsset resultImg;
vector<vector<cv::Point> > contours;
vector<cv::Vec4i> hierarchy;
vector<int> hierarchyDepths;
int maxHierarchyDepth = 0;
vector<vector<cv::Point>>  polygons;
vector<vector<curve>> traceCurves;
vector<vector<curve>> traceInfillLoops;
vector<vector<vec2>> sortedLoops;
volatile float traceSortProgress = 0;
volatile int traceSortProgressState = 0;
int tracePreviewCount = 0;
int traceLineCount = 0;
thread sortThread;

cv::Point sortTarget = cv::Point(0, 0);

void sortThreadFunc(vector<vector<vec2>> unsortedLoops) {
	sortedLoops = getOptimizedPointOrder(unsortedLoops, &traceSortProgress, &traceSortProgressState);
	sorted = true;

	float scaler = traceWidth / traceMaxX;

	//get flip y
	for (int i = 0; i < polygons.size() && i < 1000; i++)
		for (int j = 0; j < polygons[i].size() && j < 1000; j++)
			//if (hierarchyDepths[i] != maxHierarchyDepth)
			traceMaxY = glm::max((float)polygons[i][j].y * scaler, traceMaxY);

	for (int i = 0; i < sortedLoops.size(); i++)
	{
		if (sortedLoops[i].size() < 3)
			continue;
		vector<curve> loop;

		for (int j = 1; j < sortedLoops[i].size() && j < 1000; j++) {
			curve line = cLine(sortedLoops[i][j - 1], sortedLoops[i][j]) * scaler;
			for (int l = 0; l < 3; l++)
				line.points[l].y = traceMaxY - line.points[l].y;
			loop.push_back(line);
		}
		//close the loop
		{
			curve line = cLine(sortedLoops[i][sortedLoops[i].size() - 1], sortedLoops[i][0]) * scaler;
			for (int l = 0; l < 3; l++)
				line.points[l].y = traceMaxY - line.points[l].y;
			loop.push_back(line);
		}
		traceCurves.push_back(loop);
	}

	vector<curve> flat;

	for (int i = 0; i < traceCurves.size(); i++)
	{
		for (int j = 0; j < traceCurves[i].size(); j++)
		{
			flat.push_back(traceCurves[i][j]);
		}
	}

	auto infill = getRectillinearInfill(flat, 1.0, PI / 4);
	for (int i = 0; i < infill.size(); i++)
	{
		traceCurves.push_back(infill[i]);
	}


}


void vectorTraceWindow() {
	Columns(2);
	InputText("Image Filepath", picPathBuff, IM_ARRAYSIZE(picPathBuff));
	if (Button("Trace Image")) {
		cvsourceImg = imread(picPathBuff);
		sourceImg = imgAsset(cvsourceImg);
		cvtColor(cvsourceImg, cvsourceImgGray, CV_BGR2GRAY);
		blur(cvsourceImgGray, cvsourceImgGray, Size(3, 3));

		picLoaded = true;
		updateResult = true;
		traceWindow = true;
	}
	SameLine();
	bool generate = Button("Generate");


	if (Button("temp")) {
		showGenerateScriptWindow = true;
		scriptGenTarget = &traceCurves;
	}


	if (generate && contoursCreated) {
		traceCurves.clear();

		//get scale factor
		for (int i = 0; i < polygons.size() && i < 1000; i++) {
			if (hierarchyDepths[i] != maxHierarchyDepth)
				for (int j = 0; j < polygons[i].size() && j < 1000; j++)
					traceMaxX = glm::max((float)polygons[i][j].x, traceMaxX);
			else
				polygons.erase(polygons.begin() + i);
		}

		vector<vector<vec2>> unsortedLoops;
		for (int i = 0; i < polygons.size(); i++)
		{
			vector<vec2> ar;
			if (polygons[i].size() < 3)
				continue;
			for (int j = 0; j < polygons[i].size(); j++)
			{
				ar.push_back(vec2(polygons[i][j].x, polygons[i][j].y));
			}
			unsortedLoops.push_back(ar);
		}
		sortThread = thread(sortThreadFunc, unsortedLoops);
		sorted = false;
	}

	updatePolys |= SliderFloat("Epsilon", &pEpsilon, 0.5, 10);
	updatePolys |= SliderFloat("Infill spacing", &traceInfillWidth, 0.01, 10);
	SameLine();
	Checkbox("Use infill", &traceInfill);

	NextColumn();
	Text("Displaying %d/%d lines", displayedLines, totalLines);
	if (traceSortProgressState == 0)
		Text("Sorting %d loops", totalLoops);
	else if (traceSortProgressState == 1)
		Text("Creating optimized path");
	else if (traceSortProgressState == 2)
		Text("Finished");
	ProgressBar(traceSortProgress);
	Columns(1);
	SliderInt("Preview", &tracePreviewCount, 0, traceLineCount);

	if (traceWindow)
		traceSettingsWindow();

	if (contoursCreated) {
		if (!polysCreated || updatePolys) {
			for (int i = 0; i < polygons.size(); i++)
				polygons[i].clear();
			polygons.clear();
			traceLineCount = 0;

			for (int i = 0; i < contours.size(); i++)
			{
				vector<cv::Point> points;
				approxPolyDP(contours[i], points, pEpsilon, true);
				polygons.push_back(points);
				traceLineCount += contours[i].size() / 2;
			}

			if (traceInfill) {
				vector<vector<curve>> outline;
				for (int i = 1; i < polygons.size(); i++)
				{
					vector<curve> ar;
					if (polygons[i].size() < 3)
						continue;
					vec2 first(polygons[i][0].x, polygons[i][0].y);
					vec2 last = first;
					vec2 next;
					for (int j = 0; j < polygons[i].size(); j++)
					{
						next = vec2(polygons[i][j].x, polygons[i][j].y);
						ar.push_back(cLine(last, next));
						last = next;
					}
					ar.push_back(cLine(first, last));
					outline.push_back(ar);
					traceLineCount += ar.size();
				}
				traceInfillLoops = getRectillinearInfill(flatten(outline), traceInfillWidth, PI / 4);
			}

			polysCreated = true;
			updatePolys = false;
		}

		//draw stuff
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		vec2 canPos = ImGui::GetCursorScreenPos();
		vec2 mpos = ImVec2(ImGui::GetIO().MousePos.x - canPos.x, ImGui::GetIO().MousePos.y - canPos.y);

		//panning
		vec2 delta = prevMousePos - mpos;
		prevMousePos = mpos;
		if (IsMouseClicked(1))
			delta = vec2(0);
		if (IsMouseDown(1))
			CanvasPan -= delta;

		ImVec2 canvas_size = ImGui::GetContentRegionAvail();
		if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
		if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;
		drawList->AddRectFilledMultiColor(canPos, ImVec2(canPos.x + canvas_size.x, canPos.y + canvas_size.y), IM_COL32(50, 50, 50, 255), IM_COL32(50, 50, 60, 255), IM_COL32(60, 60, 70, 255), IM_COL32(50, 50, 60, 255));
		drawList->AddRect(canPos, ImVec2(canPos.x + canvas_size.x, canPos.y + canvas_size.y), white);

		ImGui::InvisibleButton("canvas", canvas_size);
		mpos = ImVec2(ImGui::GetIO().MousePos.x - canPos.x, ImGui::GetIO().MousePos.y - canPos.y);

		drawList->PushClipRect(canPos, ImVec2(canPos.x + canvas_size.x, canPos.y + canvas_size.y), true);

		//draw contours with imgui
		displayedLines = 0;
		totalLines = 0;

		for (int i = 0; i < polygons.size(); i++)
		{

			float hue = hierarchyDepths[i] / (float)maxHierarchyDepth;
			ImColor col{ 1.0, hue, hue, 1.0f };
			if (polygons[i].size() < 3 || hierarchyDepths[i] == maxHierarchyDepth)
				col = black;

			for (int j = 1; j < polygons[i].size() && totalLines < tracePreviewCount; j++) {
				totalLines++;
				vec2 testPoint = vec2(polygons[i][j].x, polygons[i][j].y) * canvasZoomAmount/* * canvasZoomAmount*/;
				if (testPoint.x < canPos.x - CanvasPan.x + canvas_size.x &&
					testPoint.x > canPos.x - CanvasPan.x - canvas_size.x &&
					testPoint.y < canPos.y - CanvasPan.y + canvas_size.y &&
					testPoint.y > canPos.y - CanvasPan.y - canvas_size.y)
				{

				}
				else
					continue;

				curve line = cLine(vec2(polygons[i][j - 1].x, polygons[i][j - 1].y), vec2(polygons[i][j].x, polygons[i][j].y));
				drawLine(line * canvasZoomAmount + CanvasPan, drawList, canPos, col);
				displayedLines++;
			}
			{
				curve line = cLine(vec2(polygons[i][0].x, polygons[i][0].y), vec2(polygons[i][polygons[i].size() - 1].x, polygons[i][polygons[i].size() - 1].y));
				if (totalLines < tracePreviewCount)
					drawLine(line * canvasZoomAmount + CanvasPan, drawList, canPos, col);
				totalLines++;
			}
			displayedLines++;
			totalLines++;
			sprintf(labelBuff, "%d", hierarchyDepths[i]);
			drawList->AddText(canvasFont, 25, vec2(polygons[i][0].x, polygons[i][0].y) * canvasZoomAmount + canPos + CanvasPan, white, labelBuff);
		}

		for (int i = 0; i < traceInfillLoops.size(); i++) {
			for (int j = 0; j < traceInfillLoops[i].size() && totalLines < tracePreviewCount; j++) {
				drawLine(traceInfillLoops[i][j] * canvasZoomAmount + CanvasPan, drawList, canPos, blue);
				totalLines++;
			}
		}

		drawList->PopClipRect();
	}
}

int hierarchyPlunge(int index, int depth) {
	if (hierarchy[index][2] != -1)
		return hierarchyPlunge(hierarchy[index][2], depth + 1);
	return depth;
}


void traceSettingsWindow() {
	ImGuiWindowFlags window_flags = 0;
	window_flags |= ImGuiWindowFlags_NoCollapse;
	window_flags |= ImGuiWindowFlags_NoNav;
	window_flags |= ImGuiWindowFlags_NoDocking;
	Begin("Tracing settings", &traceWindow, window_flags);
	ImGui::SetWindowFocus();

	updateResult |= SliderInt("Threshold", &thresh, 0, 250);
	SameLine();
	if (Button("Finished")) {
		traceWindow = false;
		contoursCreated = true;
	}
	Text("Contours: %d     Max depth: %d", contourCount, maxHierarchyDepth);

	if (updateResult) {
		contours.clear();
		hierarchy.clear();
		hierarchyDepths.clear();

		// Detect edges using canny
		Canny(cvsourceImgGray, cannyResult, thresh, thresh * 2, 3);
		// Find contours
		//findContours(cannyResult, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));
		findContours(cvsourceImgGray, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));
		contourCount = contours.size();

		int record = 0;
		for (int i = 0; i < hierarchy.size(); i++)
		{
			int depth = hierarchyPlunge(i, 0);
			record = glm::max(record, depth);
			hierarchyDepths.push_back(depth);
		}
		maxHierarchyDepth = record;

		// Draw contours
		Mat drawing = Mat::zeros(cannyResult.size(), CV_8UC3);
		for (int i = 0; i < contours.size(); i++)
		{
			Scalar color = Scalar(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255));
			drawContours(drawing, contours, i, color, 2, 8, hierarchy, 0, Point());
		}
		resultImg = imgAsset(drawing);
		grayImg = imgAsset(cvsourceImgGray);
		cannyImg = imgAsset(cannyResult);
		updateResult = false;
	}
	if (picLoaded) {
		vec2 space = GetContentRegionAvail();
		float ratio = sourceImg.size.y / sourceImg.size.x;
		vec2 ims = sourceImg.size * (space.x / sourceImg.size.x);
		float columRatioA = (2 / 3.0f);
		float columRatioB = (1 / 3.0f);

		Columns(2);
		ImGui::SetColumnWidth(0, space.x * columRatioA);
		Image((ImTextureID)sourceImg.ID, ims * columRatioA);
		NextColumn();
		Image((ImTextureID)grayImg.ID, ims * columRatioB);
		Image((ImTextureID)cannyImg.ID, ims * columRatioB);
		Columns(1);
		Image((ImTextureID)resultImg.ID, ims);
	}
	End();
}
#endif 


char lineArtPicPathBuff[512] = "../data/wavetestA-small.png";
bool showLineArtWindow = false;
bool calculatingParallel = false;
int persistantI = 0;
double calculationTime;
bool lineArtPreProccessed = false;
bool lineArtImageLoaded = false;
uint8_t* waveArtImgData;
//uint8_t ** waveArtAveragedData;
uint8_t* waveArtAveragedData;
bool waveArtAveragedDataInit = false;
SafeQueue<vec4> parallelRenderQue;
vec2 workImSize;
GLuint lineArtID;
bigCanvas waveCan;
GLuint waveArtResult;
int wvProcType = 0;
vector<vector<curve>> waveCurves;
vec4* waveArtBuffer;
bool waveArtBufferInit = false;

int lineArtDiv = 8;
float lineArtBasefrequency = 0.5;
float lineArtfrequency = 1.5;
float lineArtAmplitude = 5;
float lineArtCutoff = 0.05;

bool lineDivChanged = false;
bool reDrawLineArt = false;
bool finishedDrawingWaves = true;

void lineArtWindow() {


	InputText("Image Filepath", lineArtPicPathBuff, IM_ARRAYSIZE(lineArtPicPathBuff));

	//if (Button("Load Image")) {
	//	showLineArtWindow = true;
	//}
	bool loadingImage = Button("Load Image");
	SameLine();

	bool generate = Button("Generate");

	Columns(2);
	lineDivChanged |= SliderInt("Line div", &lineArtDiv, 2, 100);
	reDrawLineArt |= SliderFloat("Base Wave frequency", &lineArtBasefrequency, 0, 1);
	reDrawLineArt |= SliderFloat("Wave frequency darkness coefficient", &lineArtfrequency, 0, 5);
	reDrawLineArt |= SliderFloat("Wave amplitude coefficient", &lineArtAmplitude, 0, 10);
	reDrawLineArt |= SliderFloat("Wave amplitude min threshold", &lineArtCutoff, 0, 1);
	NextColumn();
	reDrawLineArt |= RadioButton("Single threaded", &wvProcType, 0);
	reDrawLineArt |= RadioButton("Multi threaded", &wvProcType, 1);
	reDrawLineArt |= RadioButton("Hardware accelerated", &wvProcType, 2);
	Columns(1);

	if (reDrawLineArt) {
		finishedDrawingWaves = false;
		if (wvProcType == 1)
			waveCan.clearCanvas();
	}

	//load image into memory
	if (loadingImage) {
		if (lineArtImageLoaded)
			stbi_image_free(waveArtImgData);
		int w, h;
		waveArtImgData = rgbaFromFile(lineArtPicPathBuff, &w, &h);
		lineArtImageLoaded = true;
		workImSize = vec2(w, h);
		lineDivChanged = true;
		lineArtPreProccessed = false;
		waveCan = bigCanvas(workImSize, 1);
		waveCan.parallelQue = &parallelRenderQue;
	}

	//proccess average pixel line values if the line division was updated
	if (lineDivChanged && lineArtImageLoaded) {
		lineArtID = getWaveArtAveragedLines(waveArtImgData, (int)workImSize.x, (int)workImSize.y, lineArtDiv, 1);
		lineArtPreProccessed = true;
		reDrawLineArt = true;

		persistantI = 0;
		if (waveArtAveragedDataInit)
			delete waveArtAveragedData;

		uint8_t* lineValues = rgbaFromTexture(lineArtID, (int)workImSize.x, (int)workImSize.y);
		uint8_t* lineValuesGrey = grayscaleFromRed(lineValues, (int)workImSize.x, (int)workImSize.y);

		int w = (int)workImSize.x;
		waveArtAveragedData = new uint8_t[((int)workImSize.y / lineArtDiv + 1) * w];
		waveArtAveragedDataInit = true;
		for (int i = 0; i < workImSize.y / lineArtDiv; i++)
			for (int j = 1; j < workImSize.x; j += 1)
				waveArtAveragedData[i * w + j] = lineValuesGrey[(i * lineArtDiv) * w + j];

		delete lineValues;
		delete lineValuesGrey;

		finishedDrawingWaves = false;
		calculationTime = glfwGetTime();
	}

	//preview rendering
	if (lineArtPreProccessed && !finishedDrawingWaves) {
		if (wvProcType == 0) {
#if 0
			vector<curve> lines = calculateWaveArt((int)workImSize.x, (int)workImSize.y, lineArtDiv, waveArtData, lineArtBasefrequency, lineArtfrequency, lineArtAmplitude, lineArtCutoff, (1000), &persistantI);
			finishedDrawingWaves = !(persistantI < workImSize.y);
			waveCan.addToBuffer(lines);
			double ttime = glfwGetTime();
			waveArtResult = waveCan.render();
			if (finishedDrawingWaves) {
				calculationTime = glfwGetTime() - calculationTime;
				cout << "calculated single core in " << calculationTime << endl;
			}
#else
			if (!waveArtBufferInit) {
				waveArtBuffer = new vec4[400000];
				waveArtBufferInit = true;
			}
			int lineCount = calculateWaveArt(waveArtBuffer, 400000, (int)workImSize.x, (int)workImSize.y, lineArtDiv, waveArtAveragedData, lineArtBasefrequency, lineArtfrequency, lineArtAmplitude, lineArtCutoff);
			waveArtResult = waveCan.renderBatch(waveArtBuffer, lineCount);
			finishedDrawingWaves = true;
#endif
	}

		if (wvProcType == 1) {
			if (!calculatingParallel && reDrawLineArt) {
				calculateWaveArtParalell((int)workImSize.x, (int)workImSize.y, lineArtDiv, waveArtAveragedData, lineArtBasefrequency, lineArtfrequency, lineArtAmplitude, lineArtCutoff, &parallelRenderQue, &remainingJobs);
				calculatingParallel = true;
			}
			if (!remainingJobs && calculatingParallel) {
				calculationTime = glfwGetTime() - calculationTime;
				waveArtJoinThreads();
				calculatingParallel = false;
				cout << "calculated multi core in " << calculationTime << endl;
			}
			waveArtResult = waveCan.render((1.0 / 16));
			finishedDrawingWaves = !waveCan.queLength();
		}
		if (wvProcType == 2) {
			waveArtResult = renderWavesAsEquation(waveArtAveragedData, (int)workImSize.x, (int)workImSize.y, lineArtDiv, lineArtBasefrequency, lineArtfrequency, lineArtAmplitude, lineArtCutoff, lineDivChanged);
			finishedDrawingWaves = true;
		}
		reDrawLineArt = false;
		lineDivChanged = false;
}

	if (lineArtPreProccessed)
		Image((ImTextureID)waveArtResult, workImSize);
	vec2 offset(0, 0);
	if (lineArtPreProccessed && generate) {
		vector<vector<curve>> ccurves;
		float scalefact = 170.0f / workImSize.x;

		for (int i = 0; i < workImSize.y; i += lineArtDiv)
		{
			//bool firstinrow = true;
			vec2 lastp(0, i);
			bool skip = false;
			float graphX = 0;
			vector<curve> loop;

			for (int j = 1; j < workImSize.x; j += 1)
			{
				uchar pix = waveArtAveragedData[(i / lineArtDiv) * (int)workImSize.x + j];// lineArtCV.at<uchar>(i, j);

				float darkness = (255.0f - pix) / 255.0f;

				if (darkness < lineArtCutoff) {
					darkness = 0.0f;
					if (j > 1) {
						skip = true;
						continue;
					}
				}

				if (skip) {
					curve line = cLine(vec2(lastp.x, workImSize.y - lastp.y), vec2(j - 1, workImSize.y - lastp.y)) * scalefact;
					loop.push_back(line + offset);
					lastp = line.end / scalefact;
				}

				graphX += darkness * lineArtfrequency;
				vec2 nextp(j, i + cos((j + graphX) * lineArtBasefrequency) * darkness * lineArtAmplitude);

				curve line = cLine(vec2(lastp.x, workImSize.y - lastp.y), vec2(nextp.x, workImSize.y - nextp.y)) * scalefact;
				loop.push_back(line + offset);
				lastp = nextp;
				skip = false;
			}
			curve line = cLine(vec2(lastp.x, workImSize.y - i), vec2(workImSize.x, workImSize.y - i)) * scalefact;
			loop.push_back(line + offset);

			ccurves.push_back(loop);
		}
		waveCurves = ccurves;
		showGenerateScriptWindow = true;
		scriptGenTarget = &waveCurves;
			//auto commands = createScript(ccurves, 1900);
			//saveScript(commands);
	}
}

char ditherArtPicPathBuff[1024] = "../data/hashimSquare.png";
vector<vector<curve>> ditherCurves;
GLuint ditheredTexture;
GLuint grayTexture;
uint8_t* ditherGrayData;
bool ditheredTextureReady = false;
vec2 ditheredImgSize;
int ditherSteps = 8;
bool updateDither = false;
bool ditherImgLoaded = false;
float brightnessOffset = 0;
bool ditherAccel = true;
float ditherDownScale = 0.163;
float ditherThresh = 1.0;

void ditheringWindow() {
	InputText("Image Filepath", ditherArtPicPathBuff, IM_ARRAYSIZE(ditherArtPicPathBuff));

	float finalWidth = 160;
	if (Button("Load")) {

		int w, h;
		uint8_t* img = NULL;
		img = rgbaFromFile(ditherArtPicPathBuff, &w, &h);
		ditherGrayData = grayscaleFromLuminance(img, w, h);
		grayTexture = textureFromGrayscale(ditherGrayData, w, h);


		ditheredImgSize = vec2(w, h);
		updateDither = true;

		ditherImgLoaded = true;
	}

	if (Button("Generate") && ditherImgLoaded && ditheredTextureReady) {
		int w = (int)ditheredImgSize.x;
		int h = (int)ditheredImgSize.y;
		cv::Mat pix = loadTexture(ditheredTexture, w, h);
		//cv::imshow("test", pix);
		float stp = w * ditherDownScale;

		float spacing = w / stp;
		float scaleFact = 1;//finalWidth / (w * spacing);

		for (float i = 0; i < h; i += spacing)
		{
			for (float j = 0; j < w; j += spacing)
			{
				float val = pix.at<uchar>((int)j, (int)i);

				val = (255.0f - val) / 255.0f;
				float rad = spacing * val;
				cout << val << endl;

				if (val > 0.05)
					ditherCurves.push_back(makeCirc(i * scaleFact + 30, j * scaleFact + 30, (rad * scaleFact) / 2));
			}
		}

		scriptGenTarget = &ditherCurves;
		showGenerateScriptWindow = true;
	}

	updateDither |= SliderInt("Steps", &ditherSteps, 1, 8);
	updateDither |= SliderFloat("Brightness Offset", &brightnessOffset, -1, 1);
	updateDither |= SliderFloat("Down scale", &ditherDownScale, 0, 1);
	Checkbox("Hardware Accelerated", &ditherAccel);

	if (updateDither && ditherImgLoaded) {
		if (ditherAccel) {
			ditheredTexture = ditherAccelerated(grayTexture, (int)ditheredImgSize.x, (int)ditheredImgSize.y, ditherSteps, brightnessOffset, ditherDownScale);
		}
		else {
			simpleDither(ditherGrayData, (int)ditheredImgSize.x, (int)ditheredImgSize.y);
			ditheredTexture = textureFromGrayscale(ditherGrayData, (int)ditheredImgSize.x, (int)ditheredImgSize.y);
		}
		updateDither = false;
		ditheredTextureReady = true;
	}

	if (ditheredTextureReady)
		Image((ImTextureID)ditheredTexture, ditheredImgSize);
}


char spiralArtPicPathBuff[1024] = "../data/NathanSquare.png";
GLuint spiralCanID;
bigCanvas spiralCan;
//vector<vector<curve>> ditherCurves;
//GLuint ditheredTexture;
//GLuint grayTexture;
//uint8_t * ditherGrayData;
//bool ditheredTextureReady = false;
//vec2 ditheredImgSize;
//int ditherSteps = 8;
//bool updateDither = false;
//bool ditherImgLoaded = false;
//float brightnessOffset = 0;
//bool ditherAccel = true;
//float ditherDownScale = 0.163;
//float ditherThresh = 1.0;

float sprialLoops = 60;
float sprialBasefrequency = 1.0;
float sprialfrequency = 1.5;
float sprialAmplitude = 5;
float sprialCutoff = 0.05;
int simSize = 0;
bool spiralPrevReady = false;
bool reDrawSpiral = false;
vector<vector<curve>> spiralCurves;
uint8_t* spiralImageData = NULL;
vec4* spiralTransformData;
GLuint obj_view;

void spiralWindow() {
	Columns(2);
	InputText("", spiralArtPicPathBuff, IM_ARRAYSIZE(spiralArtPicPathBuff));
	SameLine();
	if (Button("Browse"))
		showWindowsOpenFileDialog(spiralArtPicPathBuff);


	float finalWidth = 160;
	bool loadFile = Button("Load");
	SameLine();
	bool generate = Button("Generate Script");


	NextColumn();
	reDrawSpiral |= SliderFloat("Circle Count", &sprialLoops, 2, 100);
	reDrawSpiral |= SliderFloat("Base Wave frequency", &sprialBasefrequency, 0, 10);
	reDrawSpiral |= SliderFloat("Wave frequency darkness coefficient", &sprialfrequency, 0, 10);
	reDrawSpiral |= SliderFloat("Wave amplitude coefficient", &sprialAmplitude, 0, 10);
	reDrawSpiral |= SliderFloat("Wave amplitude min threshold", &sprialCutoff, 0, 1);
	Columns(1);
	Separator();

	if (loadFile) {

		//previous image already loaded
		if (spiralPrevReady) {
			delete spiralImageData;
			glDeleteFramebuffers(1, &spiralCanID);
			//spiralCan.clearCanvas();
		}
		else
			spiralTransformData = new vec4[200000];

		spiralPrevReady = false;
		int w, h;
		uint8_t* original = NULL;
		original = rgbaFromFile(spiralArtPicPathBuff, &w, &h);
		//volatile int temp = original[0];
		spiralImageData = grayscaleFromRed(original, w, h);
		stbi_image_free(original);
		simSize = w;
		spiralCan = bigCanvas(vec2(w), 2.0f);
		reDrawSpiral = true;
	}


	if (reDrawSpiral || generate) {
		spiralCurves.clear();
		vector<curve> loop;


		float gap = simSize / sprialLoops;
		float step = 1;
		float lastAngle = sprialLoops * PI * 2;

		vec2 offset(0, 150);
		float radius = 0;
		float angle = 0;
		float accumulator = 0;
		int i = 0;
		vec2 prev(simSize / 2);
		while (true) {
			vec2 point;
			float x, y;

			if (radius >= simSize / 2)
				break;
			radius = angle * ((simSize / 2) / lastAngle);
			angle += atan(step / radius);

			x = cos(angle) * radius + simSize / 2;
			y = sin(angle) * radius + simSize / 2;

			uint8_t pix = spiralImageData[simSize * (int)y + (int)x];
			float darkness = (255.0f - pix) / 255.0f;
			if (darkness < sprialCutoff)
				darkness = 0;
			accumulator += darkness * sprialfrequency;
			radius += cos((i + accumulator) * sprialBasefrequency) * darkness * sprialAmplitude;

			x = cos(angle) * radius + simSize / 2;
			y = sin(angle) * radius + simSize / 2;

			point = vec2(x, y);

			spiralTransformData[i] = packedLine(prev, point);

			if (generate)
				loop.push_back(cLine(vec2(prev.x * 0.261f, 170 - prev.y * 0.261f), vec2(point.x * 0.261f, 170 - point.y * 0.261f)) + 10 + offset);

			prev = point;
			i++;
		}
		spiralPrevReady = true;
		spiralCurves.push_back(loop);
		spiralCanID = spiralCan.renderBatch(spiralTransformData, i);

		reDrawSpiral = false;
	}

	if (generate) {
		scriptGenTarget = &spiralCurves;
		showGenerateScriptWindow = true;
	}



	//	if (ditheredTextureReady)
	if (spiralPrevReady)
		Image((ImTextureID)spiralCanID, vec2(GetContentRegionAvail().y));
}


#include<glm/trackball.h>
char objPathBuff[1024] = "../data/donut.obj";
bool objLoaded = false;
bigCanvas objCan;
GLuint objLineID;
int objLineCount = 80;
float objLineDiv;
vec4* objCanLines;
bool objLinesInit = false;
vec2 lastMouse;
float mouseSensitivity = 0.1;
float objRotation[4];
mat4 objQuaternion;
float objScaleX = 1, objScaleY = 1, objScaleZ = 1;
float objIncAngle = 0;
bool animX = false, animY = false, animZ = false;
vector<vector<curve>> objLoops;
float objDrawWidth = 300;
float modelAngleX = PI / 2, modelAngleY, modelAngleZ;
float depthNear = 0.1, depthFar = 10, depthSensitivity = 0.3;
float obj_lightpow = 1.25;
bool objShowDepth = false;

void objWindow() {

	vec2 canPos = ImGui::GetCursorScreenPos();
	vec2 mpos = ImVec2(ImGui::GetIO().MousePos.x - canPos.x, ImGui::GetIO().MousePos.y - canPos.y);

	vec2 delta = (lastMouse - mpos) * mouseSensitivity;

	Columns(4);
	InputText("Image Filepath", objPathBuff, IM_ARRAYSIZE(objPathBuff));
	SliderInt("Line Count", &objLineCount, 10, 200);
	bool loadFile = Button("Load");
	SameLine();
	bool generate = Button("Generate");
	NextColumn();
	InputFloat("X Rotation", &modelAngleX);
	SameLine();
	Checkbox("Animate", &animX);
	InputFloat("Y Rotation", &modelAngleY);
	SameLine();
	Checkbox("Animadte", &animY);
	InputFloat("Z Rotation", &modelAngleZ);
	SameLine();
	Checkbox("Animdate", &animZ);
	NextColumn();
	InputFloat("Scale", &objScaleX);
	Checkbox("Show Depth", &objShowDepth);
	//InputFloat("X Scale", &objScaleX);
	//InputFloat("Y Scale", &objScaleY);
	//InputFloat("Z Scale", &objScaleZ);
	NextColumn();
	SliderFloat("Near", &depthNear, 0, 0.2);
	SliderFloat("Far", &depthFar, 0, 5);
	SliderFloat("Sensitivity", &depthSensitivity, 0, 5);
	SliderFloat("light", &obj_lightpow, 0, 10);
	Columns(1);


	if (IsMouseDown(1) && lastMouse != mpos)
	{
		vec2 mpP = vec2(lastMouse.x / 800 * 2 - 1, lastMouse.y / 800 * 2 - 1);
		vec2 mpN = vec2(mpos.x / 800 * 2 - 1, mpos.y / 800 * 2 - 1);
		cout << "\r" << "X: " << mpN.x << " Y: " << mpN.y;




		//float nRotation[4];
		//trackball(nRotation, 0, 0, mpN.x, mpN.y);

		//objRotation[0] = nRotation[0];
		//objRotation[1] = nRotation[1];
		//objRotation[2] = nRotation[2];
		//objRotation[3] = nRotation[3];

		//objQuaternion = quat(objRotation[4], objRotation[1], objRotation[2], objRotation[0]);

		//objEular = glm::rotateX(objEular, delta.x);
		//objEular = glm::rotateY(objEular, delta.y);
	//	objQuaternion = glm::rotate( objEular;
	}

	lastMouse = mpos;


	if (loadFile) {

		obj_view = renderModelDepth(800, 800, window, objQuaternion, vec3(objScaleX), obj_lightpow);
		//obj_view = renderModel(800, 800, window, objQuaternion, vec3(objScaleX), obj_lightpow);
		objCan = bigCanvas(vec2(800), 3);
		objLoaded = true;
		if (!objLinesInit)
		{
			objCanLines = new vec4[400000];
			objLinesInit = true;
		}
	}


	if (objLoaded) {
		objQuaternion = mat4(1);

		objIncAngle += 0.01;
		objQuaternion = glm::rotate(objQuaternion, modelAngleX + (animX ? objIncAngle : 0.0f), vec3(1, 0, 0));
		objQuaternion = glm::rotate(objQuaternion, modelAngleY + (animY ? objIncAngle : 0.0f), vec3(0, 1, 0));
		objQuaternion = glm::rotate(objQuaternion, modelAngleZ + (animZ ? objIncAngle : 0.0f), vec3(0, 0, 1));


		obj_view = renderModelDepth(800, 800, window, objQuaternion, vec3(objScaleX), obj_lightpow);
		if (!objShowDepth) {
			GLuint tt = renderModel(800, 800, window, objQuaternion, vec3(objScaleX), obj_lightpow);
			Image((ImTextureID)tt, vec2(800));
		}
		else
			Image((ImTextureID)obj_view, vec2(800));

		uint8_t* renderData = rgbaFromTexture(obj_view, 800, 800);
		uint8_t* renderGrey = grayscaleFromRed(renderData, 800, 800);
		delete renderData;

		int w = 800, h = 800;
		objLineDiv = h / objLineCount;
		float scaleFact = objDrawWidth / w;
		if (generate)
			objLoops.clear();
		int canIndex = 0;
		float* bendRecords = new float[w];

		for (int i = 0; i < h; i += objLineDiv)
		{
			vector<curve> loop;
			vec2 prev(0, i);
			bool skip = false;

			loop.push_back(cLine(prev, prev) * scaleFact);

			for (int j = 1; j < w; j++)
			{
				uint8_t pix = renderGrey[i * w + j];

				float x = j;
				float y = i + pix * depthSensitivity;

				if (i == 0 || y > bendRecords[j])
					bendRecords[j] = y;

				else if (y < bendRecords[j]) {
					if (skip) {
						prev = vec2(x, y);
						continue;
					}
					else {
						prev = vec2(x, y);
						continue;
					}
				}


				if (generate) {
					if (y == prev.y) {
						skip = true;
						continue;
					}
					if (skip) {
						loop.push_back(cLine(prev, vec2(j - 1, prev.y)) * scaleFact);
						prev = vec2(j - 1, prev.y);
					}
				}
				else {
					if (y == prev.y) {
						//		skip = true;
							//	continue;
					}
					if (skip) {
						objCanLines[canIndex] = packedLine(cLine(prev, vec2(j - 1, prev.y)));
						canIndex++;
						prev = vec2(j - 1, prev.y);
					}
				}

				vec2 next(x, y);
				objCanLines[canIndex] = packedLine(prev, next);
				canIndex++;

				prev = next;


				if (generate)
					loop.push_back(cLine(prev, next) * scaleFact);
				skip = false;
			}


			if (generate) {
				loop.push_back(cLine(prev, vec2(w, prev.y)) * scaleFact);
				objLoops.push_back(loop);
			}
			else {
				objCanLines[canIndex] = packedLine(cLine(prev, vec2(w, prev.y)));
				canIndex++;
			}
		}

		if (generate) {
			scriptGenTarget = &objLoops;
			showGenerateScriptWindow = true;

		}

		delete renderGrey;
		delete[] bendRecords;
		///		delete data;
		objLineID = objCan.renderBatch(objCanLines, canIndex);
		SameLine();
		Image((ImTextureID)objLineID, vec2(800));
	}
}

vec2 rotatePoint(vec2 point, float angle, vec2 around) {
	mat2 rot(
		cos(angle), -sin(angle),
		sin(angle), cos(angle)
	);
	point = (point - around) * rot + around;
	return point;
}

#ifndef __TESSERACT_INSTALLED__
void letterRecognitionWindow() {
	Text("\n\
		You are seeing this message because OpenCV text module is not installed or was not found.\n\
		Please check your source includes for OpenCV contrib modules before recompiling.\n\
		The features of this window will become available when OpenCV text is successfully compiled.");
}
#else
using namespace cv;

Mat cimg;

bool loaded = false;
char predictionBuffer[64];


vector<Mat> crops;
vector<GLuint> cropIDs;
vector<char> OCRresults;



int xCells;
int yCells;
int gridTop;
int gridBottom;
int gridLeft;
int gridRight;

int cropIndex = 1;
int gridPadding = 16;

//temp
bool searched = false;
GLuint searchID;
int simw, simh;
vector<wordLocation> foundWords;
std::vector<std::string> searchList;
vec4 searchCrop;
int scols, srows;
vec2 cellSize;
curve nameLine, dateLine;
char dateStr[16];
float puzzleAngle;
vector<vector<curve>> physicalSolution;

void letterRecognitionWindow() {

	SliderInt("tesmptt", &cropIndex, 1, 100);
	bool load = Button("Load");
	loaded |= load;


	if (load) {


		//Mat temp = imread("C:/Users/Micha/Documents/capstone media/naitparking.jpg");
		//Mat out;
		//Canny(temp, out, 50, 200, 3);
		//imshow("canny", out);

		/*cimg = imread("../data/Grid.bmp", IMREAD_COLOR);

		gridDetect(1000, 1000, cimg,
			&cellSize,
			&xCells,
			&yCells,
			&gridTop,
			&gridBottom,
			&gridLeft,
			&gridRight, false);

		for (int i = 0; i < 5; i++)
		{
			for (int j = 0; j < 10; j++)
			{
				Mat crop;
				cimg(Rect(gridLeft + cellSize * j + gridPadding, gridBottom + cellSize * i + gridPadding, cellSize - gridPadding, cellSize - gridPadding)).copyTo(crop);
				char result = recognizeLetter(crop);
				OCRresults.push_back(result);
				crops.push_back(crop);

				GLuint id;
				loadTexture(crop, &id, crop.cols, crop.rows);
				cropIDs.push_back(id);
			}*/
			//}
	}
	if (loaded) {
		//Image((ImTextureID)cropIDs[cropIndex - 1], vec2(cellSize - gridPadding * 2));
		//Text("DDetected letter %c", OCRresults[cropIndex-1]);
	}

	bool startSearch = Button("Search");
	bool drawSolution = Button("Draw Solution");

	if (startSearch) {
		cout << "test";
		Mat searchim;
#if 1
		Mat orig = loadTexture(scanTexture, 2300, 2300);
		Mat crop, temp;
		orig(Rect(250, 250, 1800, 1800)).copyTo(crop);
		resize(crop, temp, cv::Size(), stitchAspect * 1.0, 1.0);
		imwrite("../data/ocr/workIm.PNG", temp);
		searchim = imread("../data/ocr/workIm.PNG", IMREAD_COLOR);
		loadTexture(searchim, &searchID, searchim.cols, searchim.rows);
		simw = searchim.cols;
		simh = searchim.rows;

#else
		searchim = imread("../data/ocr/wordserachL.PNG", IMREAD_COLOR);		
		loadTexture("../data/ocr/wordserachL.PNG", &searchID, &simw, &simh);
#endif
			bool status = solveWordSearch(searchim, &foundWords, &searchList, &searchCrop, &scols, &srows, &cellSize, &nameLine, &dateLine, &puzzleAngle, true);
		if (!status)
			cout << "Word search solving failed :(" << endl;

		searched = true;

		time_t t = time(0);   // get time now
		struct tm* now = localtime(&t);


		sprintf(dateStr, " %d/%d/%d", now->tm_mon + 1, now->tm_mday, now->tm_year + 1900);
	}
	if (searched) {

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		vec2 canPos = ImGui::GetCursorScreenPos();
		vec2 canSize = GetContentRegionAvail();

		Image((ImTextureID)searchID, vec2(simw, simh));
		vec2 cent = canPos + vec2(simw, simh) / 2.0f;

		vec2 offset = vec2(searchCrop.x, searchCrop.y) + cellSize / 4.0f;
		for (int i = 0; i < srows; i++)	
			for (int j = 0; j < scols; j++)		
				drawList->AddCircle(rotatePoint(vec2(i, j) * cellSize + offset + canPos, -puzzleAngle, cent), 10, red, 4);
			
		//drawList->AddLine(nameLine.start + canPos, nameLine.end + canPos, red, 3);
		//drawList->AddLine(dateLine.start + canPos, dateLine.end + canPos, red, 3);

		drawList->AddText(canvasFont, 40, rotatePoint(nameLine.start + canPos + vec2(0, -37), -puzzleAngle, cent), red, "  Robot");
		drawList->AddText(canvasFont, 40, rotatePoint(dateLine.start + canPos + vec2(0, -37), -puzzleAngle, cent), red, dateStr);

		for (int i = 0; i < foundWords.size(); i++)
		{
			auto word = foundWords[i];
			if (word.directionType == -1)
				continue;

			vec2 lineStart = vec2(word.x, word.y) * cellSize + offset + canPos;
			vec2 lineEnd = vec2(word.x + word.direction().x * (searchList[i].length() - 1), word.y + -word.direction().y * (searchList[i].length() - 1)) * cellSize + offset + canPos;


			lineStart = rotatePoint(lineStart, -puzzleAngle, cent);
			lineEnd = rotatePoint(lineEnd, -puzzleAngle, cent);
			if (drawSolution) {
				vector<curve> loop;

				vec2 physicalStart = mapScannedPixelToSurfaceLocation((lineStart - canPos) / vec2(stitchAspect, 1));
				vec2 physicalEnd= mapScannedPixelToSurfaceLocation((lineEnd - canPos) / vec2(stitchAspect, 1));

				loop.push_back(cLine(physicalStart, physicalEnd));
				physicalSolution.push_back(loop);
			}
			drawList->AddLine(lineStart, lineEnd, red, 6);
		}

		if (drawSolution) {
			auto script = createScript(physicalSolution, 1000);
			for (int i = 0; i < script.size(); i++)			
				sendCommand(script[i]);
			
		}
	}
}
#endif // __OPENCV_TEXT_INSTALLED__


