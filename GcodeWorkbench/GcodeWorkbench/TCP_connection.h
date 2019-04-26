#pragma once
#include <Gcode/curve_tools.h>

#define pen_up_cmd "M42 P60 S0"
#define pen_down_cmd "M42 P60 S1"

void connectTCP(const char * ip, unsigned short port);

void disconnectTCP();

void sendCommand(Gcommand command);

void pauseStream();

void resumeStream();

float getStreamProgress();

void streamFile(const char * file);

vec2 getPosition();