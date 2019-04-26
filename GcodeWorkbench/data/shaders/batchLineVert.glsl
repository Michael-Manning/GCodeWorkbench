#version 430 core
layout (location = 1) in vec2 aPos;
layout (location = 2) in vec4 transform;

uniform float aspect;
uniform float width;
uniform vec2 resolution;


mat4 trans(){
   // float sx = 0.001;//width;
    // float sx = 100000.0;
    // float sy = transform.w;
     
    float sx =  (transform.w / (resolution.x  * 0.5)) *1.005;
    float sy =  width / (resolution.y * 0.5);

    float px =  ((transform.x  ) / resolution.x) -1.0;
    float py =  ((transform.y ) / resolution.y) -1.0;

    // float an = 0.0;
    // float s = sin(an);
    // float c = cos(an);
    float s = sin(-transform.z);
    float c = cos(-transform.z);

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
    return (tran * ascal * rot *  scal);
}

void main()
{
    vec4 ndcPos =  trans() * vec4(aPos.x , aPos.y, 0.0, 1.0);    
    gl_Position = ndcPos;
}  



