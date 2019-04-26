#version 430 core
layout (location = 0) in vec3 aPos;
uniform float aspect;
uniform float skew;
uniform mat4 xform;
out vec2 frag_uv;

void main()
{

mat4 tran = mat4(1.0, tan(skew), 0.0,  0.0,
                 0.0, 1.0, 0.0,  0.0,
                 0.0, 0.0, 1.0,  0.0,
                 0.0, 0.0,  0.0,  1.0);


    vec4 ndcPos =  (tran * xform * vec4(aPos.x / aspect, aPos.y, aPos.z, 1.0));
    frag_uv = aPos.xy *0.5 + 0.5; 

   gl_Position = ndcPos;
}