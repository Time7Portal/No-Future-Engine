#version 450
// https://www.khronos.org/registry/OpenGL/specs/gl/GLSLangSpec.4.50.pdf
// 셰이더는 꼭 UTF-8 로 저장할 것! https://yam-cha.tistory.com/120
// 버텍스 셰이더는 각각의 버텍스마다 수행된다



// 테스트용 삼각형의 3점의 위치를 보여주기 위해 하드코딩
//vec2 positions[3] = vec2[] (
//	vec2(0.0, -0.5),
//	vec2(0.5, 0.5),
//	vec2(-0.5, 0.5)
//);
// 테스트용 삼각형의 3점의 칼라값 또한 하드코딩
//vec3 colors[3] = vec3[] (
//    vec3(1.0, 0.0, 0.0),
//    vec3(0.0, 1.0, 0.0),
//    vec3(0.0, 0.0, 1.0)
//);



// 데이터를 VkBuffer인 uniformBuffers에 복사하고 버텍스 셰이더에서 유티폰 버퍼 오브젝트 디스크립터를 통해 데이터에 액세스할 수 있습니다.
layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;


// 버텍스 버퍼로부터 x, y 위치값과 버텍스 컬러를 전달 받습니다.
// vec3 가 아닌 dvec3 64비트 벡터는 여러 슬롯을 사용합니다. 이는 그 뒤에 오는 location의 인덱스가 최소 2 이상 높아야 함을 의미합니다. - https://vulkan-tutorial.com/Vertex_buffers/Vertex_input_description
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;


// 프레그먼트 셰이더로 컬러값을 전달합니다.
layout(location = 0) out vec3 fragColor;



// 각각의 버텍스마다 수행
void main()
{
	// << 1차 테스트용 셰이더 : 하드코딩된 테스트 3각형 출력 >>
	// gl_Position : 버텍스 셰이더의 최종 아웃풋은 gl_Position 에 담기로 약속되어 있음
	// gl_VertexIndex : GLSL의 gl_VertexID와 동일, 처음 렌더링 명령이 시작될 때부터 지금까지 처리된 버텍스가 몇번째인지 담고 있음
	// 0.0 : z 축 위치값
	// 1.0 : w 값 (Clip coordinate 에서 Normalized Device Coordinate 로 가기 위해 최종적으로 모든 위치 스칼라들을 이 수로 나눔)
	
	// 하드코딩된 테스트 삼각형 출력용
	//gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	//fragColor = colors[gl_VertexIndex]; // 프레그먼트 셰이더로 전달할때 중간값 보간 자동으로 수행

	// << 2차 테스트용 셰이더 : 버텍스 버퍼로부터 받아서 삼각형, 사각형 출력 >>
	// 버텍스 버퍼로부터 x, y 위치값 전달 받아서 출력
	//gl_Position = vec4(inPosition, 0.0, 1.0);
	//fragColor = inColor;


	// << 3차 테스트용 셰이더 : 유니폼 버퍼로부터 MVP 변환행렬 받아서 3D 월드 내 평면 출력 >>
	// https://vulkan-tutorial.com/Uniform_buffers/Descriptor_pool_and_sets
	// (ubo 유니폼 버퍼가 담긴 디스크립터 풀) 을 묶은 디스크립터 셋이 바인딩 되어 있어야 에러 없이 실행됨 !!
	// 우리는 이전 장의 직사각형이 3D에서 회전하도록 매 프레임마다 모델, 보기 및 투영 행렬을 업데이트하여 클립 좌표계에서의 위치를 구하고 gl_Position 로 반환할 것입니다.
	// 클립 좌표계이므로 맨 마지막 요소로 vec4(inPosition, 0.0, 1.0) 에 추가된 1.0 는 나중에 퍼스펙티브 프로젝션(원근 뷰)에 사용하기 위한 나누기 요소로 쓰여서 가까이 있는 오브젝트를 크게 보여주고 멀리 있는 오브젝트는 작게 보이도록 해줍니다.
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}