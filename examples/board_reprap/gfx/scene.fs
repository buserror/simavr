uniform sampler2DShadow shadowMap ;

// This define the value to move one pixel left or right
uniform vec2 pixelOffset = vec2(1.0 / 1024.0, 1.0 / 1024.0);
uniform sampler2D tex0;
varying vec2 texCoord;

varying vec4 ShadowCoord;


float lookup( vec2 offSet)
{
	// Values are multiplied by ShadowCoord.w because shadow2DProj does a W division for us.
	return shadow2DProj(shadowMap, 
				ShadowCoord + vec4(
						offSet.x * pixelOffset.x * ShadowCoord.w, 
						offSet.y * pixelOffset.y * ShadowCoord.w, 
						0.05, 0.0) ).w;
}

void main()
{	
	// Used to lower moiré pattern and self-shadowing
	//shadowCoordinateWdivide.z += ;
	
	float shadow = 0.0;
	
	// Avoid counter shadow
	if (ShadowCoord.w > 1.0) {
		// Simple lookup, no PCF
		//shadow = lookup(vec2(0.0,0.0));

		// 8x8 kernel PCF
		/*
		float x,y;
		for (y = -3.5 ; y <=3.5 ; y+=1.0)
			for (x = -3.5 ; x <=3.5 ; x+=1.0)
				shadow += lookup(vec2(x,y));
		shadow /= 64.0 ;
		*/

		// 8x8 PCF wide kernel (step is 10 instead of 1)
		/*
		float x,y;
		for (y = -30.5 ; y <=30.5 ; y+=10.0)
			for (x = -30.5 ; x <=30.5 ; x+=10.0)
				shadow += lookup(vec2(x,y));
		shadow /= 64.0 ;
		*/

		// 4x4 kernel PCF
		/*
		float x,y;
		for (y = -1.5 ; y <=1.5 ; y+=1.0)
			for (x = -1.5 ; x <=1.5 ; x+=1.0)
				shadow += lookup(vec2(x,y));
		shadow /= 16.0 ;
		*/
		// 4x4  PCF wide kernel (step is 10 instead of 1)
		
		float x,y;
		for (y = -10.5 ; y <=10.5 ; y+=10.0)
			for (x = -10.5 ; x <=10.5 ; x+=10.0)
				shadow += lookup(vec2(x,y));
		shadow /= 16.0 ;
		
		
		// 4x4  PCF dithered
		/*
		// use modulo to vary the sample pattern
		vec2 o = mod(floor(gl_FragCoord.xy), 2.0);
	
		shadow += lookup(vec2(-1.5, 1.5) + o);
		shadow += lookup(vec2( 0.5, 1.5) + o);
		shadow += lookup(vec2(-1.5, -0.5) + o);
		shadow += lookup(vec2( 0.5, -0.5) + o);
		shadow *= 0.25 ;
		*/
	}
 	vec4 c = (shadow + 0.0) * gl_Color;
  	gl_FragColor = mix(texture2D(tex0, texCoord), vec4(0.0,0.0,0.0,0.9), 0.8-shadow);
}
