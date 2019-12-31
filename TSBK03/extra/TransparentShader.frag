#version 330

uniform sampler2D textureUnit;
in vec4 lightSourceCoord;
out vec4 out_Color;
uniform vec3 shade;
uniform float alpha;

void main()
{
	// Perform perspective division to get the actual texture position
	vec4 shadowCoordinateWdivide = lightSourceCoord / lightSourceCoord.w;

	// Used to lower moire' pattern and self-shadowing
	// The optimal value here will vary with different GPU's depending on their Z buffer resolution.
	shadowCoordinateWdivide.z -= 0.0002; // 0.0005;

	float shadow = 1.0;
	float dx, dy;
	for (dy = -3.5 ; dy <=3.5 ; dy+=1.0)
	{
		for (dx = -3.5 ; dx <=3.5 ; dx+=1.0)
		{
			float distanceFromLight = texture(textureUnit, shadowCoordinateWdivide.st + vec2(dx, dy)/250.0).x;
			distanceFromLight = (distanceFromLight-0.5) * 2.0;

			if (lightSourceCoord.w > 0.0)
				if (distanceFromLight < shadowCoordinateWdivide.z) // shadow
					shadow -= 0.5/64; // = 0.5 shadow if total shadow (for 64 samples)
		}
	}
	
	out_Color = vec4(shade[0], shade[1], shade[2], alpha); //shadow*

}
