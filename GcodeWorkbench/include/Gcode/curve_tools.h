#pragma once
#include <glm/glm.hpp>
#include <vector>

//using namespace std;
using namespace glm;

#ifndef pen_up_cmd
#define pen_up_cmd "M42 P60 S0"
#endif 
#ifndef pen_down_cmd
#define pen_down_cmd "M42 P60 S1"
#endif


typedef highp_dvec2 double2;

#define HIGH_PRECISION

#ifdef HIGH_PRECISION
typedef double cfloat;
#else
typedef float cfloat;
#endif

#define loopList std::vector<std::vector<curve>>
#define drawLoop std::vector<curve>

//Versitile struct which is compoatible between many fucntions and can be converted to G-Code.
//can hold a straight line, an arc, and a quadratic bezier
//types: 1 = line, 2 = clockwise arc, 3 = counter clockwise arc, 4 = bezier
struct curve {
	union {
		vec2 points[3];
		int extra[1]; //only required to generate Gcode (for clockwise vs counter	);

		//for bezier 3
		struct {
			vec2 p0, p1, p2;
			int type;
		};

		//for lines and arcs
		struct {
			vec2 start, end, center;
			int type;
		};
	};
};

bool operator < (curve &a, curve &b);

bool operator == (curve &a, curve &b);
bool operator != (curve &a, curve &b);
curve operator + (curve a, curve b);
curve operator - (curve a, curve b);
curve operator += (curve &a, curve b);
curve operator -= (curve &a, curve b);
curve operator * (curve a, curve b);
curve operator / (curve a, curve b);
curve operator *= (curve &a, curve b);
curve operator /= (curve &a, curve b);

//all four operator overloads for both float and vec2 are typed the exact same, so this macro simplifies that.
#define autoOP(otherType, op)\
	inline curve operator op (curve a, otherType b) {\
		curve c{ a.points[0] op b, a.points[1] op b, a.points[2] op b };\
		c.type = a.type; return c;};\
	inline curve operator op= (curve &a, otherType b) {\
		a = curve(a op b); return a;}

autoOP(float, +);
autoOP(float, -);
autoOP(float, *);
autoOP(float, / );
autoOP(glm::vec2, +);
autoOP(glm::vec2, -);
autoOP(glm::vec2, *);
autoOP(glm::vec2, / );

curve cLine(vec2 Start, vec2 End);
curve cLineFromAngle(vec2 Start, float angle, float length);
curve cBezier(vec2 p0, vec2 p1, vec2 p2);
void getGcode(char buffer[64], curve c, float feedRate = 4000, bool startOfLine = false);
void dwell(char buffer[64], int millis);
vec4 packedLine(curve c);
vec4 packedLine(vec2 A, vec2 B);
float lineLength(curve c);
curve longestLine(std::vector<curve> curves);
curve flipLine(curve c);
//Used to force std::vectors to keep store char arrays
struct Gcommand {
	char cmd[64];
	Gcommand() {};
	Gcommand(char * data) {
		_memccpy(cmd, data, 64, sizeof cmd);
	};
	Gcommand(const char * data) {
		_memccpy(cmd, data, 64, sizeof cmd);
	};
};
std::vector<Gcommand> createScript(std::vector<std::vector<curve>> curves, int feed = 3000, int travelFeed = 4500, int dwellLenth = 150, float * progress = NULL);

//defines a cubic bezier - mostly used for font calculations
typedef struct {
	union {
		vec2 v[4];
		struct { vec2 p0, p1, p2, p3; };
		struct { vec2 P0, C0, C1, P1; };
	};
} bezier4;

//defines a quadratic bezier - mostly used for font calculations
typedef struct {
	union {
		vec2 v[3];
		struct { vec2 p0, p1, p2; };
		struct { vec2 P0, C0, P1; };
	};
} bezier3;
curve fromBezier3(bezier3 b3);

//a letter built from lines and beziers. Organized, raw data from from stbtt
struct bezierLetter {
	bezier3 beziers[50];
	int types[50];
	int count = 0;
	char letter = 'c';
	int xAdvance = 0;
};

//minimum information to represent an arc. easy to convert to gcode
struct simpleArc {
	float radius;
	bool clockwise;
	vec2 start, end;
};

struct indexRange {
	int a, b;
};

struct corner {
	int Index;
	float radius;
	float angle;
	vec2 chordCentre;
};

//an arc with lots of extra data created during the aproximation process 
//when converting from a bezier. The data is useful when rendering the arc with imgui
struct detailedArc
{
	float radius;
	float minAngle, maxAngle;
	double determinant;

	vec2 A0, A1, B0, B1, C0, C1;
	float angleA, angleB, angleC;
	vec2 start, end;

	vec2 curveMiddle;
	vec2 intersectAB;
};
curve fromArc(detailedArc arc);

//only meant to be used within this file for rectilinear infill
struct linkedLineSegment {
	curve seg;
	linkedLineSegment * next = NULL;
	linkedLineSegment * last = NULL;
	linkedLineSegment * left = NULL;
	linkedLineSegment * right = NULL;
	int leftNeighbors = 0;
	int rightNeighbors = 0;
	bool partOfNet = false;
	linkedLineSegment() {}
	linkedLineSegment(curve segment) { seg = segment; }
};
void deleteSegmentsUp(linkedLineSegment * head);
struct rectilinearNetwork {
	linkedLineSegment * endA = NULL;
	linkedLineSegment * endB = NULL;
	int count = 0;
};

template <typename T>
std::vector<T> flatten(std::vector<std::vector<T>> vec) {
	std::vector<T> flat;
	for (int i = 0; i < vec.size(); i++)
		for (int j = 0; j < vec[i].size(); j++)
			flat.push_back(vec[i][j]);
	return flat;
}

//point on a line
vec2 getLinearPoint(float t, vec2 a, vec2 b);

//point on a cubic bezier
vec2 getBezierPoint(float t, bezier3 b);

//point on a quadratic bezier
vec2 getBezierPoint(float t, bezier4 b);

//fully rounded arc from three points / one triangle
detailedArc getRoundedCorner(vec2 A, vec2 C, vec2 B, float t);

//from any 2 points on a line, finds their intersection point (as lone as they are not parallel)
vec2 getLinearIntersection(vec2 A0, vec2 A1, vec2 B0, vec2 B1, double * determinant);
double2 getLinearIntersection(double2 A0, double2 A1, double2 B0, double2 B1, double * determinant);
vec2 getLinearIntersection(curve A, curve B);

//returns true only if the intersection point is actually within the length of both lines
bool getLinearIntersectionTrimmed(vec2 A0, vec2 A1, vec2 B0, vec2 B1, vec2 * intersection);
bool getLinearIntersectionTrimmed(double2 A0, double2 A1, double2 B0, double2 B1, double2 * intersection);

//true if both finite, parallel lines have any overlap
bool checkParallelOverlap(curve A, curve B, float angle, float gap);

//orders points from closest to furthest from a location
void sortByDistance(vec2 location, std::vector<vec2> * points);

//smallest enlosing rectangle with no rotation
void getBoundingBox(std::vector<vec2> points, float * W, float * H, vec2 * center);

bool validateClearPath(curve line, std::vector<curve> obsticles);

//get parallel line, trimmed to fit within a bounding box
std::vector<curve> fillRectWithLines(float rectW, float rectH, vec2 rectCenter, float lineSpacing, float lineAngle, float shrink = 0);

linkedLineSegment * linkSegments(std::vector<std::vector<curve>> trimmedLines, float infillAngle, float infillSpacing);

std::vector<rectilinearNetwork> findNetworks(linkedLineSegment * segments, int listLength);

std::vector<curve> traverseRectillinearNetwork(rectilinearNetwork net, bool reverseDirection = false, bool reverseEnd = false);

//hard to explain. returns the blue lines in this image given the yellow and white lines: https://i.imgur.com/XHtQwm1.png
//results are grouped by subdivision within their original segments
loopList getInfillBooleanOp(std::vector<curve> infilLines, std::vector<curve> polygonLines, float shrink = 0);

loopList getRectillinearInfill(std::vector<curve> polygonLines, float lineSpacing, float lineAngle, float shrink = 0);
loopList optimizeRectillinearInfill(std::vector<rectilinearNetwork> networks, std::vector<curve> outline);

//the acute angle at point B as a triangle with points A and C
float get3PointAngleAcute(vec2 A, vec2 B, vec2 C);

//returns loops sorted in an effiencet draw order
std::vector<std::vector<vec2>> getOptimizedPointOrder(std::vector<std::vector<vec2>> unsorted, volatile float * progress = NULL, volatile int * progressState = NULL);

//re orders loops for least amount of travel
loopList OptimizeDrawPath(loopList drawpath);

//losslessly compresses the drawpath with an equivalent with the lease amount of commands
drawLoop simplifyDrawPath(drawLoop drawpth);

//gets the closest circular arc from a quadratic bezier
detailedArc getArcApproximateDetailed(bezier3 b, float t0, float t1, bool correctVisuals = false);
detailedArc getArcApproximateDetailed(curve c, float t0, float t1, bool correctVisuals = false);
simpleArc getArcApproximateSimple(bezier3 b, float t0, float t1);


