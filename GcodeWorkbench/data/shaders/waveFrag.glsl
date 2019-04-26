#version 430 core

in vec2 frag_uv;
uniform sampler2D waveData;
uniform sampler2D derivatives;
uniform vec2 resolution;
uniform float div;
uniform float base;
uniform float darkFreq;
uniform float ampSetting;
out vec4 FragColor;

const float lineWidth = 1.0;

float plotSinWave(vec2 currentUv, float freq, float amp)
{
    float dx = (lineWidth) / resolution.x;
    float dy = (lineWidth) / resolution.y + sin(dx * freq) * amp;
    
    float sy = sin(currentUv.x * freq ) * amp;
    float dsy = cos(currentUv.x * freq ) * amp * freq;

    return smoothstep(0.0, dy, (abs(currentUv.y - sy))/sqrt(1.0+dsy*dsy));
   // return step(0.0, dy, (abs(currentUv.y - sy))/sqrt(1.0+dsy*dsy));
}

void main()
{
    vec2 uv = vec2(frag_uv.x, 1.0 - frag_uv.y);
 float amp, der;
      float stp = 1.0 / (div / resolution.y);
    float f = 0.0;
    for(float i = -0.5; i < 0.5; i+= div / resolution.y ){
      
        float v = i  * stp;
        v = round(v);
        v = v/stp;

      amp = texture(waveData, vec2(uv.x, v + 0.5)).r;
      der = texture(derivatives, vec2(uv.x, v + 0.5)).r * darkFreq;




       // f += 1.0 - plotSinWave(vec2(uv.x + der, uv.y + i - 0.5), 200.0,0.1 * 0.1);
        f += 1.0 - plotSinWave(vec2(uv.x + der, uv.y + i - 0.5), base, (1.0 - amp) * ampSetting);
    }

    float c = texture(waveData, uv).r / 3;//' amp / 3.0;
   // float c = texture(derivatives, uv).r / 4.0;
    FragColor = vec4(0.0, 0.0,0.0 , f );

};



