#version 120
//uniform mat4 g_WorldViewProjectionMatrix;
uniform vec2 g_Resolution = vec2(800,600);
uniform float m_SubPixelShift = 1.0 / 4.0;

varying vec2 texCoord;
varying vec4 posPos;

void main() {
	gl_Position = ftransform();
	gl_TexCoord[0] = gl_MultiTexCoord0;
	texCoord = gl_MultiTexCoord0.xy;
    vec2 rcpFrame = vec2(1.0) / g_Resolution;
    posPos.xy = texCoord;
    posPos.zw = texCoord - (rcpFrame * vec2(0.5 + m_SubPixelShift));
}
