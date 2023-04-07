//
// Created by Maksym Pasichnyk on 07.04.2023.
//

#pragma once

#define VK_NO_PROTOTYPES
#define VK_ENABLE_BETA_EXTENSIONS

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include "ManagedObject.hpp"

class WindowPlatform : public ManagedObject {
public:
    WindowPlatform(const char* title, u32 width, u32 height) {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(static_cast<i32>(width), static_cast<i32>(height), title, nullptr, nullptr);
    }

    ~WindowPlatform() override {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    auto CreateWindowSurface(vk::Instance instance, vk::SurfaceKHR* surface) -> vk::Result {
        VkResult result = glfwCreateWindowSurface(instance, window, nullptr, reinterpret_cast<VkSurfaceKHR*>(surface));
        return static_cast<vk::Result>(result);
    }

    auto GetNativeWindow() -> GLFWwindow* {
        return window;
    }

    auto PumpEvents() -> bool {
        glfwPollEvents();
        return !glfwWindowShouldClose(window);
    }

private:
    GLFWwindow* window;
};
