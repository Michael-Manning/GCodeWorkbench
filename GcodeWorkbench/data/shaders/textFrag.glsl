#version 430 core

in vec2 frag_uv;
uniform sampler2D Text;
uniform vec2 featherXY; 
uniform vec2 cropXY; 
uniform float threshold; 
out vec4 FragColor;

float feather(float t, float inner, float crop){
    
    float f = (t - inner) * (1.0/ (crop - inner));
    f = clamp(f, 0.0, 1.0);
    return f;
}

void main()
{
   vec4 texCol;
   texCol = vec4(texture(Text, frag_uv));
   vec2 uv = frag_uv;

   float  f = 1.0 - feather(uv.y, featherXY.y, cropXY.y);
   f *= 1.0 - feather(1.0 - uv.y, featherXY.y, cropXY.y);
   f *= 1.0 - feather(uv.x, featherXY.x, cropXY.x);
   f *= 1.0 - feather(1.0 - uv.x, featherXY.x, cropXY.x);

   f = clamp(f, 0.0, 1.0);

   if(threshold == 0)
      FragColor = vec4(texCol.rgb, texCol.a * (f));
   else{
      float color = step(threshold, (texCol.r + texCol.g + texCol.b) / 3);
      FragColor = vec4(vec3(color), texCol.a * (f));
   }
};



