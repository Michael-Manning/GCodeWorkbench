#version 430 core
//https://www.shadertoy.com/view/4sBSDW

//
// This is biased (saturates + adds contrast) because dithering done in non-linear space.
//

// Shows proper dithering of a signal (overlapping of dither between bands).
// Results in about a 1-stop improvement in dynamic range over conventional dither
//   which does not overlap dither across bands
//   (try "#define WIDE 0.5" to see the difference below). 
//
// This would work a lot better with a proper random number generator (flicker etc is bad).
// Sorry there is a limit to what can be done easily in shadertoy.
//
// Proper dithering algorithm,
//
//   color = floor(color * steps + noise) * (1.0/(steps-1.0))
//
// Where,
//   
//   color ... output color {0 to 1}
//   noise ... random number between {-1 to 1}
//   steps ... quantization steps, ie 8-bit = 256
//
// The noise in this case is shaped by a high pass filter.
// This is to produce a better quality temporal dither.

in vec2 frag_uv;
uniform sampler2D Text;
uniform vec2 resolution;
uniform float steps;
uniform float brightness;
uniform float downscale;
out vec4 FragColor;

// Scale the width of the dither
#define WIDE 1.0

//-----------------------------------------------------------------------


float Noise(vec2 n,float x){n+=x;return fract(sin(dot(n.xy,vec2(12.9898, 78.233)))*43758.5453)*2.0-1.0;}

// Step 1 in generation of the dither source texture.
float Step1(vec2 uv,float n){
 float a=1.0,b=2.0,c=-12.0,t=1.0;   
 return (1.0/(a*4.0+b*4.0-c))*(
  Noise(uv+vec2(-1.0,-1.0)*t,n)*a+
  Noise(uv+vec2( 0.0,-1.0)*t,n)*b+
  Noise(uv+vec2( 1.0,-1.0)*t,n)*a+
  Noise(uv+vec2(-1.0, 0.0)*t,n)*b+
  Noise(uv+vec2( 0.0, 0.0)*t,n)*c+
  Noise(uv+vec2( 1.0, 0.0)*t,n)*b+
  Noise(uv+vec2(-1.0, 1.0)*t,n)*a+
  Noise(uv+vec2( 0.0, 1.0)*t,n)*b+
  Noise(uv+vec2( 1.0, 1.0)*t,n)*a+
 0.0);}
    
// Step 2 in generation of the dither source texture.
float Step2(vec2 uv,float n){
 float a=1.0,b=2.0,c=-2.0,t=1.0;   
 return (4.0/(a*4.0+b*4.0-c))*(
  Step1(uv+vec2(-1.0,-1.0)*t,n)*a+
  Step1(uv+vec2( 0.0,-1.0)*t,n)*b+
  Step1(uv+vec2( 1.0,-1.0)*t,n)*a+
  Step1(uv+vec2(-1.0, 0.0)*t,n)*b+
  Step1(uv+vec2( 0.0, 0.0)*t,n)*c+
  Step1(uv+vec2( 1.0, 0.0)*t,n)*b+
  Step1(uv+vec2(-1.0, 1.0)*t,n)*a+
  Step1(uv+vec2( 0.0, 1.0)*t,n)*b+
  Step1(uv+vec2( 1.0, 1.0)*t,n)*a+
 0.0);}
    
// Used for stills.
vec3 Step3(vec2 uv){
 float a=Step2(uv,0.07);    
 float b=Step2(uv,0.11);    
 float c=Step2(uv,0.13);
 #if 1
  // Monochrome can look better on stills.
  return vec3(a);
 #else
  return vec3(a,b,c);
 #endif
}



void main(){    



 vec2 uv = frag_uv;


 vec2 stp = (downscale * resolution);
vec2 blocky = round(uv * stp);
 blocky = blocky/stp;

 vec3 color = vec3(texture(Text, blocky).r + brightness);
if(color.r > 0.94){
    FragColor = vec4(vec3(1.0), 1.0);
    return;
}
 color = floor(0.5+color*(steps+WIDE-1.0)+(-WIDE*0.5)+Step3(blocky * resolution)*WIDE)*(1.0/(steps-1.0));

vec2 circUV = fract((uv + (1.0 /stp) * 0.5) * stp);
if(length(circUV - 0.5) > 0.5 - clamp((color.r * 0.25), 0.0, 0.5 ))
    color = vec3(1.0); //color = mix(vec3(1.0), color, 0.5); //

//if(color.r < 1.0)
 //   color = vec3(0.0);

FragColor = vec4(color,1.0);
//FragColor=vec4(vec3(uv.x / resolution.x),1.0);
 }


