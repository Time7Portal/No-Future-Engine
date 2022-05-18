# No-Future-Engine

주의사항 - 가볍게 만들 것 (목표를 작게 가지자)



# Development log | 업데이트 내역

### 2022-04-23

glfw 윈도우를 띄웠습니다.

### 2022-04-24

Vulkan 개체를 만들고 초기화했습니다.

검증 레이어를 삽입했습니다.

그래픽카드 선택 기능을 넣었습니다.

### 2022-04-25

큐를 그래픽카드에 전달하기 위해 논리적 그래픽 디바이스를 만들었습니다.

### 2022-04-26

화면 표시를 위한 서피스를 생성하고 GLFW 윈도우에 연결하였습니다.

### 2022-04-30

스왑 체인을 구성하였습니다.

스왑 체이닝을 위한 이미지 뷰를 만들었습니다.

### 2022-05-01

코드를 깔끔하게 정리했습니다.

테스트용 셰이더를 만들고 셰이더 파일을 읽어들였습니다.

### 2022-05-02

렌더 패스를 구성했습니다.

그래픽스 파이프라인을 완성했습니다.

프레임 버퍼를 구성했습니다.

### 2022-05-03

CPU / GPU 동기화 개체를 만들었습니다.

커맨드 버퍼를 구성하고 커멘드 풀로 제출했습니다.

Hello world 삼각형을 성공적으로 그렸습니다.

파이프라이닝을 개선하여 효율적으로 GPU를 굴리기 위해 대기 없이 미리 CPU 에서 사전 렌더링을 수행하는 2개의 프레임을 만들었습니다.

#### - 이사와 짐정리 기간 (휴식) -

### 2022-05-16

GLFW 윈도우의 크기가 조정될때 불칸의 스왑체인(프레임 버퍼들)을 다시 구성하도록 만들었습니다.

GLFW 윈도우가 최소화 될 때 렌더링을 수행하지 않도록 최적화했습니다.

BUGFIX : 프로그램 종료시 클린업 과정에 스왑체인 관련 중복 소멸 에러를 수정했습니다.

### 2022-05-18

셰이더코드에 하드코딩된 버텍스 위치/컬러값을 지웠으며, 버텍스 버퍼를 만들어 커맨드 버퍼로 전달하였습니다.



# References | 참고자료

https://vulkan-tutorial.com/

https://www.vulkan.org/


