// Used for shadow lookup
uniform mat4 shadowMatrix;
varying vec4 ShadowCoord;
varying vec2 texCoord;
   
void main()
{
	ShadowCoord= shadowMatrix * gl_Vertex;

	gl_Position = ftransform();
    gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_FrontColor = gl_Color;
	texCoord = gl_MultiTexCoord0.xy;
}

