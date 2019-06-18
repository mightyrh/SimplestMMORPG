#version 330

layout(location=0) out vec4 FragColor;

in vec2 v_TexPos;
uniform vec4 u_Color;
uniform float u_Depth;
uniform sampler2D u_Texture;

void main()
{
	FragColor = texture(u_Texture, v_TexPos)*u_Color;
    if(abs(FragColor.a) < 0.00001)
        gl_FragDepth = 1.0;
    else
        gl_FragDepth = u_Depth;
}
