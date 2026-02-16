#pragma once
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

#include <app/glTexture.h>
#include <gianduia/scene/scene.h>

struct GLFWwindow; // GLFW forward declaration

class Application {
public:
    Application();
    ~Application();

    void run();

private:
    void initWindow();
    void initImGui();
    void shutdown();

    void renderUI();
    void renderViewport();
    void renderSidebar();

    void loadScene(const std::string& path);
    void startRender();
    void cancelRender();

    void onRenderUpdate(int sample, const gnd::Bitmap& film);

private:
    GLFWwindow* m_window = nullptr;

    std::shared_ptr<gnd::Scene> m_scene;
    std::string m_scenePath;

    std::thread m_renderThread;
    std::atomic<bool> m_isRendering = false;
    std::atomic<int> m_currentSample = 0;
    std::atomic<double> m_renderTime = 0.0;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_startTime;

    glTexture m_texture;
    std::mutex m_textureMutex;
    bool m_textureDirty = false;

};