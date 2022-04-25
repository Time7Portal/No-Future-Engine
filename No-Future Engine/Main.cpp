#define GLFW_INCLUDE_VULKAN // 이렇게 정의해두면 glfw3.h 에서 vulkan.h 도 같이 불러옵니다.
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // OpenGL과 다르게 0 ~ 1 스케일 깊이 값을 사용합니다.
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>
#include <stdexcept> // 예외처리
#include <vector>
#include <cstring> // strcmp 사용
#include <cstdlib> // EXIT_SUCCESS, EXIT_FAILURE 매크로
#include <optional>


// 윈도우 해상도
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

// 검증 레이어 목록
const std::vector<const char*> validationLayers {
    "VK_LAYER_KHRONOS_validation", // LunarG Vulkan SDK 에서 기본적으로 제공하는 유효성 검사 레이어
};
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true; // 디버그 모드일때만 검증 레이어 활성화
#endif


// PFN_vkCreateDebugUtilsMessengerEXT 함수는 디버깅 정보 출력용 메신저 도구 개체를 만들때 사용하며, 안타깝게도 확장 함수이기 때문에 자동으로 로드되지 않습니다. vkGetInstanceProcAddr 를 사용하여 함수 주소를 직접 찾아야 합니다. 이를 처리하는 자체 프록시(래퍼) 함수를 만들 것입니다.
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
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
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

// 큐 패밀리가 존재하지 않음을 매직 넘버 (예를들어 0) 으로 표현하는 것은 불가능하므로 C++17 문법중에 std::optional 을 사용하여 값이 존재하거나 존재하지 않음을 표현하였습니다. std::optional 는 무언가를 할당하기 전에는 값을 가지지 않습니다.
struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;

    bool isComplete()
    {
        return graphicsFamily.has_value(); // graphicsFamily 에 한번이라도 값을 넣은 적이 있다면 has_value() 는 true 를 반환합니다.
    }
};


class HelloTriangleApplication
{
private:
    GLFWwindow* window;                                 // GLFW 윈도우 핸들
    VkInstance instance;                                // Vulkan 개체 핸들
    VkDebugUtilsMessengerEXT debugMessenger;            // 디버깅 정보 출력용 메신저 도구 핸들. 놀랍게도 Vulkan의 디버그 콜백조차도 명시적으로 생성 및 소멸되어야 하는 핸들로 관리됩니다. 이러한 콜백은 디버그 메신저 의 일부이며 원하는 만큼 가질 수 있습니다.
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;   // 선택된 그래픽카드 디바이스 핸들. vkInstance 와 함께 자동으로 소멸됩니다.
    VkDevice device;                                    // 논리적 디바이스 핸들. 그래픽카드와 통신하기 위한 인터페이스 입니다. 하나의 그래픽카드에 여러개의 논리적 디바이스를 만들 수도 있습니다.
    VkQueue graphicsQueue;                              // 그래픽 큐 핸들. 사실 큐는 논리적 디바이스를 만들때 같이 만들어집니다. 하지만 만들어질 그래픽 큐를 다룰 수 있는 핸들을 따로 만들어 관리해야 합니다. VkDevice 와 함께 자동으로 소멸됩니다.


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

        // createSurface();            // 2-2. 화면 표시를 위한 서피스 생성 및 GLFW 윈도우에 연결

        pickPhysicalDevice();       // 2-3. 그래픽 카드 선택

        createLogicalDevice();      // 2-4. 그래픽 카드와 통신하기 위한 인터페이스 생성

        //createCommandPool();        // 2-5. 그래픽 카드로 보낼 명령 풀 생성 : 추후 command buffer allocation 에 사용할 예정
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

            // 지원 가능한 검증 레이어 목록을 조회하여, 우리가 필요로 하는 검증 레이어가 전부 들어있는지 확인
            // Checks if all of the requested layers are available.
            for (const char* layerName : validationLayers) // 우리가 필요로 하는 검증 레이어들 중
            {
                bool layerFound = false;
                for (const auto& layerProperties : availableLayers) // 장치에서 지원 가능한 검증 레이어 목록을 조회
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
                    throw std::runtime_error("Validation layers requested, but not available! - Did you install Vulakn SDK?");
                }
            }
        }



        // 앱에 대한 정보를 제공합니다. (필수는 아니지만 이 정보를 가지고 그래픽 드라이버가 최적화 하는데 사용하기도 합니다)
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

            // 디버그 메신저도 설정하여 createInfo 에 집어넣습니다. 이때 우리가 만든 콜백 함수를 호출할 조건들을 설정할 수 있습니다.
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
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        // 심각도에 따른 검증 레이어 메세지 출력
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            // 여기에 중단점을 추가하면 쉽게 디버깅이 가능합니다.
            std::cerr << "@ [ERROR] : " << pCallbackData->pMessage << std::endl;
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            std::cerr << "@ [WARNING] : " << pCallbackData->pMessage << std::endl;
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
    

    // 2-3. 그래픽 카드 선택
    void pickPhysicalDevice()
    {
        // 우선 Vulkan을 지원하는 그래픽카드 갯수를 받아옵니다.
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

        for (const auto& device : devices)
        {
            // 해당 그래픽카드가 우리가 사용해야할 큐 패밀리를 그래픽카드가 모두 지원하는지 확인해야 합니다.
            std::optional<uint32_t> graphicsFamily = findQueueFamilies(device);

            if (graphicsFamily.has_value() == true) // 원하는 큐 패밀리가 들어있다면
            {
                // 적합한 그래픽카드를 하나 찾았으면 핸들에 보관해두고 더이상 탐색하지 않습니다.
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

        // 찾은 그래픽카드 정보를 모아서 출력합니다.
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
    std::optional<uint32_t> findQueueFamilies(VkPhysicalDevice device)
    {
        // graphicsFamily 에는 그래픽카드가 해당 큐 패밀리를 지원하는지 여부와 지원한다면 해당 큐 페밀리의 인덱스 번호를 담고 있습니다.
        // 적합한 큐 패밀리가 존재하지 않음을 매직 넘버 (예를들어 0) 으로 표현하는 것은 불가능하므로 C++17 문법중에 std::optional 을 사용하여 값이 존재하거나 존재하지 않음을 표현하였습니다. std::optional 는 무언가를 할당하기 전에는 값을 가지지 않습니다. https://modoocode.com/309
        std::optional<uint32_t> graphicsFamily;

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
                graphicsFamily = i; // 값을 썼으므로 indices.isComplete() 했을때 true 가 반환 될것입니다.
            }

            // 이미 원하는 큐 패밀리를 찾았으므로 추가로 찾을 필요가 없습니다.
            if (graphicsFamily.has_value()) // graphicsFamily 에 값을 한번이라도 넣은 적이 있다면 graphicsFamily.has_value() 는 true 를 반환합니다.
            {
                break;
            }

            i++;
        }

        return graphicsFamily; // 지원하는 큐 페밀리의 인덱스 번호를 담고 있습니다. 또한, graphicsFamily 에 값을 한번도 넣은 적이 없다면 (지원하는 큐 페밀리를 찾지 못한 경우) graphicsFamily.has_value() 는 false 를 반환합니다.
    }


    // 2-4. 그래픽 카드와 통신하기 위한 인터페이스 생성
    void createLogicalDevice()
    {
        // graphicsFamily 에는 그래픽카드가 해당 큐 패밀리를 지원하는지 여부와 지원한다면 해당 큐 페밀리의 인덱스 번호를 담고 있습니다.
        std::optional<uint32_t> graphicsFamily = findQueueFamilies(physicalDevice);

        // 2-4-1. 하나의 큐 패밀리 안에서 사용할 큐의 갯수를 설정합니다. 지금은 그래픽스 큐 하나만 달라고 요청하겠습니다. 사실 큐는 그렇게 많이 필요하지 않으며, 일반적으로 1개면 충분합니다. 왜냐하면 커멘드 버퍼들을 멀티 스레드로 많이 만든 후에 큐로 제출할때는 메인 스레드에서 한번에 모아 하나의 큐로 묶어 제출하기 때문입니다. Vulkan에서 큐는 커맨드 버퍼를 그래픽카드에 공급하는 매체 역할을 합니다.
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsFamily.value();
        queueCreateInfo.queueCount = 1;

        // 큐의 우선순위를 0.1 ~ 1 스케일로 할당할 수 있습니다. 하나의 큐만 사용하더라도 반드시 지정해야 합니다. 1.0f 는 우선순위가 가장 높음을 의미합니다.
        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;


        // 2-4-2. 우리가 사용할 장치의 기능들을 정의합니다. 장치에서 지원하는 모든 기능들은 vkGetPhysicalDeviceFeatures 로 확인 가능합니다. 사실상 지금은 아무런 기능들도 사용하지 않으므로 모든 설정값을 기본(VK_FALSE)으로 두겠습니다.
        VkPhysicalDeviceFeatures deviceFeatures{};


        // 2-4-3. 이제 논리 장치를 만듭니다. 논리 장치를 만들때 위에서 미리 만들어둔 VkDeviceQueueCreateInfo, VkPhysicalDeviceFeatures 두개의 설정값을 사용합니다.
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.queueCreateInfoCount = 1;

        // 불칸 인스턴스 생성과 마찬가지로 확장과 검증 레이어를 명시해야 합니다. 예를들어 이미지를 윈도우에 렌더링하도록 도와주는 VK_KHR_swapchain 확장은 비트코인 채굴용 그래픽카드에서는 제공하지 않고 단지 컴퓨트 연산만 제공할 수도 있습니다.
        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = 0;

        // 초창기 불칸에서는 인스턴스와 디바이스 전용 검증 레이어를 분리해서 다뤘지만 현재는 인스턴스에만 적용하면 모든 검증을 다 할 수 있도록 바뀌었습니다. 때문에 최신버전의 Vulkan 에서 VkDeviceCreateInfo 의 enabledLayerCount 와 ppEnabledLayerNames 항목은 사실상 무의미해졌지만 구버전 호환성을 위해 불칸 인스턴스 생성때와 동일한 검증 레이어를 집어넣었습니다.
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        // 이제 논리적 디바이스를 만들기 위한 설정들을 모두 마쳤으며, 논리적 디바이스를 생성할 수 있습니다. vkCreateDevice(사용할 그래픽카드, 논리 장치를 만들기 위한 설정값 포인터, 얼로케이션 콜백 포인터, 논리적 디바이스 핸들을 저장할 변수에 대한 포인터)
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
        {
            // 존재하지 않는 확장을 활성화하거나 지원되지 않는 기능을 사용하면 논리적 디바이스 생성에 실패하고 에러를 반환합니다!
            throw std::runtime_error("Failed to create logical device!");
        }

        // 각각의 큐 페밀리에 해당하는 큐 핸들을 받아옵니다. vkGetDeviceQueue(논리적 디바이스, 큐 페밀리 인덱스, 큐 인덱스, 큐 핸들을 저장할 변수에 대한 포인터)
        vkGetDeviceQueue(device, graphicsFamily.value(), 0, &graphicsQueue);

        // 이제 논리적 디바이스와 큐 핸들을 사용하여 실제로 그래픽 카드에 명령을 때려넣어 작업을 시작할 수 있습니다.
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
        vkDestroyDevice(device, nullptr);

        // 디버깅 정보 출력용 메신저 도구 개체를 지웁니다.
        if (enableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        // 불칸 개체를 지웁니다. (인스턴스 핸들, 메모리를 직접 관리하기 위한 커스텀 얼로케이터 콜백)
        vkDestroyInstance(instance, nullptr);

        // GLFW 윈도우를 지웁니다.
        glfwDestroyWindow(window);

        // GLFW 를 종료합니다.
        glfwTerminate();


        // std::unique_ptr 나 std::shared_ptr 등을 써서 우아하게 RAII 를 처리할 수도 있지만 학습을 위해 모든 자원을 명시적으로 소멸합니다. | Possible to handle RAII more elegantly with std::unique_ptr or std::shared_ptr etc, but explicitly destroy all resources for learning now.
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