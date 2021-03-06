#version 330

in vec3 in_Position;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;

void main(void)
{
    gl_Position = projectionMatrix * modelViewMatrix * vec4(in_Position, 1.0);
}
