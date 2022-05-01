#version 450
// https://www.khronos.org/registry/OpenGL/specs/gl/GLSLangSpec.4.50.pdf
// 셰이더는 꼭 UTF-8 로 저장할 것! https://yam-cha.tistory.com/120
// 버텍스 셰이더는 각각의 버텍스마다 수행된다


// 테스트용 삼각형의 3점의 위치를 저장하기 위해 하드코딩
vec2 positions[3] = vec2[] (
	vec2(0.0, -0.5),
	vec2(0.5, 0.5),
	vec2(-0.5, 0.5)
);
// 테스트용 삼각형의 3점의 칼라값 또한 하드코딩
vec3 colors[3] = vec3[] (
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);


// 버텍스 버퍼로부터 x, y 위치값 전달 받아서
//layout(location = 0) in vec2 position;


// 프레그먼트 셰이더로 컬러값 전달하기
layout(location = 0) out vec3 fragColor;



// 각각의 버텍스마다 수행
void main()
{
	// gl_Position : 버텍스 셰이더의 최종 아웃풋은 gl_Position 에 담기로 약속되어 있음
	// gl_VertexIndex : GLSL의 gl_VertexID와 동일, 처음 렌더링 명령이 시작될 때부터 지금까지 처리된 버텍스가 몇번째인지 담고 있음
	// 0.0 : z 축 위치값
	// 1.0 : w 값 (Clip coordinate 에서 Normalized Device Coordinate 로 가기 위해 최종적으로 모든 위치 스칼라들을 이 수로 나눔)
	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	fragColor = colors[gl_VertexIndex]; // 프레그먼트 셰이더로 전달할때 중간값 보간 자동으로 수행


	// 버텍스 버퍼로부터 x, y 위치값 전달 받아서
	//gl_Position = vec4(position, 0.0, 1.0);
}