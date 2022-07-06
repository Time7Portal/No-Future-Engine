#version 450
// 모든 값들은 버텍스 셰이더에서 프레그먼트 셰이더로 넘어올때 버텍스들의 사이 값(프레그먼트)으로 자동 보간됩니다.



// 프레그먼트 셰이더에서 사용할 텍스쳐 샘플
// 결합된 이미지 샘플러 디스크립터는 GLSL에서 샘플러 유니폼으로 표현됩니다. 프래그먼트 셰이더에서 참조를 추가합니다. 다른 유형의 이미지에 대해 동등한 sampler1D 및 sampler3D 유형이 있습니다. 여기에서 올바른 바인딩을 사용해야 합니다.
layout(binding = 1) uniform sampler2D texSampler;

// 버텍스 셰이더로부터 받은 버텍스 칼라값 (값은 삼각형의 무게중심공식으로 자동 선형 보간되어 들어옴)
// 사실 변수 이름은 달라도 괜찮으며, 결국엔 location 값을 보고 버텍스 셰이더에서 어떤 변수가 넘어오는지 결정되는 것임.
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;


// 버텍스 셰이더와는 다르게 직접 output 을 정의해야 한다 (원하면 여러 로케이션으로 output 보낼 수 있음)
layout(location = 0) out vec4 outColor;



// 도형 내부의 픽셀 하나하나마다 수행
void main()
{
	// 초록색 단색 삼각형
	// outColor = vec4(0.0, 1.0, 0.0, 1.0);

	// 버텍스 칼라값을 보간해서 약하게 적용 + 이미지 텍스쳐 적용한 요란한 삼각형
	// 텍스처는 texture() 라는 빌트인 함수를 사용하여 샘플링됩니다. 샘플러와 좌표를 인수로 사용합니다. 샘플러는 백그라운드에서 필터링 및 변환을 자동으로 처리합니다. 이제 응용 프로그램을 실행할 때 사각형에 텍스처가 표시되어야 합니다.
	// fragColor 를 20% 만 적용하여 약하게 색상을 넣고 (알파는 1로 고정) fragTexCoord 에 2를 곱해서 텍스쳐를 VK_SAMPLER_ADDRESS_MODE_REPEAT 를 시험하였습니다.
	//outColor = ( vec4(fragColor, 1.0) * 0.2) + texture(texSampler, fragTexCoord * 2);
	//outColor = vec4( (fragColor * 0.2) + texture(texSampler, fragTexCoord * 2).rgb , 1.0); // 위랑 같은 코드

	// 순수하게 원본 텍스쳐만 출력
	outColor = texture(texSampler, fragTexCoord);

	// 이제 셰이더에서 이미지에 액세스하는 방법을 알게 되었습니다! 이것은 프레임 버퍼에서도 기록되는 이미지와 결합할 때 매우 강력한 기술입니다. 이러한 이미지를 입력으로 사용하여 3D 세계 내에서 후처리 및 카메라 디스플레이와 같은 멋진 효과를 구현할 수 있습니다.
}