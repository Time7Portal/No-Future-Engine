#define GLFW_INCLUDE_VULKAN // 이렇게 정의해두면 glfw3.h 에서 vulkan.h 도 같이 불러옵니다.
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // OpenGL과 다르게 0 ~ 1 스케일 깊이 값을 사용합니다.
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>         // 
#include <fstream>          // 셰이더 소스를 읽기 위함
#include <stdexcept>        // 예외처리
#include <algorithm>        // std::clamp 사용
#include <vector>           // 
#include <cstring>          // strcmp 사용
#include <cstdlib>          // EXIT_SUCCESS, EXIT_FAILURE 매크로
#include <cstdint>          // uint32_t 사용
#include <limits>           // std::numeric_limits 사용
#include <optional>         // 그래픽카드가 해당 큐 패밀리를 지원하는지 여부 검사
#include <set>              // 사용할 모든 큐 패밀리 셋을 모아서 관리

#define HELPER_FUNCTION inline    // 코드 가독성을 위해 핼퍼 함수 표시용


// 윈도우 해상도
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

// 필요한 검증 레이어 목록
const std::vector<const char*> validationLayers {
    "VK_LAYER_KHRONOS_validation", // LunarG Vulkan SDK 에서 기본적으로 제공하는 유효성 검사 레이어
};

// 필요한 확장 기능 목록
const std::vector<const char*> deviceExtensions {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME, // 모든 그래픽 카드가 화면 출력을 지원하지는 않으므로 불칸은 스왑 체인을 확장 기능으로 만들었습니다. VK_KHR_swapchain 을 장치가 지원하는지 확인하고 이 확장을 추가해야 합니다.
};

// 디버그 관련
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true; // 디버그 모드일때만 검증 레이어 활성화
#endif


// PFN_vkCreateDebugUtilsMessengerEXT 함수는 디버깅 정보 출력용 메신저 도구 개체를 만들때 사용하며, 안타깝게도 확장 함수이기 때문에 자동으로 로드되지 않습니다. vkGetInstanceProcAddr 를 사용하여 함수 주소를 직접 찾아야 합니다. 이를 처리하는 자체 프록시(래퍼) 함수를 만들 것입니다.
HELPER_FUNCTION VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        // 확장 함수를 로드할 수 없으면 nullptr 를 반환합니다.
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}


// PFN_vkDestroyDebugUtilsMessengerEXT 함수도 마찬가지로 확장 함수라서 함수 주소를 직접 받아서 사용해야 합니다. 디버깅 정보 출력용 메신저 도구 개체의 소멸을 책임집니다.
HELPER_FUNCTION void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}


// 그래픽카드가 필요한 큐 패밀리를 지원하는지 한꺼번에 정리해서 검사하기 위한 구조체
struct QueueFamilyIndices
{
    // 적합한 큐 패밀리가 존재하지 않음을 매직 넘버 (예를들어 0) 으로 표현하는 것은 불가능하므로 C++17 문법중에 std::optional 을 사용하여 값이 존재하거나 존재하지 않음을 표현하였습니다. std::optional 는 무언가를 할당하기 전에는 값을 가지지 않습니다. https://modoocode.com/309
    // graphicsFamily 에는 그래픽카드가 해당 큐 패밀리를 지원하는지 여부와 지원한다면 해당 큐 페밀리의 인덱스 번호를 담고 있습니다.
    std::optional<uint32_t> graphicsFamily;
    // presentFamily 에는 그래픽카드가 해당 프레젠테이션 큐 페밀리를 지원하는지 여부와 지원한다면 해당 큐 페밀리의 인덱스 번호를 담고 있습니다.
    std::optional<uint32_t> presentFamily;

    // 이 함수를 호출해서 우리가 원하는 모든 패밀리를 얻었는지 확인할 수 있습니다.
    bool isComplete()
    {
        // graphicsFamily 에 한번이라도 값을 넣은 적이 있다면 has_value() 는 true 를 반환합니다.
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};


// 스왑 체인을 사용하려면 기본적으로 확인해야 하는 세 가지 유형의 속성이 있습니다.
struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;      // 1) 기본 서피스 기능 (스왑 체인의 최소/최대 이미지 수, 이미지의 최소/최대 너비 및 높이)
    std::vector<VkSurfaceFormatKHR> formats;    // 2) 서피스 형식 (픽셀 형식, 색 공간)
    std::vector<VkPresentModeKHR> presentModes; // 3) 사용 가능한 프레젠테이션 모드
};



// 실제 어플리케이션 클래스
class HelloTriangleApplication
{
private:
    GLFWwindow* window;                                 // GLFW 윈도우 핸들
    VkInstance instance;                                // Vulkan 개체 핸들
    VkDebugUtilsMessengerEXT debugMessenger;            // 디버깅 정보 출력용 메신저 도구 핸들. 놀랍게도 Vulkan의 디버그 콜백조차도 명시적으로 생성 및 소멸되어야 하는 핸들로 관리됩니다. 이러한 콜백은 디버그 메신저 의 일부이며 원하는 만큼 가질 수 있습니다.
    VkSurfaceKHR surface;                               // 추상적 화면 객체 핸들. 이 개체는 플랫폼(Windows, Linux, Android 등)에 독립적으로 화면을 관리할 수 있도록 도와줍니다. 우리 프로그램은 GLFW 로 생성한 화면을 이용합니다.

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;   // 선택된 그래픽카드 디바이스 핸들. vkInstance 와 함께 자동으로 소멸됩니다.
    VkDevice device;                                    // 추상적 디바이스 핸들. 그래픽카드와 통신하기 위한 인터페이스 입니다. 하나의 그래픽카드에 여러개의 추상적 디바이스를 만들 수도 있습니다.

    VkQueue graphicsQueue;                              // 그래픽 큐 핸들. 사실 큐는 추상적 디바이스를 만들때 같이 만들어집니다. 하지만 만들어질 그래픽 큐를 다룰 수 있는 핸들을 따로 만들어 관리해야 합니다. VkDevice 와 함께 자동으로 소멸됩니다.
    VkQueue presentQueue;                               // 프레젠테이션 큐 핸들. 화면에 결과물을 보여주기 위해 사용됩니다.

    VkSwapchainKHR swapChain;                           // 불칸은 기본 프레임버퍼라는 개념이 없으므로 화면에 렌더링 하기 전에 이 버퍼를 소유할 인프라가 필요하며 이 인프라를 스왑 체인이라고 합니다. 스왑 체인은 기본적으로 화면에 표기되기를 기다리는 이미지 큐입니다. 응용 프로그램은 랜더링할 이미지를 다 그리면 이 큐에 넣습니다. 스왑 체인의 일반적인 목적은 이미지 표시를 화면의 새로 고침 빈도와 동기화하는 것입니다. 사실 프레젠테이션 큐가 지원되는 그래픽 카드에서는 스왑 체인 확장 기능도 반드시 지원할 것입니다.
    std::vector<VkImage> swapChainImages;               // 이제 스왑 체인이 생성되었으므로 남은 것은 그 안에 있는 스왑 체인용 이미지들(VkImages) 의 핸들을 받는 것입니다. 렌더링 작업 중에 이를 참조할 것입니다. 이미지는 스왑 체인 구현과 함께 생성하며 스왑 체인이 파괴되면 자동으로 소멸됩니다.
    VkFormat swapChainImageFormat;                      // 지정한 스왑 체인 이미지 형식
    VkExtent2D swapChainExtent;                         // 지정한 스왑 체인 이미지 크기
    std::vector<VkImageView> swapChainImageViews;       // 스왑 체인용 이미지 뷰들의 핸들 모음

    VkRenderPass renderPass;                            // 렌더 패스 핸들
    VkPipelineLayout pipelineLayout;                    // 파이프라인 레이아웃 핸들
    VkPipeline graphicsPipeline;                        // 그래픽스 파이프라인 핸들

public:
    void run()
    {
        initWindow();       // 1. GLFW 윈도우 초기화

        initVulkan();       // 2. 불칸 개체 초기화 및 렌더링 준비

        mainLoop();         // 3. 계속해서 매 프레임 렌더

        cleanup();          // 4. 프로그램 종료
    }



private:
    // 1. GLFW 윈도우 초기화
    inline void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // GLFW 는 OpenGL context 에 기본적으로 최적화 되어 있으므로 Vulkan 용으로 사용하기 위해 GLFW_NO_API 로 명시하였습니다.
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // 사용자가 윈도우 창 크기를 마음껏 조절하도록 만들려면 불칸에서 신경써야할 일이 엄청 많아지므로 고정 해상도를 사용하도록 제한하였습니다.

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

        // 인스턴스를 생성하기 전에 그래픽카드가 지원하는 불칸 확장 기능들을 확인합니다. | Gets how many Vulkan extensions graphics card can provides.
        // 몇개의 확장 기능을 지원하는지 갯수를 우선 받은 후에
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::cout << extensionCount << " extensions supported\n";
        // 그 갯수 정보를 활용해서 지원 목록을 받아옵니다.
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
        // 각 VkExtensionProperties구조체에는 확장의 이름과 버전이 포함됩니다. 간단한 for 루프로 나열할 수 있습니다. (\t 는 들여쓰기)
        std::cout << "Available extensions:\n";
        for (const auto& extension : extensions)
        {
            std::cout << '\t' << extension.extensionName << '\n';
        }
    }



    // 2. 불칸 개체 초기화 및 렌더링 준비
    inline void initVulkan()
    {
        createInstance();           // 2-1. Vulkan 개체 만들기

        createSurface();            // 2-2. 화면 표시를 위한 서피스 생성 및 GLFW 윈도우에 연결

        pickPhysicalDevice();       // 2-3. 그래픽 카드 선택

        createLogicalDevice();      // 2-4. 그래픽 카드와 통신하기 위한 인터페이스 생성

        createSwapChain();          // 2-5. 이미지 버퍼를 어떤 방식으로 동기화하면서 화면에 표시할지 스왑 체인 규약과 스왑 체인용 이미지 생성
       
        createImageViews();         // 2-6. 스왑 체인용 이미지 사용 방식을 정의하기 위한 이미지 뷰 생성

        createRenderPass();         // 2-7. 렌더 패스 생성

        createGraphicsPipeline();   // 2-8. 셰이더 로드 및 그래픽스 파이프라인 생성

        //createFramebuffers();       // 2-9.

        //createCommandPool();        // 2-10. 그래픽 카드로 보낼 명령 풀 생성 : 추후 command buffer allocation 에 사용할 예정

        //createCommandBuffers();     // 2-11. 

        //createSyncObjects();        // 2-12. 

    }



    // 2-1. Vulkan 개체를 만들기
    inline void createInstance()
    {
        // 우선 디버그 모드라면 필요로 하는 검증 레이어를 지원하는지 확인합니다.
        if (enableValidationLayers == true)
        {
            // 지원 가능한 검증 레이어 갯수 가져오기
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

            // 지원 가능한 검증 레이어 목록 가져오기
            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            // 지원 가능한 검증 레이어 목록을 조회하여, 필요로 하는 검증 레이어가 전부 들어있는지 확인합니다. | Checks if all of the requested layers are available.
            for (const char* layerName : validationLayers) // 우리가 필요로 하는 검증 레이어들 중
            {
                bool layerFound = false;
                for (const auto& layerProperties : availableLayers) // 장치에서 지원 가능한 검증 레이어 목록을 조회합니다.
                {
                    if (strcmp(layerName, layerProperties.layerName) == 0)
                    {
                        // 원하는 레이어를 찾음 (그럼 이어서 다음 항목 검사)
                        layerFound = true;
                        break;
                    }
                }

                // 끝까지 뒤져봤지만 필요로 하는 레이어를 못 찾았으면 에러!
                if (!layerFound)
                {
                    // 다른 노트북으로 테스트 해보니 그래픽카드가 Vulkan 을 지원하더라도 Vulakn SDK 가 설치되어 있지 않으면 Validation layer 가 없다는 것을 알았습니다. 그래서 에러 문구를 센스있게 바꿨습니다.
                    throw std::runtime_error("Validation layers requested, but not available! - Did you install Vulkan SDK?");
                }
            }
        }

        // 앱에 대한 정보를 제공합니다. (필수는 아니지만 이 정보를 가지고 그래픽 드라이버가 최적화 하는데 사용하기도 합니다.)
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; // 사실상 대부분의 불칸 구조체는 sType 이라는 멤버로 가지고 형식을 파악합니다. (그냥 appInfo 타입을 보고 판단하면 되는데 굳이 이렇게 만든 이유가??)
        appInfo.pApplicationName = "Hello Triangle"; // 앱 이름
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // 앱 버전
        appInfo.pEngineName = "No-Future Engine"; // 엔진 이름
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0); // 엔진 버전
        appInfo.apiVersion = VK_API_VERSION_1_0; // 사용할 불칸 API 버전
        appInfo.pNext = nullptr; // 미래에 사용할지도 모르는 확장 구조체 연결 목록을 전달하려는 용도입니다.

        // VkInstance는 VkInstanceCreateInfo에 의해서 생성됩니다. 구조체의 변수를 통해 어플리케이션의 정보와 어떤 Vulkan Layer 들로 Vulkan Layer Chain 을 구성할지 정의합니다. 불칸의 많은 정보는 이처럼 함수 매개변수 대신 구조체를 통해 전달되며 인스턴스 생성을 위한 충분한 정보를 제공하려면 아래 구조체를 하나 더 채워야 합니다. 다음 구조체는 사용하려는 전역 확장 및 유효성 검사 계층을 Vulkan 드라이버에 알려줍니다. 여기에서 전역이란 특정 장치가 아니라 전체 프로그램에 적용된다는 것을 의미합니다.
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // Vulkan은 플랫폼에 구애받지 않는 API이므로 윈도우 시스템과 상호작용 하려면 확장이 필요합니다. GLFW에는 필요한 확장을 반환하는 편리한 내장 함수가 있습니다.
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::cout << "Used extensions from GLFW:\n";
        for (int i = 0; i < glfwExtensionCount; i++)
        {
            std::cout << '\t' << glfwExtensions[i] << '\n';
        }

        // 확장성을 위해 GLFW 표준 확장(필수 요소들)을 우선 vector 에 넣어서 관리합니다.
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount); // std::vector<>(이터레이터begin, 이터레이터end)

        // 디버그 모드일 경우
        if (enableValidationLayers)
        {
            // 검증 레이어는 기본적으로 표준 디버그 메세지 출력을 제공하지만 프로그램에서 명시적 콜백을 제공하여 직접 처리할 수도 있습니다. 또한 모든 메시지가 반드시 (치명적인) 오류가 아니기 때문에 보고 싶은 메시지 종류도 결정할 수 있습니다. 메시지 및 관련 세부 정보를 처리하기 위해 프로그램에서 콜백을 설정하려면 VK_EXT_debug_utils 확장을 사용하고 콜백 함수도 등록해야 합니다.
            // The validation layers will print debug messages to the standard output by default, but we can also handle them ourselves by providing an explicit callback in our program. This will also allow you to decide which kind of messages you would like to see, because not all are necessarily (fatal) errors. To set up a callback in the program to handle messages and the associated details, we have to set up a debug messenger with a callback using the VK_EXT_debug_utils extension.

            // 디버깅용 메신저 확장을 추가합니다. VK_EXT_DEBUG_UTILS_EXTENSION_NAME 는 VK_EXT_debug_utils 와 같습니다.
            // https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap50.html#VK_EXT_debug_utils
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        // 활성화할 전역 검증 레이어를 설정합니다. 검증 레이어는 Vulkan API를 가로채서 로깅, 프로파일링, 디버깅, 혹은 다른 추가적인 기능에 사용될 수 있습니다.
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // 디버그 모드라면 디버깅을 도와주는 Vulkan 툴중 하나인 디버그 메신저를 설정합니다.
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers)
        {
            // 검증 레이어도 넣고
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            // 디버그 메신저도 설정하여 createInfo 에 집어넣습니다. 이때 우리가 만든 디버그 콜백 함수를 호출할 조건들을 필터링할 수 있습니다.
            debugCreateInfo = {};
            debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = debugCallback; // 디버그 메신저가 메시지 출력을 위해 사용할 콜백 함수를 등록합니다.
            debugCreateInfo.pUserData = nullptr; // (옵션) 콜백 함수에 전달할 필드에 대한 포인터를 선택적으로 전달할 수 있습니다. 예를 들면 이것을 사용하여 HelloTriangleApplication클래스에 대한 포인터를 전달할 수 있습니다.
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo; // 이 방법으로 추가 디버그 메신저를 생성하면 vkCreateInstance 할때 해당 메신저를 사용하고 vkDestroyInstance 할때 자동으로 정리됩니다.
        }
        else // 디버그 모드가 아니면 검증 레이어를 다 빼버립니다.
        {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }


        // 이제 Vulkan이 인스턴스를 생성하는 데 필요한 모든 것을 설정했으며 마침내 vkCreateInstance호출을 실행할 수 있습니다. (VkInstanceCreateInfo 의 포인터, 메모리를 직접 관리하기 위한 커스텀 얼로케이터 콜백들, 불칸 인스턴스 핸들 포인터)
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create instance!");
        }

        // 불칸 인스턴스가 정상적으로 생성된 이후에 위에서 설정했던 디버깅 툴인 디버그 메신저를 활성화할 수 있습니다. 따라서 vkCreateInstance 와 vkDestroyInstance 단계를 디버깅 할 수 없는 상황이지만, createInfo 를 설정할때 pNext 로 debugCreateInfo 핸들을 우선적으로 넘겨서 해결 하였습니다.
        // https://github.com/KhronosGroup/Vulkan-Docs/blob/master/appendices/VK_EXT_debug_utils.txt#L120
        if (enableValidationLayers)
        {
            // 디버그 메신저 생성을 위한 PFN_vkCreateDebugUtilsMessengerEXT 확장 함수를 사용해서 확장 개체를 만듭니다
            // PFN_vkCreateDebugUtilsMessengerEXT 는 확장 함수이기 때문에 직접 콜이 불가능해 문서의 맨 위쪽에 만들어둔 프록시(래퍼) 함수를 대신 사용합니다.
            if (CreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to set up debug messenger!");
            }
        }
    }



    // 디버깅용 콜백 함수는 static 멤버 함수로 만들어야 하며 PFN_vkDebugUtilsMessengerCallbackEXT 의 프로토타입이어야 합니다. VKAPI_ATTR 와 VKAPI_CALL 가 함수의 시그니쳐가 올바르도록 보장하며 Vulkan이 이 함수를 콜할 수 있게 합니다. debugCallback(메세지의 심각도, 메세지의 특성, 메세지 내용, 특수 데이터 전달용)
    // Needs to be a new static member function called debugCallback with the PFN_vkDebugUtilsMessengerCallbackEXT prototype. The VKAPI_ATTR and VKAPI_CALL ensure that the function has the right signature for Vulkan to call it.
    HELPER_FUNCTION static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        // 심각도에 따른 검증 레이어 메세지 출력
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            // 여기에 중단점을 추가하면 쉽게 디버깅이 가능합니다.
            // 콘솔에 빨간 글씨 출력 방법 - https://stackoverflow.com/questions/2616906/how-do-i-output-coloured-text-to-a-linux-terminal
            std::cerr << "\033[1;31m" << "@ [ERROR] : " << pCallbackData->pMessage << "\033[0m\n" << std::endl;
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            std::cerr << "\033[1;33m" << "@ [WARNING] : " << pCallbackData->pMessage << "\033[0m\n" << std::endl;
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
        {
            //std::cerr << "@ [NOTICE] : " << pCallbackData->pMessage << std::endl;
        }
        else
        {
            std::cerr << "@ [INFO] : " << pCallbackData->pMessage << std::endl;
        }
        
        return VK_FALSE; // 디버깅용 콜백 함수는 반드시 VK_FALSE 를 반환하도록 약속되어 있습니다.

        /*
        검증 레이어는 다음과 같은 기능을 제공합니다. | Validation layer provides following things.
        파라미터 값 오사용 감지 | Checking the values of parameters against the specification to detect misuse
        메모리 릭 감지 | Tracking creation and destruction of objects to find resource leaks
        스레드 안전성 확인 | Checking thread safety by tracking the threads that calls originate from
        모든 콜과 파라미터값 출력 | Logging every call and its parameters to the standard output
        불칸 콜 추적 | Tracing Vulkan calls for profiling and replaying

        messageSeverity 플래그는 다음과 같은 정보를 제공합니다.
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: 리소스 생성과 같은 정보 메시지
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: 진단 메시지
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: 버그일 가능성이 높은 동작에 대한 메시지
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: 유효하지 않고 충돌을 일으킬 수 있는 동작에 대한 메시지

        messageType 플래그는 다음과 같은 정보를 제공합니다.
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: 사양이나 성능과 무관한 일반 이벤트가 발생한 경우
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: 사양을 위반하거나 잘못된 동작일 가능성이 있음
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: 잠재적으로 최적화되지 않은 Vulkan 사용 방식

        pCallbackData 구조체는 다음과 같은 정보를 제공합니다.
        pMessage: null로 끝나는 문자열로서의 디버그 메시지
        pObjects: 메시지와 관련된 Vulkan 개체 핸들의 배열
        objectCount: 배열의 개체 수

        pUserData 보이드 포인터는 다음과 같은 정보를 제공합니다.
        Vulkan 개체 초기화 단계에서 디버그 메신저를 설정할때 지정해둔 고유한 포인터를 그대로 전달받습니다.

        더 자세한 디버깅 레이어 설정은
        No-Future Engine\EXTERNALS\VulkanSDK_1.3.211.0\Config\vk_layer_settings.txt
        에 나와있습니다.
        */
    }
    


    // 2-2. 화면 표시를 위한 서피스 생성 및 GLFW 윈도우에 연결
    inline void createSurface()
    {
        // Vulkan은 플랫폼에 구애받지 않는 API이므로 자체적으로 창 시스템과 직접 인터페이스할 수 없습니다. Vulkan과 윈도우 시스템 간의 연결을 설정하여 결과를 화면에 표시하려면 WSI(Window System Integration) 확장을 사용해야 합니다. surface 개채를 생성하기 위해선 VK_KHR_surface 확장이 필요한데 이미 GLFW 에서 제공하는 glfwGetRequiredInstanceExtensions 를 통해 해당 확장을 열었으므로 안심해도 됩니다. surface 는 사용할 그래픽카드 선택에 따라 영향을 받을 수 있으므로 불칸 인스턴스 생성 직후에 생성해야 합니다. OpenGL 과는 다르게 화면 구성은 Vulkan 에선 단지 선택적 요소입니다. 지금은 윈도우 환경이므로 glfwCreateWindowSurface 를 호출하면 GLFW 내부적으로 알아서 VkWin32SurfaceCreateInfoKHR 로 윈도우 서피스를 만들고 VK_KHR_win32_surface 확장을 사용하여 윈도우의 HWND 와 HMODULE 를 처리할 것입니다. glfwCreateWindowSurface(VkInstance, GLFW window pointer, custom allocators, pointer to VkSurfaceKHR handle)
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create window surface!");
        }
    }



    // 2-3. 그래픽 카드 선택
    inline void pickPhysicalDevice()
    {
        // 2-3-1. 우선 Vulkan을 지원하는 그래픽카드 갯수를 받아옵니다.
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        // Vulkan을 지원하는 그래픽카드가 하나도 없으면 에러!
        if (deviceCount == 0)
        {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }

        // 그래픽카드 리스트를 전부 받아옵니다.
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        // 요구사항을 만족하는 그래픽카드가 존재하는지 확인합니다.
        for (const auto& device : devices)
        {
            // 우리가 사용할 그래픽카드가 필요한 큐 패밀리, 확장, 스왑체인을 가지고 있는지 검사하는 함수가 필요합니다. 함수가 멀리 떨어져 있으면 가독성을 해칠 수 있어서 isDeviceSuitable 이라는 static 람다함수로 만들었습니다.
            static auto isDeviceSuitable = [&](VkPhysicalDevice device) -> bool
            {
                // 2-3-2. 해당 그래픽카드가 필요한 큐 패밀리를 모두 지원하는지 확인해야 합니다.
                QueueFamilyIndices indices = findQueueFamilies(device);


                // 2-3-3. 해당 그래픽카드가 필요한 확장 기능들을 모두 지원하는지 확인해야 합니다. 그래픽카드가 우리가 필요로 하는 모든 확장 기능들을 지원하는지 확인하고 그런 경우 extensionsSupported 에 true 를 저장합니다.
                // 우선 그래픽카드가 지원하는 확장 기능 갯수를 먼저 받아옵니다.
                uint32_t extensionCount;
                vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

                // 받아온 확장 기능 갯수를 사용해 이번엔 그래픽카드가 지원하는 모든 확장 기능 리스트를 받아옵니다.
                std::vector<VkExtensionProperties> availableExtensions(extensionCount);
                vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

                // requiredExtensions 는 확인이 필요한 필요한 확장 기능 체크리스트 입니다. 그래픽카드가 지원하는 모든 확장 기능 리스트를 순회하면서 우리가 필요로 하는 확장 기능들을 전부 지원하는지 하나씩 지워나가며 확인할 것입니다.
                std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
                for (const auto& extension : availableExtensions)
                {
                    requiredExtensions.erase(extension.extensionName);
                }

                // 필요한 모든 확장 기능들이 모두 지워졌으면 (모두 확인되었으면) extensionsSupported 에 true 를 저장합니다.
                bool extensionsSupported = requiredExtensions.empty();


                // 2-3-4. 해당 그래픽카드가 필요한 스왑 체인 기능들을 모두 지원하는지 확인해야 합니다.
                bool swapChainAdequate = false;
                if (extensionsSupported)
                {
                    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
                    // 현재로썬 윈도우 서피스에 하나 이상의 이미지 포멧과 하나 이상의 프레젠테이션 모드를 지원한다면 충분합니다.
                    swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
                }


                // 2-3-5. 그래픽카드가 우리가 필요로 하는 모든 기능들을 제공함을 확인하였습니다.
                return indices.isComplete() && extensionsSupported && swapChainAdequate;
            };


            // 우리가 사용할 그래픽카드가 필요한 큐 패밀리, 확장, 스왑체인을 전부 지원한다면
            if (isDeviceSuitable(device) == true) 
            {
                // 적합한 그래픽카드를 하나 찾았으므로 핸들에 보관해두고 더이상 탐색하지 않습니다.
                physicalDevice = device;
                break;
            }
        }

        // 요구사항을 만족하는 그래픽카드를 찾지 못했을 경우
        if (physicalDevice == VK_NULL_HANDLE)
        {
            // 에러를 반환하고 프로그램을 종료합니다!
            throw std::runtime_error("Failed to find a suitable GPU!");
        }

        // 2-3-6. 찾은 그래픽카드 정보를 모아서 출력합니다.
        // 이름, 유형 및 지원되는 Vulkan 버전과 같은 기본 장치 속성 등 | Basic device properties like the name, type and supported Vulkan version
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        // 텍스처 압축, 64비트 부동 소수점 및 다중 뷰포트 렌더링(VR에 유용)과 같은 선택적 기능에 대한 지원 등 | Optional features like texture compression, 64 bit floats and multi viewport rendering (useful for VR)
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

        std::cout << "Suitable device found : \n";
        std::cout << '\t' << "deviceName: " << deviceProperties.deviceName << '\n';
        std::cout << '\t' << "deviceID: " << deviceProperties.deviceID << '\n';
        std::cout << '\t' << "apiVersion: " << deviceProperties.apiVersion << '\n';
        std::cout << '\t' << "deviceType: " << deviceProperties.deviceType << '\n'; // 내장형 GPU = 1 외장형 GPU = 2
        std::cout << '\t' << "driverVersion: " << deviceProperties.driverVersion << '\n';
        std::cout << '\t' << "maxImageDimension2D: " << deviceProperties.limits.maxImageDimension2D << '\n';
        std::cout << '\t' << "tessellationShader supported: " << deviceFeatures.tessellationShader << '\n';
        std::cout << '\t' << "geometryShader supported: " << deviceFeatures.geometryShader << '\n';
    }



    // 불칸에서 모든 명령은 큐에 넣어서 그래픽 카드에 보내야 합니다. 사실 큐 종류는 여러개이며 어떤 큐는 그래픽 명령을 처리하지 않고 컴퓨트 명령만 처리하기도 하고 메모리 전송만 하기도 합니다. 물론 다양한 종류의 명령을 동시에 처리하는 큐도 있습니다. 이러한 큐 종류를 큐 패밀리라고 부릅니다. 우리가 사용해야할 큐 패밀리를 그래픽카드가 모두 지원하는지 확인해야 합니다.
    HELPER_FUNCTION QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
    {
        // 해당 그래픽카드가 우리가 사용해야할 큐 패밀리를 그래픽카드가 모두 지원하는지 확인해야 합니다.
        QueueFamilyIndices indices;

        // 그래픽카드가 지원하는 모든 큐 패밀리 갯수와 리스트를 받아옵니다.
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
        // 이제 queueFamilies 에는 VkQueueFamilyProperties 정보들 묶음이 담겨있습니다.

        int i = 0;
        // queueFamily 에는 각각의 큐 페밀리에 대한 정보가 담겨있습니다. queueFamily 를 전부 탐색해서 우리가 원하는 큐 페밀리가 있는지 확인합니다.
        for (const auto& queueFamily : queueFamilies)
        {
            // 지금은 그래픽 명령을 지원하는 큐 페밀리 VK_QUEUE_GRAPHICS_BIT 하나만 필요하지만 나중에는 더 추가될 수 있습니다.
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = i; // 값을 썼으므로 indices.isComplete() 했을때 true 가 반환 될것입니다.
            }

            // @@ queueFamily 를 사용하지도 않는데 아래 코드는 꼭 반복문 안에 있어야 하는건가?? i 값 사용 때문에 그런 것 같기도..
            // 서피스에 이미지를 표시할 수 있는 큐 패밀리가 있는지 검사합니다. 보통은 그리기와 화면 표시가 동시에 지원되는 큐 페밀리가 있지만 (그런경우 i 값이 동일하게 저장될 것입니다), 각각 따로 존재할 수도 있습니다 (이런 경우 성능이 떨어집니다).
            VkBool32 presentSupport = false;
            // 특이하게 생성된 추상적 서피스 개체를 사용하고 프레젠테이션을 지원할 수 있는 그래픽스 큐를 검색 하려면 queueFamily.queueFlags 가 아닌vkGetPhysicalDeviceSurfaceSupportKHR 라고 따로 존재하는 함수를 사용해야 한다.
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport)
            {
                indices.presentFamily = i;
            }


            // 이미 원하는 모든 큐 패밀리를 찾았으므로 추가로 찾을 필요가 없습니다.
            if (indices.isComplete())
            {
                break;
            }

            // 큐 패밀리에는 다수의 큐가 있을 수 있다. 이때 큐 패밀리 내의 각각의 큐는 유일한 인덱스 값으로 구분된다.
            i++;
        }

        // 지원하는 큐 페밀리의 인덱스 번호들을 담고 있습니다.
        return indices;
    }



    // 2-4. 그래픽 카드와 통신하기 위한 인터페이스 생성
    inline void createLogicalDevice()
    {
        // graphicsFamily 에는 그래픽카드가 해당 큐 패밀리를 지원하는지 여부와 지원한다면 해당 큐 페밀리의 인덱스 번호를 담고 있습니다.
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        // 필요한 큐 페밀리가 많이질 것 같으므로 우아하게 각 큐 페밀리에 대한 설정값들을 set 을 만들어 관리하겠습니다.
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsFamily.value(), 
            indices.presentFamily.value(), 
        };

        // 각각의 큐 패밀리를 위해 만든 queueCreateInfo 정보들을 벡터 리스트로 저장합니다.
        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            // 2-4-1. 하나의 큐 패밀리 안에서 사용할 큐의 갯수를 설정합니다. 지금은 그래픽스 큐, 프레젠트 큐 하나씩을 달라고 요청하고 있습니다. 사실 큐는 그렇게 많이 필요하지 않으며, 일반적으로 1개면 충분합니다. 왜냐하면 커멘드 버퍼들을 멀티 스레드로 많이 만든 후에 큐로 제출할때는 메인 스레드에서 한번에 모아 하나의 큐로 묶어 제출하기 때문입니다. Vulkan에서 큐는 커맨드 버퍼를 그래픽카드에 공급하는 매체 역할을 합니다.
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;

            // 큐의 우선순위를 0.1 ~ 1 스케일로 할당할 수 있습니다. 하나의 큐만 사용하더라도 반드시 지정해야 합니다. 1.0f 는 우선순위가 가장 높음을 의미합니다.
            static float queuePriority = 1.0f;
            queueCreateInfo.pQueuePriorities = &queuePriority;

            queueCreateInfos.push_back(queueCreateInfo);
        }
        

        // 2-4-2. 우리가 사용할 장치의 기능들을 정의합니다. 장치에서 지원하는 모든 기능들은 vkGetPhysicalDeviceFeatures 로 확인 가능합니다. 사실상 지금은 아무런 기능들도 사용하지 않으므로 모든 설정값을 기본(VK_FALSE)으로 두겠습니다.
        VkPhysicalDeviceFeatures deviceFeatures{};


        // 2-4-3. 이제 논리 장치를 만듭니다. 논리 장치를 만들때 위에서 미리 만들어둔 VkDeviceQueueCreateInfo, VkPhysicalDeviceFeatures 두개의 설정값들을 사용합니다.
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()); // queueCreateInfo 의 갯수를 설정합니다.
        createInfo.pQueueCreateInfos = queueCreateInfos.data(); // 이렇게 쓰면 queueCreateInfos 를 C 스타일 배열처럼 만들어 포인터를 반환합니다.
        

        // 불칸 인스턴스 생성과 마찬가지로 확장과 검증 레이어를 명시해야 합니다. 예를들어 이미지를 윈도우에 렌더링하도록 도와주는 VK_KHR_swapchain 확장은 디스플레이 출력이 없는 서버용이나 비트코인 채굴용 그래픽카드에서는 제공하지 않고 단지 컴퓨트 연산만 제공할 수도 있습니다.
        createInfo.pEnabledFeatures = &deviceFeatures;

        // 우리가 사용할 추가 확장 기능 리스트도 불칸에게 전달합니다.
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // 초창기 불칸에서는 인스턴스와 디바이스 전용 검증 레이어를 분리해서 다뤘지만 현재는 인스턴스에만 적용하면 모든 검증을 다 할 수 있도록 바뀌었습니다. 때문에 최신버전의 Vulkan 에서 VkDeviceCreateInfo 의 enabledLayerCount 와 ppEnabledLayerNames 항목은 사실상 무의미해졌지만 구버전 호환성을 위해 불칸 인스턴스 생성때와 동일한 검증 레이어를 집어넣었습니다.
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }


        // 2-4-4. 이제 추상적 디바이스를 만들기 위한 설정들을 모두 마쳤으며, 추상적 디바이스를 생성할 수 있습니다. vkCreateDevice(사용할 그래픽카드, 논리 장치를 만들기 위한 설정값 포인터, 얼로케이션 콜백 포인터, 추상적 디바이스 핸들을 저장할 변수에 대한 포인터)
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
        {
            // 존재하지 않는 확장을 활성화하거나 지원되지 않는 기능을 사용하면 추상적 디바이스 생성에 실패하고 에러를 반환합니다!
            throw std::runtime_error("Failed to create logical device!");
        }


        // 2-4-5. 각각의 큐 페밀리에 해당하는 큐 핸들을 받아옵니다. 큐 패밀리가 동일한 경우 graphicsQueue 와 presentQueue 핸들은 동일한 값을 가질 가능성이 높습니다. 대기열 패밀리가 동일한게 확실한 경우 해당 인덱스를 한 번만 전달해도 됩니다. vkGetDeviceQueue(추상적 디바이스, 큐 페밀리 인덱스, 큐 인덱스, 큐 핸들을 저장할 변수에 대한 포인터)
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

        // 이제 추상적 디바이스와 큐 핸들을 사용하여 실제로 그래픽 카드에 명령을 때려넣어 작업을 시작할 수 있습니다.
    }



    // 2-5. 이미지 버퍼를 어떤 방식으로 동기화하면서 화면에 표시할지 스왑 체인 규약과 스왑 체인용 이미지 생성
    inline void createSwapChain()
    {
        // 2-5-1. 스왑 체인이 사용 가능한지 확인하는 것만으로는 충분하지 않습니다. 실제로 창 표면과 호환되지 않을 수 있기 때문입니다. 스왑 체인을 생성하려면 인스턴스 및 장치 생성보다 훨씬 더 많은 설정이 필요하므로 계속 진행하기 전에 더 디테일한 기능들이 사용 가능한지 확인해야 합니다.
        // 스왑 체인을 사용하려면 기본적으로 확인해야 하는 세 가지 유형의 속성이 있습니다.
        // 1) 기본 서피스 기능 (스왑 체인의 최소/최대 이미지 수, 이미지의 최소/최대 너비 및 높이)
        // 2) 서피스 형식 (픽셀 형식, 색 공간)
        // 3) 사용 가능한 프레젠테이션 모드
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);


        // 2-5-2. 그래픽카드가 스왑 체인을 사용할 수 있다는 것이 확인되면, 스왑 체인을 우리가 원하는 최상의 환경으로 설정해야 합니다.
        // 결정할 세 가지 유형의 속성이 있습니다.
        // 1) 서피스 형식 (색상 깊이)
        // 2) 프레젠테이션 모드 (이미지를 화면으로 "교체"하기 위한 조건)
        // 3) 스왑 범위 (스왑 체인의 이미지 해상도)
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);


        // 2-5-3. 스왑 체인에 포함할 이미지 수를 결정해야 합니다. 구현은 작동하는 데 필요한 최소 수를 지정합니다. 단순히 최소값을 사용하면 렌더링할 이미지를 얻기 위해 드라이버가 내부 작업을 완료할 때까지 기다려야 할 수도 있음을 의미합니다. 따라서 최소 이미지 갯수보다 하나 이상의 이미지 갯수를 사용하는 것이 좋습니다.
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        // 또한, 위에서 최소 이미지 갯수 + 1 을 했지만 지원하는 최대 이미지 갯수를 초과하지 않도록 해야 합니다. 여기서 0 은 최대값이 없음을 의미하는 특수 값입니다.
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        // 스왑 체인을 설정하기 위한 속성값들을 정의합니다.
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface; // 스왑 체인과 연결할 서피스를 설정합니다.

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        // 양안 3D 렌더링을 하지 않는 이상 이미지 레이어 수는 항상 1 입니다.
        createInfo.imageArrayLayers = 1; 
        // 스왑 체인에서 어떤 연산으로 이미지를 사용할 지 지정합니다. 우리는 이미지를 이용해서 바로 렌더링을 할 것이므로 COLOR_ATTACHMENT 로 사용합니다. 나중에 포스트-프로세싱 이펙트 같은 것들을 위해 이 값을 VK_IMAGE_USAGE_TRANSFER_DST_BIT 로 설정하여 메모리 연산을 이용해 최종 스왑 체인 이미지로 전송할 수도 있습니다.
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;


        // 2-5-4. 다음으로, 여러 큐 패밀리에서 사용될 스왑 체인 이미지를 처리하는 방법을 지정해야 합니다. 현재 애플리케이션의 경우 그래픽 큐 패밀리와 프레젠테이션 큐 페밀리가 다르게 사용됩니다. 따라서 그래픽 큐에서 스왑 체인의 이미지를 그린 다음 프레젠테이션 큐에 제출합니다.
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        if (indices.graphicsFamily != indices.presentFamily)
        {
            // 여러 큐에서 이미지를 엑세스하는 두 가지 방식이 있습니다.
            // 1) VK_SHARING_MODE_EXCLUSIVE: 이미지는 한 번에 하나의 큐 패밀리가 소유하며 소유권은 다른 큐 패밀리에서 사용하기 전에 명시적으로 설정되어야 합니다. 이 옵션은 최상의 성능을 제공합니다.
            // 2) VK_SHARING_MODE_CONCURRENT: 명시적 소유권 이전 없이 여러 대기열 패밀리에서 이미지를 사용할 수 있습니다. 이 옵션은 코딩하기 편합니다.
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            // 동시 모드를 사용하려면 queueFamilyIndexCount 및 pQueueFamilyIndices 매개변수를 사용하여 공유할 큐 패밀리 소유권을 미리 지정해야 합니다.
            // 그래픽 큐 페밀리와 프레젠테이션 큐 페밀리가 동일한 경우(대부분의 하드웨어에서 마찬가지입니다.) 동시 모드에서는 최소 두 개의 고유한 대기열 제품군을 지정해야 하므로 VK_SHARING_MODE_EXCLUSIVE (배타적 모드)를 사용해야 합니다.
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        // 그래픽 카드에서 지원되는 경우 90도 시계 방향 회전 또는 수평 뒤집기와 같이 스왑 체인의 이미지에 특정 변환을 적용하도록 지정할 수 있습니다. 변환을 원하지 않는 경우 그냥 현재 변환을 지정하기만 하면 됩니다.
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        // 윈도우 시스템에서 다른 윈도우들과 이미지를 혼합하는 데 알파 채널을 사용할지 여부를 지정합니다. 거의 대부분은 알파 채널을 무시하고 싶을 것이므로 VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR 을 사용합니다.
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        // 다른 윈도우 등에 의해 가려진 영역(화면에 보이지 않는 부분)에 있는 렌더링 작업을 생략하고 버릴 수 있는지 여부를 설정합니다.
        createInfo.clipped = VK_TRUE;

        // Vulkan을 사용하면 애플리케이션이 실행되는 동안 스왑 체인이 유효하지 않거나 최적화되지 않을 수 있습니다. 예를 들어 창 크기가 조정된 경우 스왑 체인은 실제로 처음부터 다시 만들어야 하며 이전 체인에 대한 참조를 이 필드에 지정해야 합니다.
        createInfo.oldSwapchain = VK_NULL_HANDLE;


        // 2-5-5. 이제 스왑 체인을 만들고 핸들을 얻습니다!
        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create swap chain!");
        }


        // 2-5-6. 스왑 체인이 만들어졌으면 불칸에서 우리가 설정해둔 만큼 스왑 체인용 이미지도 같이 만들어줄 것입니다. 위에서 그래픽카드가 지원하는 최소 스왑체인 이미지 수 + 1 를 지정했기 때문에 그 수를 런타임에 알 수 있기 때문에 갯수를 먼저 받고 그 다음 vkGetSwapchainImagesKHR를 다시 호출하여 swapChainImages 핸들을 얻습니다.
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        // 마지막으로 스왑 체인 이미지에 대해 선택한 형식과 범위를 멤버 변수에 저장합니다. 나중에 필요할 것입니다.
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }



    // 스왑 체인의 디테일한 기능들을 모두 지원하는지 자세히 확인합니다.
    HELPER_FUNCTION SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
    {
        SwapChainSupportDetails details;

        // 1) 기본 서피스 기능(스왑 체인의 최소/최대 이미지 수, 이미지의 최소/최대 너비 및 높이 등 화면 사용 용도)을 제공하는지 확인합니다. VkPhysicalDevice 와 VkSurfaceKHR 는 스왑 체인을 사용하기 위한 핵심 요소들이기 때문에 불칸이 이 요소들을 가지고 기능 지원을 확인합니다. 쿼리의 결과는 VkSurfaceCapabilitiesKHR 구조체 즉, &details.capabilities 로 반환됩니다
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        // 2) 서피스 형식(픽셀 형식, 색 공간)을 제공하는지 확인합니다.
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0)
        {
            details.formats.resize(formatCount); // 모든 지원 형식을 전부 저장할 수 있도록 벡터를 리사이징 합니다
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        // 3) 필요한 프레젠테이션 모드를 제공하는지 확인합니다.
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0)
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    // 스왑 체인에서 사용할 서피스 형식(색상 깊이)을 설정합니다.
    HELPER_FUNCTION VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        // VkSurfaceFormatKHR 에는 format 과 colorSpace 라는 멤버변수가 있습니다. format 은 VK_FORMAT_B8G8R8A8_SRGB (픽셀당 32비트 RGBA) 와 같은 컬러 체널을, colorSpace 는 SRGB, CMYK, HSV 와 같은 색공간을 정의합니다.
        for (const auto& availableFormat : availableFormats)
        {
            // 가장 흔하게 사용하는 32비트 RGBA 픽셀 포멧과 SRGB 색공간을 지원하는지 확인합니다.
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    // 스왑 체인에서 사용할 프레젠테이션 모드(이미지를 화면으로 "교체"하기 위한 조건)를 설정합니다.
    HELPER_FUNCTION VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        // 프레젠테이션 모드는 화면에 이미지를 표시하기 위한 실제 조건을 나타내기 때문에 스왑 체인에서 가장 중요한 설정입니다. Vulkan에서는 4가지 가능한 모드를 사용할 수 있습니다. MAILBOX 모드가 가장 좋고 이 모드를 사용 못하면 IMMEDIATE 을 사용하는 것을 추천합니다.
        // VK_PRESENT_MODE_IMMEDIATE_KHR: 수직 동기화 없이 바로바로 화면에 이미지를 표시합니다. (티어링 발생 함, 최대한 지연 없이 최신 화면을 보여주고 싶을 때 사용하면 좋습니다.)
        // VK_PRESENT_MODE_FIFO_KHR : 일반적으로 알려진 수직 동기화 모드입니다. GPU가 다음 프레임을 위한 이미지를 한 번만 연산하여 내부 큐에 저장된 상태로 기다리다가 다음 화면 주사율에서 순차적으로 보여줍니다. (티어링 발생 안함, GPU를 여유롭게 굴리고 싶을 때 사용하면 좋습니다.)
        // VK_PRESENT_MODE_FIFO_RELAXED_KHR : 위와 비슷하지만, GPU가 다음 프레임 이미지를 그리는 도중에 화면 주사율에 도달하면 그냥 현재까지 그린 이미지를 바로 보여줍니다. (티어링 발생 함, 어플리케이션이 화면 주사율보다 빠를때 좋음)
        // VK_PRESENT_MODE_MAILBOX_KHR : 일반적으로 알려진 트리플 버퍼링 수직 동기화 모드입니다. GPU가 다음 프레임을 위한 이미지를 다 연산했는데다음 화면 주사율까지 여유가 있으면 계속해서 프레임을 업데이트 하다가 화면 주사율에 도달했을때 가장 최근에 완성한 이미지를 보여줍니다. (티어링이 발생하지 않는 모드중 가장 지연이 적으나 GPU 가 쉴 시간을 주지 않으므로 전력 사용량이 많아 모바일에서는 적절하지 않음)
        for (const auto& availablePresentMode : availablePresentModes)
        {
            // 트리플 버퍼링 수직 동기화 모드가 가능하면 활성화
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ VK_PRESENT_MODE_FIFO_KHR 로 동작하도록 주석처리함
                //return availablePresentMode;
            }
        }

        // 아니면 일반 수직 동기화 모드 활성화 (일반 수직 동기화 모드는 불칸에서 스왑 체이닝이 가능한 경우 반드시 지원하도록 보장합니다.)
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    // 스왑 체인에서 사용할 스왑 범위(스왑 체인의 이미지 해상도)를 설정합니다.
    HELPER_FUNCTION VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
    {
        // VkSurfaceCapabilitiesKHR 구조체에는 설정 가능한 화면 해상도 범위가 들어있습니다. 
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            // capabilities.currentExtent 멤버에는 현재 윈도우 화면의 크기(해상도)가 들어있으며 일반적으로는 이 값을 사용합니다.
            return capabilities.currentExtent;
        }
        else // 하지만 일부 윈도우 관리자에서는 특이하게 currenExtent 값을 uint32_t 의 최대값으로 설정하여 표시하는 경우가 있습니다. 그럴 경우 minImageExtent 와 maxImageExtent 범위 내에서 창과 가장 일치하는 해상도를 계산해 넣어야 합니다. 우리는 GLFW 를 사용했으므로 이 값들을 GLFW 로 부터 직접 받아옵니다. 혹시 윈도우에 High DPI 설정 (UI스케일) 이 되어있다면 불행하게도 실제 윈도우의 해상도는 스크린 좌표계 해상도보다 클 수 있습니다. 때문에 최소 및 최대 이미지 범위와 일치시키기 전에 실제 픽셀 단위로 창의 해상도를 요청하기 위해 glfwGetFramebufferSize 를 반드시 사용해야 합니다.
        {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            // std::clamp 는 actualExtent를 그래픽카드에서 지원 가능한 최소 및 최대 해상도 범위로 제한하기 위해서 사용합니다.
            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }



    // 2-6. 스왑 체인용 이미지 사용 방식을 정의하기 위한 이미지 뷰 생성
    void createImageViews()
    {
        // 스왑 체인을 포함하여 모든 VkImage 이미지 개체는 렌더 파이프라인에서 이미지 사용 방식인 VkImageView 개체를 만들어야 이미지를 사용할 수 있습니다. 이미지 사용 방식의 예는 2D 텍스쳐를 밉맵핑 없이 단순 뎁스 맵으로 인식하게 하는 것 등이 있습니다. 지금으로썬 createImageViews() 는 스왑 체인의 모든 이미지들에 대해 칼라 타겟으로 사용하는 가장 기본적인 이미지 뷰를 제공합니다.

        // 스왑 체인용 이미지 뷰(사용 방식)을 정의하기 위해 모든 스왑 체인 이미지를 순회합니다.
        swapChainImageViews.resize(swapChainImages.size());

        for (size_t i = 0; i < swapChainImages.size(); i++)
        {
            // 이미지 뷰 생성을 위한 속성값들은 VkImageViewCreateInfo 구조체로 설정합니다.
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];
            // viewType 과 format 은 어떻게 이미지 데이터가 해석될지 지정합니다.
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // 1D, 2D, 3D 텍스쳐나 큐브 맵도 될 수 있습니다.
            createInfo.format = swapChainImageFormat;
            // components 설정값을 사용하면 RGBA 색상 채널을 GGBA 처럼 막 섞을 수 있습니다. 예를 들어 모노크롬 텍스처의 경우 모든 채널을 빨간색 채널에 매핑할 수 있습니다. - https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkComponentSwizzle.html
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            // subresourceRange는 이미지의 목적이 무엇이며 액세스해야 하는 이미지 부분을 설명합니다. 우리의 이미지는 밉매핑 레벨이나 다중 레이어 없이 색상 대상으로 사용됩니다.
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // 칼라 타겟으로 사용
            createInfo.subresourceRange.baseMipLevel = 0; // 밉맵 없음
            createInfo.subresourceRange.levelCount = 1; // 밉맵 계층 갯수
            // 스테레오그래픽 3D 응용 프로그램에서는 여러 레이어가 있는 스왑 체인을 만들 것입니다. 그런 다음 다른 레이어에 액세스하여 왼쪽 및 오른쪽 눈의 보기를 나타내는 각 이미지에 대해 여러 이미지 뷰를 만들 수 있습니다.
            createInfo.subresourceRange.baseArrayLayer = 0; // 뷰에 보여질 첫번째 배열 레이어
            createInfo.subresourceRange.layerCount = 1; // 배열 레이어 수

            // 이미지 뷰 개체를 만들고 핸들을 받습니다.
            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create image views!");
            }
        }
    }



    // 2-7. 렌더링에 사용할 프레임 버퍼 첨부들과 서브패스에 대한 정보를 명시합니다.
    // 어테치먼트(첨부)는 이미지 뷰의 일종이지만 추가적으로 어떻게 읽고, 쓰고, 사용하는지에 대한 내용이 포함되어 있습니다 -  https://www.reddit.com/r/vulkan/comments/a27cid/what_is_an_attachment_in_the_render_passes/ , 처음 만나는 불칸 232페이지 참고
    void createRenderPass()
    {
        // 파이프라인 생성을 완료하기 전에 렌더링하는 동안 사용할 프레임 버퍼 첨부 요소들에 대해 Vulkan에 알려야 합니다. 색상 및 깊이 버퍼의 수, 각각에 사용할 샘플의 수, 렌더링 작업 전반에 걸쳐 해당 내용을 처리하는 방법을 지정해야 합니다. 이 모든 정보는 새로운 createRenderPass 함수를 생성할 렌더 패스 객체에 래핑됩니다.

        // 우리의 경우 스왑 체인의 이미지 중 하나로 표시되는 단일 색상 버퍼 첨부만 있을 것입니다. VkAttachmentDescription 디스크립터는 첨부의 다양한 속성을 지정하는데 사용합니다.
        VkAttachmentDescription colorAttachment{};
        // 색상 첨부 형식은 스왑 체인 이미지 형식과 동일해야 하며 아직 멀티샘플링을 사용하지 않으므로 1개의 샘플을 사용합니다.
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        // loadOp 및 storeOp는 렌더링 전과 렌더링 후에 첨부 데이터로 수행할 작업을 결정합니다.
        // loadOp에 대해 다음과 같은 선택 사항이 있습니다.
        // VK_ATTACHMENT_LOAD_OP_LOAD: 기존 첨부 내용을 유지합니다.
        // VK_ATTACHMENT_LOAD_OP_CLEAR: 시작 시 값을 상수로 지웁니다.
        // VK_ATTACHMENT_LOAD_OP_DONT_CARE : 시작 값은 신경 안씁니다.
        // // 우리의 경우 새 프레임을 그리기 전에 clear 작업을 사용하여 프레임 버퍼를 검은색으로 지웁니다.
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // storeOp에는 두 가지 가능성만 있습니다.
        // VK_ATTACHMENT_STORE_OP_STORE: 렌더링된 콘텐츠는 메모리에 저장되며 나중에 읽을 수 있습니다.
        // VK_ATTACHMENT_STORE_OP_DONT_CARE: 프레임 버퍼의 내용은 렌더링 작업 후에 어떻게 되던 신경 안씁니다.
        // 렌더링된 삼각형을 화면에 표시하는 데 관심이 있으므로 여기에서는 저장 작업을 수행하겠습니다.
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // loadOp & storeOp는 색상 및 깊이 데이터에 적용되고 stencilLoadOp & stencilStoreOp 는 스텐실 데이터에 적용됩니다. 우리 응용 프로그램은 스텐실 버퍼로 아무 것도 하지 않으므로 로드 및 저장 결과는 관련이 없습니다.
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // Vulkan의 텍스처와 프레임 버퍼는 특정 픽셀 형식의 VkImage 개체로 표시되지만 메모리에 픽셀을 정렬하는 방식인 픽셀 레이아웃은 이미지로 수행하려는 작업에 따라 변경될 수 있습니다.
        // 가장 일반적인 레이아웃은 다음과 같습니다.
        // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: 색상 첨부로 사용되는 이미지
        // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : 스왑 체인에 표시할 이미지
        // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : 메모리 복사 작업의 대상으로 사용할 이미지
        // 텍스처링 장에서 이 주제에 대해 더 깊이 논의할 것이지만 지금 알아야 할 중요한 것은 이미지가 다음에 포함될 작업에 적합한 특정 레이아웃으로 전환되어야 한다는 것입니다.
        // initialLayout은 렌더 패스가 시작되기 전에 이미지가 가질 레이아웃을 지정합니다. finalLayout은 렌더 패스가 완료될 때 자동으로 전환할 레이아웃을 지정합니다. initialLayout에 VK_IMAGE_LAYOUT_UNDEFINED 를 사용한다는 것은 이미지가 어떤 이전 레이아웃에 있던지 신경 쓰지 않는다는 것을 의미합니다. 이 값의 주의 사항으로 이미지의 내용이 보존된다는 보장이 없지만 우리가 계속해서 클리어 할 것이기 때문에 중요하지 않다는 것입니다. 렌더링 후 스왑 체인을 사용하여 이미지를 표시할 준비가 되기를 원합니다. 이것이 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR을 finalLayout으로 사용하는 이유입니다.
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


        // 서브패스 및 첨부 파일 속성 설정
        // 하나의 렌더 패스는 여러 서브(하위)패스로 구성될 수 있습니다. 서브패스는 이전 패스의 프레임 버퍼 내용에 따라 달라지는 후속 렌더링 작업입니다(예: 차례로 적용되는 후처리 효과 시퀀스). 이러한 렌더링 작업을 하나의 렌더 패스로 그룹화하면 Vulkan은 작업을 재정렬하고 더 나은 성능을 위해 메모리 대역폭을 절약할 수 있습니다. 그러나 첫 번째 삼각형의 경우 단일 서브패스를 사용합니다.
        // 모든 서브패스는 하나 이상의 첨부를 참조합니다. 이러한 참조는 다음과 같은 VkAttachmentReference 구조체로 표현합니다.
        // Attachment 파라미터는 어떤 첨부를 참조할지 디스크립터 배열의 인덱스를 지정합니다. 우리의 배열은 단일 VkAttachmentDescription으로 구성되어 있으므로 인덱스는 0입니다. Vulkan은 서브패스가 시작될 때 자동으로 첨부 파일을 이 레이아웃으로 전환합니다. 첨부 파일을 사용하여 색상 버퍼 기능을 수행할 예정이며 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 레이아웃은 이름에서 알 수 있듯이 최고의 성능을 제공합니다.
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // 서브패스는 VkSubpassDescription 구조를 사용하여 설명됩니다.
        VkSubpassDescription subpass{};
        // Vulkan은 향후 컴퓨팅 서브패스도 지원할 수 있으므로 이것이 그래픽 서브패스임을 명시해야 합니다. 다음으로 색상 첨부 파일에 대한 참조를 지정합니다.
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        // 이 배열의 첨부 인덱스는 layout(location = 0) out vec4 outColor 지시문을 사용하여 프래그먼트 셰이더에서 직접 참조됩니다!
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        // 다음과 같은 다른 유형의 첨부도 추가로 서브패스에서 참조할 수 있습니다.
        // pInputAttachments : 셰이더에서 읽어온 첨부
        // pResolveAttachments : 멀티샘플링 색상 첨부 파일에 사용되는 첨부
        // pDepthStencilAttachment : 깊이 및 스텐실 데이터에 대한 첨부
        // pPreserveAttachments : 이 서브패스에서 사용하지 않지만 데이터를 보존해야 하는 첨부


        // 그런 다음 VkRenderPassCreateInfo 구조를 첨부 및 서브패스의 배열로 채워서 렌더 패스 개체를 생성할 수 있습니다. VkAttachmentReference 개체는 이 배열의 인덱스를 사용하여 첨부를 참조합니다.
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;


        // 이제 렌더 패스 객체를 생성합니다!
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create render pass!");
        }
    }



    // 2-8. 셰이더 로드 및 그래픽스 파이프라인 생성
    inline void createGraphicsPipeline()
    {
        // ------------- 이 아래로는 프로그래밍 가능한 셰이더 스테이지에 대한 설정입니다. -------------
        
        // 2-8-1. 바이너리 파일을 읽어서 바이트 배열로 반환합니다.
        auto vertShaderCode = readFile("Shaders/hello_triangle_shader.vert.spv");
        auto fragShaderCode = readFile("Shaders/hello_triangle_shader.frag.spv");

        // 2-8-2. 바이트 배열로 저장된 버퍼를 받아서 셰이더 모듈 (VkShaderModule) 를 만듭니다. 셰이더 모듈은 단순히 셰이더 바이트코드의 얇은 래퍼입니다.
        // GPU에서 실행하기 위해 SPIR-V 바이트코드를 기계어 코드로 컴파일하고 링킹하는 작업은 그래픽 파이프라인이 생성될 때까진 발생하지 않습니다. 즉, 파이프라인 생성이 완료되는 즉시 셰이더 모듈을 파괴해도 상관없습니다. 따라서 클래스 멤버 대신 createGraphicsPipeline 함수 내 로컬 변수로 만들었습니다.
        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        // 2-8-3. 셰이더 스테이지를 설정합니다. 셰이더를 사용하기 위해서는 특정한 파이프라인 스테이지에 배치하여야 합니다.
        // 버텍스 셰이더 스테이지를 정의합니다.
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        // 이 셰이더를 어떤 파이프라인 스테이지에서 사용할지 enum 값으로 알려줍니다.
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        // 해당하는 셰이더 모듈의 핸들을 할당합니다.
        vertShaderStageInfo.module = vertShaderModule;
        // 진입점 (엔트리포인트 함수) 를 설정합니다. 여러 프레그먼트 셰이더들을 단일 셰이더 모듈로 결합하고 서로 다른 진입점을 사용하여 동작을 구분할 수도 있습니다.
        vertShaderStageInfo.pName = "main";
        // vertShaderStageInfo.pSpecializationInfo 는 사용되지 않았지만 중요합니다. 셰이더에 쓰일 상수 (constants) 에 대한 값을 지정할 수 있습니다. 셰이더를 런타임에 교환하는 것보다 상수값을 이용해 코드의 흐름을 변경시키는 것이 효과적일 때가 많습니다. 컴파일러는 이러한 상수값에 따라 if 문을 제거하는 것과 같은 최적화를 수행할 수 있기 때문입니다. 이와 같은 상수가 없으면 구조체 초기화가 자동으로 수행하는 nullptr로 멤버를 설정할 수 있습니다.
        vertShaderStageInfo.pSpecializationInfo = nullptr;

        // 프레그먼트 셰이더도 마찬가지로 스테이지를 정의합니다.
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";
        fragShaderStageInfo.pSpecializationInfo = nullptr;

        // 스테이지 설정값들을 묶어서 한번에 전달해야 합니다. (@@ 나중에 사용)
        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };


        // ------------- 이 아래로는 프로그래밍 불가능한 스테이지에 대한 설정입니다. -------------
        
        // 이전 세대 그래픽스 API 에서는 그래픽스 파이프라인의 대부분의 단계에 대해 기본 설정을 제공하지만, 불칸은 모든 것들을 직접 다 설정해야 합니다.

        // 2-8-4. 버텍스 셰이더에 어떤 형식으로 버텍스 데이터를 집어넣을지 설정합니다. pVertexBindingDescriptions 및 pVertexAttributeDescriptions 멤버는 정점 데이터를 로드하기 위해 앞서 언급한 세부 정보를 설명하는 구조체 배열을 가리킵니다.
        // 버텍스 데이터 형식은 크게 두가지 방법으로 표현됩니다.
        // Bindings: 데이터 사이의 간격 및 데이터가 버텍스당인지 또는 인스턴스당인지 여부( 인스턴싱 참조 )
        // Attribute descriptions : 정점 셰이더에 전달된 속성의 유형, 속성을 로드할 바인딩 및 오프셋
        // @@ 일단 현재는 버텍스 데이터를 셰이더에 하드 코딩 했으므로 읽어들일 정점 정보가 없습니다. 나중에 버텍스 버퍼를 받을때 사용하겠습니다.
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional


        // 2-8-5. 버텍스 데이터들을 가지고 어떤 방식으로 지오메트리를 그릴지, 그리고 다른 프리미티브로 그리기 여부를 설정합니다. 일반적으로 버텍스은 인덱스에 따라 버텍스 버퍼에서 순차적으로 로드되지만 element 버퍼 를 사용하면 인덱스를 지정하여 사용할 수 있습니다. 이를 통해 버텍스 재사용과 같은 최적화를 수행할 수 있습니다. primitiveRestartEnable 멤버를 VK_TRUE로 설정하면 0xFFFF 또는 0xFFFFFFFF 같은 특수 인덱스를 사용하여 _STRIP 토폴로지 모드에서 선과 삼각형을 분할할 수 있습니다.
        // 지오메트리를 그리는 방식 (토폴로지) 은 다음과 같은 종류가 있습니다.
        // VK_PRIMITIVE_TOPOLOGY_POINT_LIST :       각각의 점으로 그리기
        // VK_PRIMITIVE_TOPOLOGY_LINE_LIST :        재사용하지 않고 모든 2개의 점을 이어서 라인으로 그리기
        // VK_PRIMITIVE_TOPOLOGY_LINE_STRIP :       모든 라인의 끝 정점은 다음 라인의 시작 정점으로 사용합니다.
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST :    재사용하지 않고 3개의 정점마다 삼각형 그리기
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP :   모든 삼각형의 두 번째와 세 번째 정점은 다음 삼각형의 처음 두 정점으로 사용합니다.
        // @@ 일단 현재는 듀토리얼을 위해 정점으로 삼각형 (TRIANGLE_LIST) 을 그립니다.
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;


        // 2-8-6. 뷰포트 영역을 설정합니다.
        // 뷰포트는 기본적으로 출력이 렌더링될 프레임 버퍼의 영역을 설명합니다. 이것은 거의 항상 (0, 0) ~ (너비, 높이) 입니다.
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        // 스왑 체인 이미지들이 나중에 프레임 버퍼로 사용될 것입니다.
        viewport.width = (float)swapChainExtent.width;
        viewport.height = (float)swapChainExtent.height;
        // 프레임 버퍼에서 사용할 깊이 값 범위를 설정합니다. 반드시 0 ~ 1 범위 내에서만 설정해야 합니다.
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;


        // 2-8-7. 시저 영역을 설정합니다.
        // 뷰포트는 이미지에서 프레임 버퍼로의 변환을 정의하는 반면 시저 직사각형은 픽셀이 실제로 저장될 영역을 정의합니다. 시저 직사각형 외부의 모든 픽셀은 래스터라이저에 의해 연산되지 않고 무시됩니다. 변환이 아니라 필터처럼 작동하므로 성능 향상을 볼 수 있습니다. 차이점은 이 이미지를 참고해주세요 - https://vulkan-tutorial.com/images/viewports_scissors.png
        // @@ 일단 지금은 화면 전부를 보여주기 위해 프레임 버퍼 크기만큼 덮는 시저로 설정합니다.
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = swapChainExtent;


        // 2-8-8. 뷰포트와 시저 영역 설정들을 결합합니다.
        // 뷰포트와 시저 영역 설정은 VkPipelineViewportStateCreateInfo 구조체를 사용하여 뷰포트 스테이트로 결합되어야 합니다. 일부 그래픽 카드에서는 여러 뷰포트와 가위형 직사각형을 사용할 수 있으므로 해당 멤버는 해당 그래픽 카드의 배열을 참조합니다. 여러 개를 사용하려면 GPU 기능을 활성화해야 합니다 (추상적 장치 생성 참조).
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;


        // 2-8-9. 레스터라이저를 설정합니다.
        // 래스터라이저는 버텍스 셰이더에서 만들어진 지오메트리를 가져와 프레그먼트 셰이더를 가지고 색칠된 조각으로 바꿉니다. 또한 depth testing, face culling, scissor test 를 수행하며 전체 다각형 또는 가장자리(와이어프레임 렌더링)만 그리도록 구성할 수도 있습니다. 이 모든 것은 VkPipelineRasterizationStateCreateInfo 로 설정합니다.
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        // depthClampEnable이 VK_TRUE로 설정되면 뷰 프러스텀 근거리 평면보다 안쪽 또는 원거리 평면 너머에 있는 픽셀들의 z 값을 뷰 프러스텀 범위 내로 클램핑 합니다. 이것은 그림자 맵과 같은 특수한 경우에 유용합니다. 이를 사용하려면 GPU 기능을 활성화해야 합니다.
        rasterizer.depthClampEnable = VK_FALSE;
        // rasterizerDiscardEnable이 VK_TRUE로 설정되면 지오메트리는 래스터라이저 단계를 거치지 않습니다. 이것은 기본적으로 프레임 버퍼에 대한 모든 출력을 비활성화합니다.
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        // 폴리곤 모드는 지오메트리에 대해 프래그먼트가 생성되는 방법을 결정합니다. 다음 모드를 사용할 수 있습니다.
        // VK_POLYGON_MODE_FILL: 다각형을 색으로 채우기
        // VK_POLYGON_MODE_LINE : 와이어프레임 모드
        // VK_POLYGON_MODE_POINT : 점들만 표히사는 모드
        // 채우기 이외의 모드를 사용하려면 GPU 기능을 활성화해야 합니다.
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        // 레스터화에 사용할 선분의 굵기를 설정합니다. 지원되는 최대 라인 너비는 그래픽 카드에 따라 다르며 1.0f 보다 두꺼운 라인은 wideLines GPU 기능을 활성화해야 합니다.
        rasterizer.lineWidth = 1.0f;
        // 표면 컬링 유형을 결정합니다. 컬링을 비활성화하거나, 앞면을 컬링하거나, 뒷면을 컬링하거나, 둘 모두를 컬링할 수 있습니다. frontFace 변수는 정면으로 간주되는 면의 꼭짓점 순서를 지정하며 시계 방향 (왼손 중심) 또는 시계 반대 방향 (오른손 중심) 일 수 있습니다.
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        // 바이어스 값을 사용하여 (상수를 더해) 깊이 값을 계산합니다. 이것은 때때로 그림자 매핑에 사용되지만 우리는 사용하지 않을 것입니다. depthBiasEnable을 VK_FALSE로 설정하기만 하면 됩니다.
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional


        // 2-8-10. 멀티샘플링을 설정합니다.
        // VkPipelineMultisampleStateCreateInfo 구조체는 안티앨리어싱을 수행하는 방법 중 하나인 멀티샘플링을 구성합니다. 동일한 픽셀에서 여러 다각형의 프레그먼트 셰이더 결과를 결합하여 작동합니다. 이것은 주로 가장 눈에 띄는 앨리어싱 아티팩트가 발생하는 가장자리 계단 현상을 해결합니다. 하나의 폴리곤만 픽셀에 매핑되는 경우 프래그먼트 셰이더를 여러 번 실행할 필요가 없기 때문에 단순히 더 높은 해상도로 렌더링한 다음 축소하는 것보다 훨씬 저렴합니다. 활성화하려면 GPU 기능을 활성화해야 합니다. 지금은 비활성화 상태로 둡니다.
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional


        // 2-8-11. 깊이 및 스텐실 테스팅을 설정합니다.
        // 깊이 및/또는 스텐실 버퍼를 사용하는 경우 VkPipelineDepthStencilStateCreateInfo를 사용하여 깊이 및 스텐실 테스트도 구성해야 합니다. 지금 당장은 없으므로 그러한 구조체에 대한 포인터 대신 nullptr을 전달할 수 있습니다. 깊이 버퍼링 장에서 다시 다루겠습니다.
        // @@ ---아직은 코드없음---


        // 2-8-12. 컬러 블랜딩을 설정합니다.
        // 프레그먼트 셰이더가 색상을 반환한 후에는 이미 프레임 버퍼에 있는 색상과 결합해야 합니다. 이 변환을 색상 혼합이라고 하며 두 가지 방법으로 수행할 수 있습니다.
        // 1) 이전 값과 새 값을 혼합하여 최종 색상 생성
        // 2) 비트 연산을 사용하여 이전 값과 새 값 결합 (logicOpEnable을 VK_TRUE로 설정해야 합니다.)
        // 색상 혼합을 설정하는 두 가지 유형의 구조체가 있습니다. VkPipelineColorBlendAttachmentState 는 프레임버퍼당 설정이고 두 번째 VkPipelineColorBlendStateCreateInfo 는 전역적인 색상 혼합 설정이 들어있습니다. 우리의 경우에는 하나의 프레임 버퍼만 있습니다.
        // 색상 혼합을 사용하는 가장 일반적인 방법은 불투명도에 따라 새 색상을 이전 색상과 혼합하려는 알파 블랜딩을 구현하는 것입니다. 컬러 블랜딩에 아래 설정들이 어떻게 활용되는지는 다음 사이트의 의사 코드를 참고해 주세요. - https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions 
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        // 색상 블랜딩을 하지 않고 최신 색상만 표시합니다.
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

        // 두 번째 구조는 모든 프레임 버퍼에 대한 구조 배열을 참조하며 앞서 언급한 계산에서 혼합 계수로 사용할 수 있는 혼합 상수를 설정할 수 있습니다.
        // 두 번째 혼합 방법(비트 조합)을 사용하려면 logicOpEnable을 VK_TRUE로 설정해야 합니다. 그런 다음 비트 연산을 logicOp 필드에 지정할 수 있습니다. 첫 번째 혼합 방법을 위해 blendEnable을 VK_FALSE로 설정한 것처럼 이렇게 하면 연결된 모든 프레임 버퍼에 대해 자동으로 비활성화됩니다. colorWriteMask는 또한 이 모드에서 실제로 영향을 받을 프레임 버퍼의 채널을 결정하는 데 사용됩니다. 여기에서 했던 것처럼 두 모드를 모두 비활성화할 수도 있습니다. 이 경우 프레그먼트 색상이 블랜딩 되지 않은 상태로 프레임 버퍼에 바로 기록됩니다.
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional


        // 2-8-13. 동적 스테이트를 설정합니다.
        // 위에서 우리가 만들었던 설정값들은 사실 파이프라인을 완전히 새로 만들지 않고도 변경할 수 있습니다. 뷰포트의 크기, 선 너비 및 블렌드 상수가 그 예입니다. 그렇게 하려면 다음과 같이 VkPipelineDynamicStateCreateInfo 구조를 채워야 합니다. 이렇게 하면 이러한 값의 구성이 무시되고 드로잉 시(런타임) 에 데이터를 지정해야 합니다. 이에 대해서는 다음 장에서 다시 다루겠습니다. 이 구조체는 나중에 동적 상태가 없는 경우 nullptr로 대체될 수 있습니다.
        //std::vector<VkDynamicState> dynamicStates = {
        //    VK_DYNAMIC_STATE_VIEWPORT,
        //    VK_DYNAMIC_STATE_LINE_WIDTH
        //};
        //VkPipelineDynamicStateCreateInfo dynamicState{};
        //dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        //dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        //dynamicState.pDynamicStates = dynamicStates.data();


        // 2-8-14. 파이프라인 레이아웃을 설정합니다.
        // 셰이더에서 uniform 값을 사용할 수 있습니다. 이는 동적 상태 변수와 유사한 전역 변수로, 드로잉 시 변경할 수 있어 셰이더를 다시 생성하지 않고도 셰이더의 동작을 변경할 수 있습니다. 변환 행렬을 정점 셰이더에 전달하거나 프래그먼트 셰이더에서 텍스처 샘플러를 만드는 데 일반적으로 사용됩니다. 이러한 uniform 값은 VkPipelineLayout 객체를 생성하여 파이프라인 생성 중에 지정해야 합니다.다음 장까지 사용하지 않겠지만 여전히 빈 파이프라인 레이아웃을 만들어야 합니다. 이 구조는 또한 푸시 상수를 지정하는데, 이는 동적 값을 셰이더에 전달하는 또 다른 방법이며, 이는 향후 장에서 다룰 것입니다.
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // Optional
        pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional


        // 2-8-15. 파이프라인 레이아웃을 만듭니다.
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create pipeline layout!");
        }


        // 2-8-16. 그래픽스 파이프라인을 만듭니다.
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        
        // 다 쓴 셰이더 모듈은 소멸시킵니다.
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }



    // 바이너리 파일을 읽어서 바이트 배열로 반환합니다.
    // @@ 나중에 C 스타일로 바꿔서 성능 향상 가능할 듯..
    HELPER_FUNCTION static std::vector<char> readFile(const std::string& filename)
    {
        // 지정된 파일에서 모든 바이트를 읽고 바이트 배열 (std::vector) 로 저장하여 반환합니다.
        // 두 개의 플래그 설정이 필요합니다.
        // ate: 파일의 끝에서부터 읽기 시작 (끝에서부터 읽는 이유는 파일의 사이즈를 바로 알 수 있기 때문입니다.)
        // binary: 파일을 바이너리 형식으로 읽기 (텍스트 변형 방지, 이진 코드이기 때문에 %%EOF 로 끝나지 않음)
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        // 파일 열기를 시도합니다.
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file!");
        }

        // ate 속성으로 파일을 열었기 때문에 파일의 끝 위치가 즉 파일 사이즈가 됩니다.
        size_t fileSize = (size_t)file.tellg();
        // 파일 사이즈만큼 버퍼를 준비합니다.
        std::vector<char> buffer(fileSize);

        // 커서가 파일의 맨 끝에 있으므로 맨 앞으로 가져와서 읽기 시작해야 합니다.
        file.seekg(0);
        // 파일을 전부 읽어서 버퍼에 몽땅 담습니다.
        file.read(buffer.data(), fileSize);

        // 파일을 다 읽었으면 닫습니다.
        file.close();

        // 바이트 배열로 저장된 버퍼를 반환합니다.
        return buffer;
    }

    // 바이트 배열로 저장된 버퍼를 받아서 셰이더 모듈 (VkShaderModule) 를 만듭니다. 셰이더 모듈은 단순히 셰이더 바이트코드의 얇은 래퍼입니다.
    HELPER_FUNCTION VkShaderModule createShaderModule(const std::vector<char>& code)
    {
        // 셰이더 모듈을 만들기 위한 설정값들을 구성합니다.
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        // 1 바이트 단위로 사이즈를 설정해야 합니다. 골때리는건 아래 pCode 는 또 데이터를 4바이트 단위로 전달합니다.
        createInfo.codeSize = code.size();
        // pCode 는 uint32_t 형을 요구하기 때문에 캐스팅이 필요합니다. 1바이트 4개짜리 std::vector<char>(4) 는 4바이트 하나인 uint32_t 1개로 묶이기 때문에 데이터 정렬에는 이상이 없습니다.
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        // 드디어 셰이더 모듈을 만들고 그 핸들값을 저장합니다. 셰이더 모듈을 만들고 나면 code 버퍼는 소멸되어도 괜찮습니다.
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create shader module!");
        }

        // 만들어진 셰이더 모듈의 핸들을 반환합니다.
        return shaderModule;
    }



    // 3. 계속해서 매 프레임 렌더
    inline void mainLoop()
    {
        // 사용자가 창을 닫을 때까지는 계속 이벤트 처리를 하면서 루프를 돕니다.
        while (!glfwWindowShouldClose(window))
        {
            // GLFW 윈도우 이벤트 처리
            glfwPollEvents();

            // @@@ 여기서 매 프레임을 랜더링 할 예정
        }
    }


    // 4. 프로그램 종료
    inline void cleanup()
    {
        // 
        vkDestroyPipeline(device, graphicsPipeline, nullptr);

        // 파이프라인 레이아웃은 프로그램 수명 내내 참조되므로 마지막에 삭제해야 합니다.
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

        // 파이프라인 레이아웃과 마찬가지로 렌더 패스는 프로그램 전체에서 참조되므로 마지막에만 정리해야 합니다.
        vkDestroyRenderPass(device, renderPass, nullptr);

        // 이미지와 달리 이미지 뷰는 명시적으로 생성되었으므로 프로그램 종료 시 전부 지워야 합니다.
        for (auto imageView : swapChainImageViews)
        {
            vkDestroyImageView(device, imageView, nullptr);
        }

        // 스왑 체인 개체를 지웁니다.
        vkDestroySwapchainKHR(device, swapChain, nullptr);

        // 추상적 디바이스 개체를 지웁니다.
        vkDestroyDevice(device, nullptr);

        // 디버깅 정보 출력용 메신저 도구 개체를 지웁니다.
        if (enableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        // 서피스 개체를 지웁니다.
        vkDestroySurfaceKHR(instance, surface, nullptr);

        // 불칸 개체를 지웁니다. (인스턴스 핸들, 메모리를 직접 관리하기 위한 커스텀 얼로케이터 콜백)
        vkDestroyInstance(instance, nullptr);

        // GLFW 윈도우를 지웁니다.
        glfwDestroyWindow(window);

        // GLFW 를 종료합니다.
        glfwTerminate();


        // std::unique_ptr 나 std::shared_ptr 등을 써서 우아하게 RAII 를 처리할 수도 있지만 학습을 위해 모든 자원을 명시적으로 소멸합니다.
        // Possible to handle RAII more elegantly with std::unique_ptr or std::shared_ptr etc, but explicitly destroy all resources for learning now.
    }
};


int main()
{
    HelloTriangleApplication app;

    // HelloTriangleApplication 앱을 실행하다 오류 발생시 main 에서 에외를 받습니다. | Exception will propagate back to the main function.
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
Create a VkInstance @
Select a supported graphics card(VkPhysicalDevice) @
Create a VkDeviceand VkQueue for drawing and presentation @
Create a window, window surfaceand swap chain @
Wrap the swap chain images into VkImageView @
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