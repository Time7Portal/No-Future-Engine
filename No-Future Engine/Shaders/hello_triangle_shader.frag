#version 450


// 버텍스 셰이더로부터 받은 버텍스 칼라값 (값은 삼각형의 무게중심공식으로 자동 선형 보간되어 들어옴)
// 사실 변수 이름은 달라도 괜찮으며, 결국엔 location 값을 보고 버텍스 셰이더에서 어떤 변수가 넘어오는지 결정되는 것임.
layout(location = 0) in vec3 fragColor;


// 버텍스 셰이더와는 다르게 직접 output 을 정의해야 한다 (원하면 여러 로케이션으로 output 보낼 수 있음)
layout (location = 0) out vec4 outColor;


// 도형 내부의 픽셀 하나하나마다 수행
void main()
{
	// 초록색 단색 삼각형
	// outColor = vec4(0.0, 1.0, 0.0, 1.0);

	// 버텍스 칼라값을 보간한 요란한 삼각형
	outColor = vec4(fragColor, 1.0);
}