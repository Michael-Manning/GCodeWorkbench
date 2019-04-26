#version 430 core

in vec2 frag_uv;
uniform sampler2D Text;
uniform vec2 resolution;
uniform vec2 clippingPlane;
out vec4 FragColor;


void main()
{
    float zNear = clippingPlane.x;//0.5;  //zNear
    float zFar  = clippingPlane.y;//10.0; //zFar  
    float depth = texture2D(Text,  frag_uv).x * 1.000;
    float c = (2.0 * zNear) / (zFar + zNear - depth * (zFar - zNear));
    //c = clamp(c, 0.0, 1.0);
    FragColor = vec4(vec3(c), 1.0);

}