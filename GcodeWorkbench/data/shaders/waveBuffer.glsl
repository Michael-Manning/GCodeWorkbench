#version 430 core

in vec2 frag_uv;
uniform sampler2D Text;
uniform vec2 resolution;
uniform float div;
out vec4 FragColor;

void main()
{
    vec2 uv = frag_uv;


    vec4 texCol;


    if(mod(uv.y * resolution.y, div) > 1.0){// || uv.y * imSize.y > imSize.y - div){
        if(uv.y * resolution.y > resolution.y - div){
            FragColor = vec4(vec3(1.0), 1.0);
            return;
        }
         FragColor = vec4(vec3(1.0), 1.0);
        return;
    }


    //  FragColor = vec4(texture(Text, frag_uv));
    //  return;

    float av = 0;

    for(float i = 0.0; i < div; i++){
        vec4 col = vec4(texture(Text, frag_uv + vec2(0.0, i / resolution.y)));
        av += (col.r + col.g + col.b) / 3.0;
    }
    av /= div;
    FragColor = vec4(vec3(av), 1.0);
    
};



