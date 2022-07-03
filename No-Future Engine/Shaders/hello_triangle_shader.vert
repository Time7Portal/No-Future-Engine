#version 450
// https://www.khronos.org/registry/OpenGL/specs/gl/GLSLangSpec.4.50.pdf
// 셰이더는 꼭 UTF-8로 저장해야 합니다! https://yam-cha.tistory.com/120
// 버텍스 셰이더는 각각의 버텍스마다 수행됩니다.

// 데이터를 VkBuffer인 uniformBuffers에 복사하고 버텍스 셰이더에서 유티폰 버퍼 오브젝트 디스크립터를 통해 데이터에 액세스할 수 있습니다.
// 일부 구조 및 함수 호출에서 힌트를 얻었듯이 실제로 여러 디스크립터 세트를 동시에 바인딩하는 것이 가능합니다. 파이프라인 레이아웃을 생성할 때 각 디스크립터 세트에 대한 디스크립터 레이아웃을 지정해야 합니다. 셰이더는 다음과 같은 특정 디스크립터 세트를 참조할 수 있습니다.
// layout(set = 0, binding = 0) uniform UniformBufferObject { ... }
// 이 기능을 사용하여 개체별로 달라지는 디스크립터와 공유되는 디스크립터를 별도의 디스크립터 세트에 넣을 수 있습니다. 이 경우 잠재적으로 더 효율적인 그리기 호출에서 대부분의 디스크립터를 다시 바인딩하는 것을 피할 수 있습니다.
// MVP 매트릭스 전달
layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;


// 버텍스 버퍼로부터 x, y 위치값과 버텍스 컬러를 전달 받습니다.
// vec3 가 아닌 dvec3 64비트 벡터는 여러 슬롯을 사용합니다. 이는 그 뒤에 오는 location의 인덱스가 최소 2 이상 높아야 함을 의미합니다. - https://vulkan-tutorial.com/Vertex_buffers/Vertex_input_description 텍스처 좌표를 통해 프래그먼트 셰이더로 전달하도록 정점 셰이더를 수정해야 합니다.
layout(location = 0) in vec3 inPosition;	// X, Y, Z 위치값
layout(location = 1) in vec3 inColor;		// R, G, B 컬러값
layout(location = 2) in vec2 inTexCoord;	// 텍스쳐 UV 좌표값


// 프레그먼트 셰이더로 컬러값을 전달합니다.
// 버텍스별 색상과 마찬가지로 fragTexCoord 값은 래스터라이저에 의해 정사각형 영역에 걸쳐 부드럽게 보간됩니다. 프래그먼트 셰이더가 텍스처 좌표를 색상으로 출력하도록 하여 이것을 시각화할 수 있습니다.
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;



// 각각의 버텍스마다 수행
void main()
{
	// << 3차 테스트용 셰이더 : 유니폼 버퍼로부터 MVP 변환행렬 받아서 3D 월드 내 평면 출력 >>
	// https://vulkan-tutorial.com/Uniform_buffers/Descriptor_pool_and_sets
	// (ubo 유니폼 버퍼가 담긴 디스크립터 풀) 을 묶은 디스크립터 셋이 바인딩 되어 있어야 에러 없이 실행됨 !!
	// 우리는 이전 장의 직사각형이 3D에서 회전하도록 매 프레임마다 모델, 보기 및 투영 행렬을 업데이트하여 클립 좌표계에서의 위치를 구하고 gl_Position 로 반환할 것입니다.
	// 클립 좌표계이므로 맨 마지막 요소로 vec4(inPosition, 0.0, 1.0) 에 추가된 1.0 는 w 값으로 (Clip coordinate 에서 Normalized Device Coordinate 로 가기 위해 최종적으로 모든 위치 스칼라들을 이 수로 나눔) 나중에 퍼스펙티브 프로젝션(원근 뷰)에 사용하기 위한 나누기 요소로 쓰여서 가까이 있는 오브젝트를 크게 보여주고 멀리 있는 오브젝트는 작게 보이도록 해줍니다.
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
	fragTexCoord = inTexCoord;



	// gl_Position : 버텍스 셰이더의 최종 아웃풋은 gl_Position 에 담기로 약속되어 있음
	// gl_VertexIndex : GLSL의 gl_VertexID와 동일, 처음 렌더링 명령이 시작될 때부터 지금까지 처리된 버텍스가 몇번째인지 담고 있음
}