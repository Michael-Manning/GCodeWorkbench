#pragma once

#include <stdio.h>
#include "bezier_tools.h"

using namespace std;

//rapid linear move
void G0(char buff[128], float x, float y, int feed) {
	sprintf(buff, "G0 X%.1f Y%.1f F%d", x, y, feed);
}

//smooth linear move
void G1(char buff[128], float x, float y, int feed) {
	sprintf(buff, "G1 X%.1f Y%.1f F%d", x, y, feed);
}

//clockwise arc
void G2(char buff[128], float x, float y, float r, int feed) {
	sprintf(buff, "G2 X%.1f Y%.1f R%.1f F%d", x, y, r, feed);
}

//counter clockwise arc
void G3(char buff[128], float x, float y, float r, int feed) {
	sprintf(buff, "G3 X%.1f Y%.1f R%.1f F%d", x, y, r, feed);
}
void setSpindle(char buff[8], bool state) {

}








//void G0(char buff[128], float x, float y, int feed);
//void G1(char buff[128], float x, float y, int feed);
//void G2(char buff[128], float x, float y, float r, int feed);
//void G3(char buff[128], float x, float y, float r, int feed);
//void setSpindle(char buff[8], bool state);