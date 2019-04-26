#version 430 core

in vec3 Position_worldspace;
uniform float lightPow;

out vec4 color;

void main(){
	color = vec4(vec3(clamp((Position_worldspace.z + 0.0)  * lightPow,0.0, 1.0)), 1.0);
}