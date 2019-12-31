#version 330

uniform float shade;
uniform float alpha;
out vec4 out_Color;

void main(void)
{
	out_Color = vec4(shade);
}
