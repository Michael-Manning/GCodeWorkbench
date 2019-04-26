#version 430 core
layout (location = 0) in vec3 aPos;
uniform float aspect;
uniform float angle;
uniform vec2 scale;
uniform vec2 pos;

out vec2 frag_uv;

mat4 trans(){
    float sx = scale.x;
    float sy = scale.y;
     
    float px = pos.x;
    float py = pos.y;

    float s = sin(-angle);
    float c = cos(-angle);

    //translation
mat4 tran = mat4(1.0, 0.0, 0.0,  0.0,
                 0.0, 1.0, 0.0,  0.0,
                 0.0, 0.0, 1.0,  0.0,
                 px,  py,  0.0,  1.0);

    //aspect scale
mat4 ascal = mat4(aspect, 0.0,  0.0,  0.0,
                     0.0, 1.0,  0.0,  0.0,
                     0.0, 0.0,  1.0,  0.0,
                     0.0, 0.0,  0.0,  1.0);

    //scale
mat4 scal = mat4(sx,  0.0, 0.0,  0.0,
                 0.0, sy,  0.0,  0.0,
                 0.0, 0.0, 1.0,  0.0,
                 0.0, 0.0, 0.0,  1.0);
    //rotation
mat4 rot = mat4( c,  -s,  0.0, 0.0,
                 s,   c,  0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0);
    return (tran * ascal * rot * scal);
}

void main()
{
     vec4 ndcPos =  trans() * vec4(aPos.x /*/ aspect*/, aPos.y, aPos.z, 1.0);    
    frag_uv = aPos.xy *0.5 + 0.5; 
    gl_Position = ndcPos;
}