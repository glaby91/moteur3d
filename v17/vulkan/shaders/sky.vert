#version 450
// Fullscreen triangle drawn without vertex buffers, as in the V17 DX11 sky pass.
void main(){
 vec2 ndc=gl_VertexIndex==0?vec2(-1.0,-1.0):
          gl_VertexIndex==1?vec2(-1.0,3.0):vec2(3.0,-1.0);
 gl_Position=vec4(ndc,0.0,1.0);
}
