#define GLFW_INCLUDE_VULKAN // 이렇게 정의해두면 glfw3.h 에서 vulkan.h 도 같이 불러옵니다
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // OpenGL과 다르게 0 ~ 1 스케일 깊이 값을 사용합니다
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>
#include <stdexcept> // 예외처리
#include <cstdlib> // EXIT_SUCCESS, EXIT_FAILURE 매크로

// 윈도우 해상도
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;


class HelloTriangleApplication
{
private:
    GLFWwindow* window; // GLFW 윈도우
    VkInstance instance; // Vulkan 객체

public:
    void run()
    {
        initWindow();       // 1. GLFW 윈도우 초기화
        initVulkan();       // 2. 불칸 객체 초기화 및 렌더링 준비
        mainLoop();         // 3. 계속해서 매 프레임 렌더
        cleanup();          // 4. 프로그램 종료
    }

private:
    // 1. GLFW 윈도우 초기화
    void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // GLFW 는 OpenGL context 에 기본적으로 최적화 되어 있으므로 Vulkan 용으로 사용하기 위해 GLFW_NO_API 로 명시하였습니다
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // 사용자가 윈도우 창 크기를 마음껏 조절하도록 만들려면 불칸에서 신경써야할 일이 엄청 많아지므로 고정 해상도를 사용하도록 제한하였습니다

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

        // 그래픽카드가 불칸 확장 기능들을 몇개 제공하는지 받아옵니다 | Gets how many Vulkan extensions graphics card can provides
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::cout << extensionCount << " extensions supported\n";
    }


    // 2. 불칸 객체 초기화 및 렌더링 준비
    void initVulkan()
    {
        createInstance();       // 2-1. Vulkan 객체를 만듭니다
    }


    // 2-1. Vulkan 객체를 만듭니다
    void createInstance()
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;

        createInfo.enabledLayerCount = 0;

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }


    // 3. 계속해서 매 프레임 렌더
    void mainLoop()
    {
        // 사용자가 창을 닫을 때까지는 계속 이벤트 처리를 하면서 루프를 돕니다
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

            // @@@ 여기서 매 프레임을 랜더링 할 예정
        }
    }


    // 4. 프로그램 종료
    void cleanup()
    {
        glfwDestroyWindow(window);

        glfwTerminate();


        // std::unique_ptr 나 std::shared_ptr 등을 써서 우아하게 RAII 를 처리할 수도 있지만 학습을 위해 모든 자원을 명시적으로 소멸합니다 | Possible to handle RAII more elegantly with std::unique_ptr or std::shared_ptr etc, but explicitly destroy all resources for learning now.
    }
};


int main()
{
    HelloTriangleApplication app;

    // HelloTriangleApplication 앱을 실행하다 오류 발생시 main 에서 에외를 받습니다 | Exception will propagate back to the main function
    try
    {
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}






/*
// To-do list | 삼각형 하나 그릴려면 앞으로 해야할 일
Create a VkInstance
Select a supported graphics card(VkPhysicalDevice)
Create a VkDeviceand VkQueue for drawingand presentation
Create a window, window surfaceand swap chain
Wrap the swap chain images into VkImageView
Create a render pass that specifies the render targets and usage
Create framebuffers for the render pass
Set up the graphics pipeline
Allocateand record a command buffer with the draw commands for every possible swap chain image
Draw frames by acquiring images, submitting the right draw command bufferand returning the images back to the swap chain


// Coding conventions of Vulkan.h | 불칸 코딩 관습
functions, enumerations and structs are defined in the vulkan.h header
Functions have a lower case vk prefix | 함수는 vk 로 시직합니다
Types like enumerations and structs have a Vk prefix | 타입은 Vk 로 시작합니다
Enumeration values have a VK_ prefix | 이넘(플래그)값은 VK_ 로 시작합니다
Almost all functions return a VkResult that is either VK_SUCCESS or an error code | 대부분의 Vulkan 함수들이 실행 결과로 VK_SUCCESS 나 에러 코드를 반환합니다




*/