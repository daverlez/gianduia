#pragma once
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

#include "app/glTexture.h"
#include "app/interactiveCamera.h"
#include "app/postProcessor.h"
#include "app/debugRenderer.h"

#include <gianduia/scene/scene.h>

struct GLFWwindow;

class Application {
public:
    explicit Application(bool headless = false);
    ~Application();

    void run();
    void runHeadless(const std::string& scenePath, bool applyDenoise = false);
    void cancelRender();

private:
    void initWindow();
    void initImGui();
    void shutdown();

    void renderUI();
    void renderViewport();
    void renderSidebar();

    void loadScene(const std::string& path);
    void startRender();

    void onRenderUpdate(int sample, const gnd::Film& film);

private:
    enum class ViewMode {
        Beauty,
        Albedo,
        Normal
    };

    enum class ViewportMode {
        Render,
        Interactive
    };

    ViewportMode m_viewportMode = ViewportMode::Render;
    ViewMode m_viewMode = ViewMode::Beauty;

    bool m_headless;

    GLFWwindow* m_window = nullptr;

    std::shared_ptr<gnd::Scene> m_scene;
    std::string m_scenePath;

    std::thread m_renderThread;
    std::atomic<bool> m_isRendering = false;
    std::atomic<int> m_currentSample = 0;
    std::atomic<double> m_renderTime = 0.0;
    std::chrono::steady_clock::time_point m_startTime;

    glTexture m_texture;
    std::mutex m_textureMutex;
    bool m_textureDirty = false;

    PostProcessor m_postProcessor;
    InteractiveCamera m_interactiveCamera;
    DebugRenderer m_debugRenderer;

    std::vector<gnd::BvhDebugNode> m_cachedBvhNodes;
    bool m_showTLAS = true;
    bool m_showBLAS = true;
    int m_maxBvhDepth = 0;
    bool m_bvhFiltersDirty = false;
    int m_maxTlasDepth = 0;
    int m_maxBlasDepth = 0;
    int m_absoluteMaxDepth = 0;

    void updateBvhBuffers();
};