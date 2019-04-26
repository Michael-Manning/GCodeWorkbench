#include"pch.h"
#define _CRT_SECURE_NO_WARNINGS
#include "curve_tools.h"
#include <iostream>
#include <thread>

#define PI 3.14159265359f

using namespace std;

//curve constructors
curve cLine(vec2 Start, vec2 End) {
	curve c{ Start, End };
	c.type = 1;
	return c;
}
curve cLineFromAngle(vec2 Start, float angle, float length) {
	curve c{ Start, vec2(cos(angle), sin(angle)) * length + Start };
	c.type = 1;
	return c;
}
curve cBezier(vec2 p0, vec2 p1, vec2 p2) {
	curve c{ p0, p1, p2 };
	c.type = 4;
	return c;
}
curve fromBezier3(bezier3 b3) {
	curve c{ b3.p0, b3.p1, b3.p2 };
	c.type = 4;
	return c;
}
curve fromArc(detailedArc arc) {
	curve c{ arc.start, arc.end , arc.intersectAB };
	c.type = arc.determinant < 0 ? 2 : 3;
	return c;
}

inline float lineLength(curve c)
{
	float X = c.start.x - c.end.x;
	float Y = c.start.y - c.end.y;
	return sqrtf(X * X + Y * Y);
}

curve longestLine(vector<curve> curves)
{
	int recordIndex = 0;
	float recordLength = 0;
	for (int i = 0; i < curves.size(); i++)
	{
		float length = lineLength(curves[i]);
		if (length > recordLength) {
			recordIndex = i;
			recordLength = length;
		}
	}
	return curves[recordIndex];
}

curve flipLine(curve c) { return cLine(c.end, c.start); }



bool operator<(curve & a, curve & b)
{
	//float aX = a.start.x - a.end.x;
	//float aY = a.start.y - a.end.y;
	//float bX = b.start.x - b.end.x;
	//float bY = b.start.y - b.end.y;
	//return sqrtf(aX * aX + aY * aY) > sqrtf(bX * bX + bY * bY);
	return lineLength(a) > lineLength(b);
}


//curve operators
bool operator == (curve &a, curve &b) {
	return (a.p0 == b.p0) && (a.p1 == b.p1) && (a.p2 == b.p2);
}
bool operator != (curve &a, curve &b) {
	return !(a == b);
}
curve operator + (curve a, curve b) {
	curve c{ a.points[0] + b.points[0], a.points[1] + b.points[1], a.points[2] + b.points[2] };
	c.type = a.type;
	return c;
}
curve operator - (curve a, curve b) {
	curve c{ a.points[0] - b.points[0], a.points[1] - b.points[1], a.points[2] - b.points[2] };
	c.type = a.type;
	return c;
}
curve operator += (curve &a, curve b) {
	a = curve(a + b);
	return a;
}
curve operator -= (curve &a, curve b) {
	a = curve(a + b);
	return a;
}
curve operator * (curve a, curve b) {
	curve c{ a.points[0] * b.points[0], a.points[1] * b.points[1], a.points[2] * b.points[2] };
	c.type = a.type;
	return c;
}
curve operator / (curve a, curve b) {
	curve c{ a.points[0] / b.points[0], a.points[1] / b.points[1], a.points[2] / b.points[2] };
	c.type = a.type;
	return c;
}
curve operator *= (curve &a, curve b) {
	a = curve(a * b);
	return a;
}
curve operator /= (curve &a, curve b) {
	a = curve(a * b);
	return a;
}

void getGcode(char buffer[64], curve c, float feedRate, bool startOfLine)
{
	if (c.type == 1) {
		if (startOfLine)
			sprintf(buffer, "G1 X%.2f Y%.2f F%.1f", c.start.x, c.start.y, feedRate);
		else
			sprintf(buffer, "G1 X%.2f Y%.2f F%.1f", c.end.x, c.end.y, feedRate);
	}
	else if (c.type == 2) {
		sprintf(buffer, "G2 X%.2f Y%.2f I%.2f J%.2f F%.1f", c.end.x, c.end.y, c.center.x - c.start.x, c.center.y - c.start.y, feedRate);
	}
	else if (c.type == 3) {
		sprintf(buffer, "G3 X%.2f Y%.2f I%.2f J%.2f F%.1f", c.end.x, c.end.y, c.center.x - c.start.x, c.center.y - c.start.y, feedRate);
	}
	else
		cout << "can't get G-code from invalid type!\n";
}
void dwell(char buffer[64], int millis) {
	sprintf(buffer, "G4 P%d", millis);
}

vec4 packedLine(vec2 A, vec2 B) {
	vec2 position = vec2(
		(A.x + B.x),
		(A.y + B.y));
	if (A.y == B.y) {
		return vec4(position, 0, abs(A.x - B.x));
	}
	float deltaY = (A.y - B.y);
	float deltaX = (A.x - B.x);
	float angle = atan(deltaY / deltaX);
	float length = sqrt(deltaY * deltaY + deltaX * deltaX);
	return vec4(position, angle, length);
}

vec4 packedLine(curve c)
{
	return packedLine(c.start, c.end);
	//float deltaY = (c.start.y - c.end.y);
	//float deltaX = (c.start.x - c.end.x);
	//float angle = atan(deltaY / deltaX);
	//vec2 position = vec2(
	//	(c.start.x + c.end.x),
	//	(c.start.y + c.end.y));
	//float length = sqrt(deltaY * deltaY + deltaX * deltaX);
	//return vec4(position, angle, length);
}

vector<Gcommand> createScript(vector<vector<curve>> curves, int feed, int travelFeed, int dwellLenth, float * progress) {
	vector<Gcommand> commands;
	char buffer[64];
	char dwellBuff[64];
	dwell(dwellBuff, dwellLenth);

	int totalCurves = 0;
	float progInc;
	if (progress != NULL) {
		for (int i = 0; i < curves.size(); i++)
			totalCurves += curves[i].size();
		progInc = 1.0f / (float)totalCurves;
	}

	for (int i = 0; i < curves.size(); i++)
	{
		commands.push_back(Gcommand(pen_up_cmd));
		commands.push_back(Gcommand(dwellBuff));
		getGcode(buffer, curves[i][0], travelFeed, true);
		commands.push_back(Gcommand(buffer));
		commands.push_back(Gcommand(pen_down_cmd));
		commands.push_back(Gcommand(dwellBuff));
		for (int j = 0; j < curves[i].size(); j++) {
			getGcode(buffer, curves[i][j], feed);
			commands.push_back(Gcommand(buffer));
			if (progress != NULL)
				*progress += progInc;
		}
	}
	if (progress != NULL)
		*progress = 1;
	return commands;
}

void deleteSegmentsUp(linkedLineSegment * head) {
	linkedLineSegment *next = head;
	while (next) {
		linkedLineSegment *deleteMe = next;
		next = next->next;
		delete deleteMe;
	}
}

#define DISTANCE(a, b) sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y))

inline float distance(vec2 a, vec2 b) {
	return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}



vec2 getLinearPoint(float t, vec2 a, vec2 b) {
	//https://en.wikipedia.org/wiki/B%C3%A9zier_curve#Linear_B%C3%A9zier_curves
	return a + t * (b - a);
}

vec2 getBezierPoint(float t, bezier3 b)
{
	//https://en.wikipedia.org/wiki/B%C3%A9zier_curve#Quadratic_B%C3%A9zier_curves
	return (1 - t) * ((1 - t) * b.p0 + t * b.p1) + t * ((1 - t) * b.p1 + t * b.p2);
};

vec2 getBezierPoint(float t, bezier4 b)
{
	//https://en.wikipedia.org/wiki/B%C3%A9zier_curve#Cubic_B%C3%A9zier_curves
	vec2 A = getBezierPoint(t, bezier3{ b.p0, b.p1, b.p2 });
	vec2 B = getBezierPoint(t, bezier3{ b.p1, b.p2, b.p3 });
	return (1 - t) * A + t * B;
};

//stolen from https://www.geeksforgeeks.org/program-for-point-of-intersection-of-two-lines/
template<typename T>
T getLinearIntersectionVariablePrecision(T  A0, T A1, T B0, T B1, double * determinant) {
	// Line AB represented as a1x + b1y = c1 
	double a1 = A1.y - A0.y;
	double b1 = A0.x - A1.x;
	double c1 = a1 * (A0.x) + b1 * (A0.y);

	// Line CD represented as a2x + b2y = c2 
	double a2 = B1.y - B0.y;
	double b2 = B0.x - B1.x;
	double c2 = a2 * (B0.x) + b2 * (B0.y);

	*determinant = a1 * b2 - a2 * b1;

	if (*determinant == 0)
	{
		// The lines are parallel.
		return double2();
	}
	else
	{
		double x = (b2*c1 - b1 * c2) / *determinant;
		double y = (a1*c2 - a2 * c1) / *determinant;
		return T(x, y);
	}
}

double2 getLinearIntersection(double2  A0, double2 A1, double2 B0, double2 B1, double * determinant) {
	return getLinearIntersectionVariablePrecision(A0, A1, B0, B1, determinant);
}
vec2 getLinearIntersection(vec2 A0, vec2 A1, vec2 B0, vec2 B1, double * determinant) {
	return getLinearIntersectionVariablePrecision(A0, A1, B0, B1, determinant);
}
vec2 getLinearIntersection(curve A, curve B) {
	double unused;
	return getLinearIntersectionVariablePrecision(A.start, A.end, B.start, B.end, &unused);
}

template<typename V, typename T>
bool getLinearIntersectionTrimmedVariablePrecision(V A0, V A1, V B0, V B1, V * intersection)
{
	double determinant;
	*intersection = getLinearIntersection(A0, A1, B0, B1, &determinant);
	if (determinant == 0)
		return false;

	T leftBoundA = glm::min(A0.x, A1.x);
	T rightBoundA = glm::max(A0.x, A1.x);
	T leftBoundB = glm::min(B0.x, B1.x);
	T rightBoundB = glm::max(B0.x, B1.x);
	T upperBoundA = glm::max(A0.y, A1.y);
	T lowerBoundA = glm::min(A0.y, A1.y);
	T upperBoundB = glm::max(B0.y, B1.y);
	T lowerBoundB = glm::min(B0.y, B1.y);

	//bool a = intersection->x <= leftBoundA;
	//bool s = intersection->x >= rightBoundA;
	//bool d = intersection->x <= leftBoundB;
	//bool f = intersection->x >= rightBoundB;
	//bool g = intersection->y <= upperBoundA;
	//bool h = intersection->y >= lowerBoundA;
	//bool j = intersection->y <= upperBoundB;
	//bool k = intersection->y >= lowerBoundB;

	return
		intersection->x >= leftBoundA &&
		intersection->x <= rightBoundA &&
		intersection->x >= leftBoundB &&
		intersection->x <= rightBoundB &&
		intersection->y <= upperBoundA &&
		intersection->y >= lowerBoundA &&
		intersection->y <= upperBoundB &&
		intersection->y >= lowerBoundB;
}

bool getLinearIntersectionTrimmed(vec2 A0, vec2 A1, vec2 B0, vec2 B1, vec2 * intersection)
{
	return getLinearIntersectionTrimmedVariablePrecision<vec2, float>(A0, A1, B0, B1, intersection);
}
bool getLinearIntersectionTrimmed(double2 A0, double2  A1, double2  B0, double2  B1, double2 * intersection)
{
	return getLinearIntersectionTrimmedVariablePrecision<double2, double>(A0, A1, B0, B1, intersection);
}

bool checkParallelOverlap(curve A, curve B, float angle, float gap)
{
	vec2  unusedIntersection;
	curve shortLine = B;
	curve longLine = A;

	//solve for the shortest line
	if (distance(A.start, A.end) < distance(B.start, B.end)) {
		shortLine = A;
		longLine = B;
	}

	float rayAngle = angle - PI / 2;
	vec2 rayStart = vec2(cos(rayAngle), sin(rayAngle)) * gap * 1.5f + shortLine.start; //project just past
	rayAngle = angle + PI / 2;
	vec2 rayEnd = vec2(cos(rayAngle), sin(rayAngle)) * gap * 1.5f + shortLine.start;

	if (getLinearIntersectionTrimmed(rayStart, rayEnd, longLine.start, longLine.end, &unusedIntersection))
		return true;

	rayAngle = angle - PI / 2;
	rayStart = vec2(cos(rayAngle), sin(rayAngle)) * gap * 1.5f + shortLine.end;
	rayAngle = angle + PI / 2;
	rayEnd = vec2(cos(rayAngle), sin(rayAngle)) * gap * 1.5f + shortLine.end;

	return getLinearIntersectionTrimmed(rayStart, rayEnd, longLine.start, longLine.end, &unusedIntersection);
}

void sortByDistance(vec2 location, vector<vec2>* points)
{
	int i, j, min_idx;
	int length = points->size();

	for (i = 0; i < length - 1; i++)
	{
		min_idx = i;
		for (j = i + 1; j < length; j++)
			if (distance((*points)[j], location) < distance((*points)[min_idx], location))
				min_idx = j;

		vec2 temp = (*points)[min_idx];
		(*points)[min_idx] = (*points)[i];
		(*points)[i] = temp;
	}
}

//smallest enlosing rectangle with no rotation
void getBoundingBox(vector<vec2> points, float * W, float * H, vec2 * center) {
	int length = points.size();
	if (length < 1)
		return;

	float minX = points[0].x, maxX = points[0].x;
	float minY = points[0].y, maxY = points[0].y;

	for (int i = 0; i < length; i++)
	{
		minX = glm::min(minX, points[i].x);
		minY = glm::min(minY, points[i].y);
		maxX = glm::max(maxX, points[i].x);
		maxY = glm::max(maxY, points[i].y);
	}

	*W = maxX - minX;
	*H = maxY - minY;
	*center = vec2(minX + *W / 2, minY + *H / 2);
}

bool validateClearPath(curve line, vector<curve> obsticles)
{
	double2 unUsed;
	for (int i = 0; i < obsticles.size(); i++)
		if (getLinearIntersectionTrimmed((double2)obsticles[i].start, (double2)obsticles[i].end, (double2)line.start, (double2)line.end, &unUsed))
			return false;
	return true;
}

vector<curve> fillRectWithLines(float rectW, float rectH, vec2 rectCenter, float lineSpacing, float lineAngle, float shrink)
{
	vector<curve> lines;
	lineAngle = glm::min(lineAngle, PI / 2);
	lineAngle = glm::max(lineAngle, 0.0f);

	//to lazy to fix math, so perfectly vertical and horizontal lines are handled manually
	if (lineAngle == 0) {
		float verticalPos = shrink + rectCenter.y - rectH / 2;
		while (verticalPos < rectCenter.y + rectH / 2 - shrink) {
			vec2 lineStart = vec2(rectCenter.x - rectW / 2 + shrink, verticalPos);
			vec2 lineEnd = vec2(rectCenter.x + rectW / 2 - shrink, verticalPos);
			lines.push_back(cLine(lineStart, lineEnd));
			verticalPos += lineSpacing;
		}
		return lines;
	}
	else if (lineAngle == PI / 2) {
		float horizontalPos = shrink + rectCenter.x - rectW / 2;
		while (horizontalPos < rectCenter.x + rectW / 2 - shrink) {
			vec2 lineStart = vec2(horizontalPos, rectCenter.y - rectH / 2 + shrink);
			vec2 lineEnd = vec2(horizontalPos, rectCenter.y + rectH / 2 - shrink);
			lines.push_back(cLine(lineStart, lineEnd));
			horizontalPos += lineSpacing;
		}
		return lines;
	}

	//left edge 
	float outerAngle = PI / 2 - lineAngle;
	float edgeSpacing = lineSpacing / sin(outerAngle);
	vec2 topCorner = vec2(rectCenter.x - rectW / 2, rectCenter.y + rectH / 2);
	float maxLength = (rectW - shrink * 2) / cos(lineAngle);
	float edgeDistance = shrink;

	while (edgeDistance < rectH - shrink) {
		vec2 lineStart = topCorner + vec2(shrink, -edgeDistance);
		float lineLength = glm::min((edgeDistance - shrink) / cos(outerAngle), maxLength);
		vec2 lineEnd = vec2(cos(lineAngle), sin(lineAngle)) * lineLength + lineStart;
		lines.push_back(cLine(lineStart, lineEnd));
		edgeDistance += edgeSpacing;
	}

	//bottom edge
	edgeDistance = -tan(outerAngle) * (rectH - edgeDistance + edgeSpacing - shrink) + shrink;
	edgeSpacing = lineSpacing / sin(lineAngle);
	edgeDistance += edgeSpacing;
	vec2 bottomCorner = vec2(rectCenter.x - rectW / 2, rectCenter.y - rectH / 2);
	maxLength = (rectH - shrink * 2) / cos(outerAngle);

	while (edgeDistance < rectW - shrink) {

		vec2 lineStart = bottomCorner + vec2(edgeDistance, shrink);
		float lineLength = glm::min((rectW - edgeDistance - shrink) / cos(lineAngle), maxLength);
		vec2 lineEnd = vec2(cos(lineAngle), sin(lineAngle)) * lineLength + lineStart;
		lines.push_back(cLine(lineStart, lineEnd));
		edgeDistance += edgeSpacing;
	}
	return lines;
}

linkedLineSegment * linkSegments(vector<vector<curve>> trimmedLines, float infillAngle, float infillSpacing)
{
	int listLength = trimmedLines.size();
	linkedLineSegment * segmentEntries = new linkedLineSegment[listLength];

	//populate list with segment data and link vertically
	for (int i = 0; i < listLength; i++) {
		vector<curve> subdivisions = trimmedLines[i];
		//has to be at least one so populate data
		segmentEntries[i].seg = subdivisions[0];

		if (subdivisions.size() == 1)
			continue;

		//traversing up, linking backward, populating data
		linkedLineSegment * lastSeg = segmentEntries + i;
		for (int j = 1; j < subdivisions.size(); j++)
		{
			linkedLineSegment * seg = new linkedLineSegment(subdivisions[j]);
			seg->last = lastSeg;
			lastSeg->next = seg;
			lastSeg = seg;
		}
	}

	//link adjacent segments horizontally and count neighbors
	for (int i = 0; i < listLength - 1; i++)
	{
		//traverse upward
		linkedLineSegment * LLSeg = segmentEntries + i;
		do {
			//traverse upward from the bottom of the head which is one to the right
			curve leftLine = LLSeg->seg;
			linkedLineSegment * RLSeg = segmentEntries + i + 1;
			do {
				//detect neibors and link horizontally
				curve rightLine = RLSeg->seg;
				if (checkParallelOverlap(leftLine, rightLine, infillAngle, infillSpacing)) {
					LLSeg->rightNeighbors++;
					LLSeg->right = RLSeg;

					RLSeg->leftNeighbors++;
					RLSeg->left = LLSeg;
				}
				RLSeg = RLSeg->next;
			} while (RLSeg);
			LLSeg = LLSeg->next;
		} while (LLSeg);
	}

	return segmentEntries;
}

vector<rectilinearNetwork> findNetworks(linkedLineSegment * segments, int listLength)
{
	vector<rectilinearNetwork> networks;
	for (int i = 0; i < listLength; i++)
	{
		//traverse upward
		linkedLineSegment * lSeg = segments + i;
		do {
			if (!lSeg->partOfNet && lSeg->leftNeighbors > 1 || lSeg->rightNeighbors > 1) {
				rectilinearNetwork net;
				net.endA = lSeg;
				net.endB = lSeg;
				net.count = 1;
				networks.push_back(net);
				//drawLine(lSeg->seg, drawList, canPos, yellow, 13);
			}
			else if (!lSeg->partOfNet && lSeg->rightNeighbors == 1 && (lSeg->right->leftNeighbors > 1 || lSeg->right->rightNeighbors > 1)) {
				rectilinearNetwork net;
				net.endA = lSeg;
				net.endB = lSeg;
				net.count = 1;
				lSeg->partOfNet = true;

				//traverse horizontallyy (left)
				linkedLineSegment * nextLSeg = lSeg->left;
				while (nextLSeg && nextLSeg->rightNeighbors == 1 && nextLSeg->leftNeighbors <= 1) {
					net.endB = nextLSeg;
					nextLSeg->partOfNet = true;
					nextLSeg = nextLSeg->left;
					net.count++;
				}
				networks.push_back(net);
			}
			else if (!lSeg->partOfNet && lSeg->leftNeighbors == 1 && (lSeg->left->leftNeighbors > 1 || lSeg->left->rightNeighbors > 1)) {
				rectilinearNetwork net;
				net.endA = lSeg;
				net.endB = lSeg;
				net.count = 1;
				lSeg->partOfNet = true;

				//traverse horizontallyy (right)
				linkedLineSegment * nextLSeg = lSeg->right;
				while (nextLSeg && nextLSeg->leftNeighbors == 1 && nextLSeg->rightNeighbors <= 1) {
					net.endA = nextLSeg;
					nextLSeg->partOfNet = true;
					nextLSeg = nextLSeg->right;
					net.count++;
				}
				networks.push_back(net);
			}
			lSeg = lSeg->next;
		} while (lSeg);
	}
	return networks;
}

vector<curve> traverseRectillinearNetwork(rectilinearNetwork net, bool reverseDirection, bool reverseEnd)
{
	vector<curve> path;
	linkedLineSegment * lSeg = reverseDirection ? net.endB : net.endA;

	if (reverseEnd) {
		int temp = 0;
	}

	//reverseEnd = !reverseEnd;

	if (reverseDirection) {
		int count = 0;
		do {
			if (count > 0) {
				if (count % 2 != reverseEnd) {
					path.push_back(cLine(lSeg->seg.end, lSeg->seg.start));
					path.push_back(cLine(lSeg->left->seg.end, lSeg->seg.end));
				}
				else {
					path.push_back(lSeg->seg);
					path.push_back(cLine(lSeg->left->seg.start, lSeg->seg.start));
				}
			}
			else
				path.push_back(reverseEnd ? cLine(lSeg->seg.end, lSeg->seg.start) : lSeg->seg);
			lSeg = lSeg->right;
			count++;
		} while (count < net.count && lSeg);
	}
	else {
		int count = 0;
		do {
			if (count > 0) {
				if (count % 2 != reverseEnd) {
					path.push_back(cLine(lSeg->seg.end, lSeg->seg.start));
					path.push_back(cLine(lSeg->right->seg.end, lSeg->seg.end));
				}
				else {
					path.push_back(lSeg->seg);
					path.push_back(cLine(lSeg->right->seg.start, lSeg->seg.start));
				}
			}
			else
				path.push_back(reverseEnd ? cLine(lSeg->seg.end, lSeg->seg.start) : lSeg->seg);
			lSeg = lSeg->left;
			count++;
		} while (count < net.count && lSeg);
	}



	/*int count = 0;
	do {
		if (count > 0) {
			if (count % 2) {
				path.push_back(cLine(lSeg->seg.end, lSeg->seg.start));
				path.push_back(cLine(lSeg->right->seg.end, lSeg->seg.end));
			}
			else {
				path.push_back(lSeg->seg);
				path.push_back(cLine(lSeg->right->seg.start, lSeg->seg.start));
			}
		}
		else
			path.push_back(lSeg->seg);
		lSeg = reverseDirection ? lSeg->right : lSeg->left;
		count++;
	} while (count < net.count && lSeg);*/
	return path;
}

void booleanOpthreadFunc(vector<curve> infilLines, vector<curve> polygonLines, float shrink) {
	//for (int i = 0; i < infilLines.size(); i++) {
	//	vector<vec2> intersections;
	//	vector<curve> subdivisions;

	//	//for the given full segment, save all the points it intersects with the polygon
	//	for (int j = 0; j < polygonLines.size(); j++) {
	//		vec2 intersect;
	//		if (getLinearIntersectionTrimmed(infilLines[i].start, infilLines[i].end, polygonLines[j].start, polygonLines[j].end, &intersect))
	//			intersections.push_back(intersect);
	//	}

	//	//sort the subdivions and turn them into line segments
	//	sortByDistance(infilLines[i].start, &intersections);
	//	for (int j = 0; j < intersections.size(); j += 2)
	//	{
	//		if (j + 2 > intersections.size()) {
	//			//cout << "infill segment error: un even line intersections!\n";
	//			break;
	//		}
	//		curve segment = cLine(intersections[j], intersections[j + 1]);
	//		float segLength = distance(segment.start, segment.end);
	//		if (segLength < shrink * 2)
	//			continue;
	//		if (shrink == 0)
	//			subdivisions.push_back(segment);
	//		else {
	//			subdivisions.push_back(
	//				cLine(
	//					getLinearPoint(shrink / segLength, segment.start, segment.end),
	//					getLinearPoint(1 - shrink / segLength, segment.start, segment.end)
	//				)
	//			);
	//		}
	//	}
	//	if (subdivisions.size() > 0)
	//		output.push_back(subdivisions);
	//}
}

vector<vector<curve>> getInfillBooleanOp(vector<curve> infilLines, vector<curve> polygonLines, float shrink)
{
	vector<vector<curve>> output;

	//int threadCount = std::thread::hardware_concurrency();


	for (int i = 0; i < infilLines.size(); i++) {
		vector<vec2> intersections;
		vector<curve> subdivisions;

		//for the given full segment, save all the points it intersects with the polygon
		for (int j = 0; j < polygonLines.size(); j++) {
			vec2 intersect;
			if (getLinearIntersectionTrimmed(infilLines[i].start, infilLines[i].end, polygonLines[j].start, polygonLines[j].end, &intersect))
				intersections.push_back(intersect);
		}

		//sort the subdivions and turn them into line segments
		sortByDistance(infilLines[i].start, &intersections);
		for (int j = 0; j < intersections.size(); j += 2)
		{
			if (j + 2 > intersections.size()) {
				//cout << "infill segment error: un even line intersections!\n";
				break;
			}
			curve segment = cLine(intersections[j], intersections[j + 1]);
			float segLength = distance(segment.start, segment.end);
			if (segLength < shrink * 2)
				continue;
			if (shrink == 0)
				subdivisions.push_back(segment);
			else {
				subdivisions.push_back(
					cLine(
						getLinearPoint(shrink / segLength, segment.start, segment.end),
						getLinearPoint(1 - shrink / segLength, segment.start, segment.end)
					)
				);
			}
		}
		if (subdivisions.size() > 0)
			output.push_back(subdivisions);
	}
	return output;
}



vector<vector<curve>> getRectillinearInfill(vector<curve> polyLines, float lineSpacing, float lineAngle, float shrink)
{
	//	lineAngle = 0.2f;
		//initial and trimmed segments
	vector<vector<curve>> trimmedLines;
	vector<vec2> polyPoints;

	//polyPoints.push_back(polyLines[0].start);
	for (int i = 0; i < polyLines.size(); i++)
		polyPoints.push_back(polyLines[i].end);


	//bounding rect
	float W, H;
	vec2 cent;
	getBoundingBox(polyPoints, &W, &H, &cent);
	vec2 off = vec2(W, H) / 2.0f;
	W += 20;
	H += 20;

//	if (shrink == 0)
	//	shrink = 2.2001;

	vector<curve> fullSegments = fillRectWithLines(W, H, cent, lineSpacing, lineAngle, -100);
	trimmedLines = getInfillBooleanOp(fullSegments, polyLines, shrink);



	int listLength = trimmedLines.size();
	int networkCount = 0;
	if (listLength)
	{

		linkedLineSegment * segmentEntries = linkSegments(trimmedLines, lineAngle, lineSpacing);

		//organize forks amd draw debug info
		vector<rectilinearNetwork> networks = findNetworks(segmentEntries, listLength);

		networkCount = networks.size();

		vector<vector<curve>> loops;
#if 1
		//loops.push_back(polyLines);
		for (int i = 0; i < networkCount; i++)
		{
			rectilinearNetwork net = networks[i];
			vector<curve> path = traverseRectillinearNetwork(net);
			loops.push_back(path);
		}
		//single network
		if (networkCount == 0) {

			rectilinearNetwork net;
			net.endB = &segmentEntries[0];
			net.endA = &segmentEntries[listLength - 1];
			net.count = listLength;

			vector<curve> path = traverseRectillinearNetwork(net);
			loops.push_back(path);
		}
#else
		loops = optimizeRectillinearInfill(networks, polyLines);
#endif

		for (int i = 0; i < listLength; i++)
			if (segmentEntries[i].next)
				deleteSegmentsUp(segmentEntries[i].next);
		delete segmentEntries;

		return loops;
	}
}

//awful variable names, but b if the point of the angle
float get3PointAngleAcute(vec2 A, vec2 B, vec2 C) {
	//get lines lengths from the point7 s
	vec2 vA = A - B;
	vec2 vB = C - B;

	//need normalized vectors for the dot product
	vec2 vnA = normalize(vA);
	vec2 vnB = normalize(vB);

	//get the angle with dot
	float dp = dot(vnA, vnB);
	float angle = acosf(dp);

	//make sure the angle is acute
	if (angle > PI)
		angle -= PI;

	return angle;
}

const int nodeNeighborCount = 50;

struct loopNode {
	loopNode * neighbors[nodeNeighborCount];
	float neighborDistances[nodeNeighborCount];
	vec2 entryPosition;
	loopNode * exitNode;
	int vectorIndex = 0;
	int neighborCount = 0;
	bool traversed = false;
	bool * commonTraversal = NULL;
	int traversalType = 0;
	loopNode * next = NULL;
	loopNode() {};
	loopNode(vec2 pos, int index) {
		entryPosition = pos;
		vectorIndex = index;
		std::fill(neighborDistances, neighborDistances + nodeNeighborCount, -1);
		for (int i = 0; i < nodeNeighborCount; i++)
			neighbors[i] = NULL;
	};
};

loopNode * traverseNodes(loopNode * start, loopNode * end, loopNode * curnode, int progress = 0) {
	curnode->traversed = true;
	for (int i = 0; i < nodeNeighborCount; i++)
	{
		if (!curnode->neighbors[i]) {
			break;
		}
		if (!curnode->neighbors[i]->traversed) {
			curnode->next = curnode->neighbors[i];
			return curnode->neighbors[i];
			//traverseNodes(start, end, curnode->neighbors[i], progress++);
		}
	}

	//all neihbors taken. have to search through the whole array
	loopNode * node = start;
	loopNode * closest = NULL;
	float closestDistance;

	while (node != end) {
		if ((closest == NULL || distance(curnode->entryPosition, node->entryPosition) < closestDistance) && node->entryPosition != curnode->entryPosition && !node->traversed) {
			closest = node;
			closestDistance = distance(curnode->entryPosition, node->entryPosition);
		}
		node++;
	}
	//finished
	if (closest == NULL)
		return NULL;
	curnode->next = closest;
	return closest;
	//traverseNodes(start, end, closest, progress++);
}
loopNode * traverseRectillinearNodes(loopNode * start, loopNode * end, loopNode * curnode, int progress = 0) {
	//curnode->traversed = true;
	*(curnode->commonTraversal) = true;
	loopNode * exit = curnode->exitNode;
	for (int i = 0; i < nodeNeighborCount; i++)
	{
		if (!exit->neighbors[i]) 
			break;
		
		if (!*(exit->neighbors[i]->commonTraversal)) {
			//exit->next = exit->neighbors[i];
			curnode->next = exit->neighbors[i];
			return exit->neighbors[i];
		}
	}

	//all neihbors taken. have to search through the whole array
	loopNode * node = start;
	loopNode * closest = NULL;
	float closestDistance;

	while (node != end) {
		if ((closest == NULL || distance(curnode->entryPosition, node->entryPosition) < closestDistance) && node->entryPosition != curnode->entryPosition && !(*node->commonTraversal)) {
			closest = node;
			closestDistance = distance(curnode->entryPosition, node->entryPosition);
		}
		node++;
	}
	//finished
	if (closest == NULL)
		return NULL;
	curnode->next = closest;
	return closest;
	//traverseNodes(start, end, closest, progress++);
}



//goes : start of total array, end of total array, first node in array to find neigbors for, last node to find neighbors for
volatile int findNeighborProgress = 0, neighborJobLength;
volatile float * GlobalProgressState;
void findLoopNeighborsParallel(loopNode * start, loopNode * end, loopNode * jobStart, loopNode * jobEnd, bool lastThread = false, bool rectillinear = false) {
	loopNode * curnode = jobStart;

	//iterate to find closeset nodes for each node
	while (curnode != jobEnd) {
		loopNode * testNode = start;
		while (testNode != end) {
			if (testNode == curnode || (rectillinear && testNode->commonTraversal == curnode->commonTraversal)) {
				testNode++;
				continue;
			}

			float dist = distance(curnode->entryPosition, testNode->entryPosition);
			for (int i = 0; i < nodeNeighborCount; ++i)
			{
				//fits in an empty neibor slot
			//	if (curnode->neighborDistances[i] == -1) {
				if (curnode->neighborCount == i) {
					curnode->neighbors[i] = testNode;
					curnode->neighborDistances[i] = dist;
					curnode->neighborCount++;
					break;
				}
				//overrides a neighbor slot
				if (dist < curnode->neighborDistances[i]) {
					loopNode * tempnodeA = curnode->neighbors[i];
					float tempDistA = curnode->neighborDistances[i];
					curnode->neighbors[i] = testNode;
					curnode->neighborDistances[i] = dist;

					//shuffle
					loopNode * tempnodeB;
					float tempDistB;
					for (int j = i + 1; j < nodeNeighborCount; j++)
					{
						tempnodeB = curnode->neighbors[j];
						tempDistB = curnode->neighborDistances[j];
						curnode->neighbors[j] = tempnodeA;
						curnode->neighborDistances[j] = tempDistA;
						tempnodeA = tempnodeB;
						tempDistA = tempDistB;
					}
					i == nodeNeighborCount;
				}
			}
			testNode++;
		}
		curnode++;
		findNeighborProgress++;
		if (lastThread && !rectillinear) {
			*GlobalProgressState = (float)findNeighborProgress / (float)neighborJobLength;
		}
	}
}

vector<vector<vec2>> getOptimizedPointOrder(vector<vector<vec2>> unsorted, volatile float * progress, volatile int * progressState)
{
	if (progressState != NULL)
		*progressState = 0;
	if (progress != NULL)
		*progress = 0;
	findNeighborProgress = 0;
	GlobalProgressState = progress;

	vector<vector<vec2>> sorted;
	int pointCount = unsorted.size();

	//create nodes for all loop starts
	loopNode * nodes = new loopNode[pointCount];
	for (int i = 0; i < pointCount; i++)
		nodes[i] = loopNode(unsorted[i][0], i);

	//start threads
	int threadCount = std::thread::hardware_concurrency();
	thread * threads = new thread[threadCount];
	int jobSize = pointCount / threadCount;
	neighborJobLength = jobSize;
	for (int i = 0; i < threadCount - 1; i++)
		threads[i] = thread(findLoopNeighborsParallel, nodes, nodes + pointCount, nodes + jobSize * i, nodes + jobSize * (i + 1), false, false);
	threads[threadCount - 1] = thread(findLoopNeighborsParallel, nodes, nodes + pointCount, nodes + jobSize * threadCount - 2, nodes + pointCount, true, false);
	for (int i = 0; i < threadCount; i++)
		threads[i].join();


	if (progressState != NULL)
		*progressState = 1;
	if (progress != NULL)
		*progress = 0;

	//get final path
	loopNode * node = traverseNodes(nodes, nodes + pointCount, nodes);
	int traverseProgress = 0;
	while (node) {
		node = traverseNodes(nodes, nodes + pointCount, node);
		traverseProgress++;
		if (progress != NULL)
			*progress = (float)traverseProgress / (float)pointCount;
	}

	if (progressState != NULL)
		*progressState = 2;
	if (progress != NULL)
		*progress = 1;

	node = nodes;
	while (node) {
		sorted.push_back(unsorted[node->vectorIndex]);
		node = node->next;
	}
//	delete nodes;
	//	delete threads;
	return sorted;
}

vector<vector<curve>> optimizeRectillinearInfill(vector<rectilinearNetwork> networks, vector<curve> outline)
{
	int nodeCount = 0;
	int networkCount = networks.size();
	for (int i = 0; i < networkCount; i++)
		nodeCount += networks[i].count > 1 ? 4 : 2;

	loopNode * nodes = new loopNode[nodeCount];
	bool * commonFlags = new bool[networkCount];

	vector<vector< curve >> traversal;
	for (int i = 0; i < networkCount; i++)	
		traversal.push_back(traverseRectillinearNetwork(networks[i]));
	
	{
		int i = 0, j = 0;
		while (i < nodeCount && j < networkCount) {
			rectilinearNetwork * net = &networks[j];
			if (networks[j].count > 1) {
				nodes[i++] = loopNode(net->endA->seg.start, j);
				nodes[i++] = loopNode(net->endA->seg.end, j);
				nodes[i++] = loopNode(net->endB->seg.start, j);
				nodes[i++] = loopNode(net->endB->seg.end, j);
				nodes[i - 4].exitNode = net->count % 2 ? nodes + i - 1 : nodes + i - 2;
				nodes[i - 3].exitNode = net->count % 2 ? nodes + i - 2 : nodes + i - 1;
				nodes[i - 2].exitNode = net->count % 2 ? nodes + i - 3 : nodes + i - 4;
				nodes[i - 1].exitNode = net->count % 2 ? nodes + i - 4 : nodes + i - 3;
				nodes[i - 4].commonTraversal = commonFlags + j;
				nodes[i - 3].commonTraversal = commonFlags + j;
				nodes[i - 4].traversalType = 0;
				nodes[i - 3].traversalType = 1;
				nodes[i - 2].traversalType = 2;
				nodes[i - 1].traversalType = 3;
			}
			else {
				nodes[i++] = loopNode(net->endA->seg.start, j);
				nodes[i++] = loopNode(net->endA->seg.end, j);
				nodes[i - 2].exitNode = nodes + i - 1;
				nodes[i - 1].exitNode = nodes + i - 2;
				nodes[i - 2].traversalType = 0;
				nodes[i - 1].traversalType = 2;
			}
			nodes[i - 2].commonTraversal = commonFlags + j;
			nodes[i - 1].commonTraversal = commonFlags + j;
			commonFlags[j] = false;
			j++;
		}
	}

	//start threads
	int threadCount = std::thread::hardware_concurrency();
	thread * threads = new thread[threadCount];
	int jobSize = nodeCount / threadCount;
	neighborJobLength = jobSize;
	for (int i = 0; i < threadCount - 1; i++)
		threads[i] = thread(findLoopNeighborsParallel, nodes, nodes + nodeCount, nodes + jobSize * i, nodes + jobSize * (i + 1), false, true);
	threads[threadCount - 1] = thread(findLoopNeighborsParallel, nodes, nodes + nodeCount, nodes + jobSize * threadCount - 2, nodes + nodeCount, true, true);
	for (int i = 0; i < threadCount; i++)
		threads[i].join();

		//get final path
	loopNode * node = traverseRectillinearNodes(nodes, nodes + nodeCount, nodes);
	int traverseProgress = 0;
	while (node) {
		node = traverseRectillinearNodes(nodes, nodes + nodeCount, node);
		traverseProgress++;
		cout << traverseProgress << endl;
	}

	vector<vector<curve>> sortedLoops;

	node = nodes;
	int tempI = 0;
	while (node) {
		//sortedLoops.push_back(traversal[node->vectorIndex]);
		sortedLoops.push_back(traverseRectillinearNetwork(networks[node->vectorIndex], node->traversalType > 1, (node->traversalType % 2)));
		node = node->next;
		//node = node->exitNode;
		tempI++;
		//cout << tempI << endl;
	}

	delete nodes;
	delete commonFlags;


	vector<vector<curve>> tempLoops;
	for (int i = 0; i < sortedLoops.size(); i++)
	{
		vector<curve> loop;
		for (int j = 0; j < sortedLoops[i].size(); j++)
		{
			if (i > 0 && j == 0) {
				curve potentialLine = cLine(sortedLoops[i - 1][sortedLoops[i - 1].size() - 1].end, sortedLoops[i][j].start);
				if(validateClearPath(potentialLine, outline))
					loop.push_back(potentialLine);
			}
			loop.push_back(sortedLoops[i][j]);
		}
		tempLoops.push_back(loop);
	}

	return tempLoops; //sortedLoops;
}

loopList OptimizeDrawPath(loopList drawpath)
{
	return loopList();
}

drawLoop simplifyDrawPath(drawLoop drawpth)
{
	return drawLoop();
}


//t0 and t1 are to specify the portion of the bezier that should be simplified (0 and 1 for the whole thing)
detailedArc getArcApproximateDetailed(bezier3 b3, float t0, float t1, bool correctVisuals)
{
	detailedArc ar;
	float lineLength = 600;

	ar.start = getBezierPoint(t0, b3);
	ar.end = getBezierPoint(t1, b3);
	ar.curveMiddle = getBezierPoint(t0 + (t1 - t0) / 2, b3);

	/*
		There are a few ways of going about this, but we closely follows the process
		layed out here: https://pomax.github.io/bezierinfo/#arcapproximation
		It essentially treats the 3 control points of a quadratic bezier (or section of one)
		as lines, and uses the intersection point of the original lines centered tangents
		to located the center of their closest perfect circle
	*/

	//A
	ar.A0 = getLinearPoint(0.5, ar.start, ar.curveMiddle);
	ar.angleA = atan2((ar.curveMiddle.y - ar.start.y), (ar.curveMiddle.x - ar.start.x));
	ar.A1 = vec2(cos(ar.angleA - PI / 2) * lineLength, sin(ar.angleA - PI / 2) * lineLength) + ar.A0;


	//B
	ar.B0 = getLinearPoint(0.5, ar.curveMiddle, ar.end);
	ar.angleB = atan2((ar.end.y - ar.curveMiddle.y), (ar.end.x - ar.curveMiddle.x));
	ar.B1 = vec2(cos(ar.angleB - PI / 2) * lineLength, sin(ar.angleB - PI / 2) * lineLength) + ar.B0;

	//C
	ar.C0 = getLinearPoint(0.5, ar.start, ar.end);
	ar.angleC = atan2((ar.end.y - ar.start.y), (ar.end.x - ar.start.x));
	ar.C1 = vec2(cos(ar.angleC - PI / 2) * lineLength, sin(ar.angleC - PI / 2) * lineLength) + ar.C0;

	//the guide says to find the 3-way intersection, but from my testing it makes no difference with just A and B
	ar.intersectAB = getLinearIntersection(ar.A0, ar.A1, ar.B0, ar.B1, &ar.determinant);
	ar.radius = distance(ar.intersectAB, ar.curveMiddle);

	//flip the chord lines to be visually correct if there is a positive determinant and set line length to circle edge
	//There are actually even more senerios to acount for when displaying the arc, but they are handled later
	if (correctVisuals) {
		int flip = (ar.determinant > 0) ? 1 : -1;

		//A
		float lineLA = distance(ar.A0, ar.intersectAB) + ar.radius;
		ar.A1 = vec2(cos(ar.angleA + (flip * PI) / 2) * lineLA, sin(ar.angleA + (flip * PI) / 2) * lineLA) + ar.A0;
		//B
		float lineLB = distance(ar.B0, ar.intersectAB) + ar.radius;
		ar.B1 = vec2(cos(ar.angleB + (flip * PI) / 2) * lineLB, sin(ar.angleB + (flip * PI) / 2) * lineLB) + ar.B0;
		//C
		float lineLC = distance(ar.C0, ar.intersectAB) + ar.radius;
		ar.C1 = vec2(cos(ar.angleC + (flip * PI) / 2) * lineLC, sin(ar.angleC + (flip * PI) / 2) * lineLC) + ar.C0;
	}
	return ar;
}
detailedArc getArcApproximateDetailed(curve c, float t0, float t1, bool correctVisuals) {
	return getArcApproximateDetailed(bezier3{ c.p0, c.p1, c.p2 }, t0, t1, correctVisuals);
}

//aproximates like before, but cuts out the fat created by the calculation
simpleArc getArcApproximateSimple(bezier3 b, float t0, float t1) {
	detailedArc dar = getArcApproximateDetailed(b, t0, t1);
	simpleArc sar{ dar.radius, dar.determinant < 0, dar.start, dar.end };
	return sar;
}

/*
	lazy mans way to round the a corner. It does this by treating the 3 triangle points
	as a bezier control points and aproximating them. What this function does in addition to the
	aproximation is make sure the bezier is trimmed down to it's shorted edge which insures the approximated
	bezier is semtrical. Couple that with an apropriatae t value and the arc should always stay withen the bounds of the triangle
*/
detailedArc getRoundedCorner(vec2 A, vec2 C, vec2 B, float t) {
	detailedArc ar;

	float dist0 = distance(A, C);
	float dist1 = distance(B, C);
	float mindist = glm::min(dist0, dist1);
	float maxdist = glm::max(dist0, dist1);

	vec2 P0 = getLinearPoint(t, A, C);
	vec2 P1 = getLinearPoint(1.0 - t, C, B);

	if (dist0 != mindist)
		P1 = getLinearPoint(1.0 - ((mindist / maxdist) * t), C, B);
	else
		P0 = getLinearPoint(((mindist / maxdist) * t), A, C);

	bezier3 b3{ P0, C, P1 };
	return getArcApproximateDetailed(b3, 0, 1, true);
}

#undef DISTANCE