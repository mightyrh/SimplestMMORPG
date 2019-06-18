#version 330

layout(location=0) out vec4 FragColor;

uniform vec4 u_Color;
uniform float u_Depth;

void main()
{
	FragColor = vec4(u_Color.r, u_Color.g, u_Color.b, u_Color.a);
    if(abs(FragColor.a) < 0.00001)
        gl_FragDepth = 1.0;
    else
        gl_FragDepth = u_Depth;
}
