#version 330

uniform sampler2D textureUnit;
uniform sampler2D transShadowsTexUnit;
in vec4 lightSourceCoord;
out vec4 out_Color;
uniform vec4 shade;
uniform float alpha;

void main()
{
	// Perform perspective division to get the actual texture position
	vec4 shadowCoordinateWdivide = lightSourceCoord / lightSourceCoord.w;

	// Used to lower moire' pattern and self-shadowing
	// The optimal value here will vary with different GPU's depending on their Z buffer resolution.
	shadowCoordinateWdivide.z -= 0.0005; // 0.0005;

	float shadow = 1.0;
	float dx, dy;
	float transDistanceFromLight;
	float distanceFromLight;

	for (dy = -3.5 ; dy <= 3.5 ; dy+=1.0)
	{
		for (dx = -3.5 ; dx <= 3.5 ; dx+=1.0)
		{
			distanceFromLight = texture(textureUnit, shadowCoordinateWdivide.st + vec2(dx, dy)/1000.0).x;
			distanceFromLight = (distanceFromLight-0.5) * 2.0;

			transDistanceFromLight = texture(transShadowsTexUnit, shadowCoordinateWdivide.st + vec2(dx, dy)/1000.0).x;
			transDistanceFromLight = (transDistanceFromLight-0.5) * 2.0;

			if (lightSourceCoord.w > 0.0)
			{
				if (distanceFromLight < shadowCoordinateWdivide.z) // shadow
				{
					shadow -= 0.6/64; // = 0.5 shadow if total shadow (for 64 samples)
				}
				else if(transDistanceFromLight < shadowCoordinateWdivide.z)
				{
					shadow -= 0.15/64;
				}
			}
		}
	}

	out_Color = vec4(shadow*shade[0], shadow*shade[1], shadow*shade[2], shadow*shade[3]);

	//out_Color = vec4(alpha);

}
