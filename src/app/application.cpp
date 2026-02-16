#include <app/application.h>

#include <iostream>
#include <chrono>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include "gianduia/core/integrator.h"
#include "gianduia/core/parser.h"


Application::Application() {
    initWindow();
    initImGui();
}

Application::~Application() {
    cancelRender();
    shutdown();
}

void Application::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    m_window = glfwCreateWindow(1280, 720, "Gianduia Path Tracer", nullptr, nullptr);
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
    }

    m_postProcessor.init();
}

void Application::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void Application::run() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        if (m_isRendering) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = now - m_startTime;
            m_renderTime = elapsed.count();
        }

        if (m_textureDirty) {
            std::lock_guard<std::mutex> lock(m_textureMutex);
            if (m_scene && m_scene->getCamera()) {
                m_texture.update(*m_scene->getCamera()->getFilm());
            }
            m_textureDirty = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        renderSidebar();
        renderViewport();

        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(m_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }
}

void Application::renderSidebar() {
    ImGui::Begin("Controls");

    ImGui::Text("Scene Loader");
    static char buf[128] = "../scenes/cbox.xml";
    ImGui::InputText("Path", buf, IM_ARRAYSIZE(buf));

    if (ImGui::Button("Load Scene")) {
        loadScene(buf);
    }

    ImGui::Separator();

    ImGui::Text("Rendering");
    if (m_scene) {
        if (!m_isRendering) {
            if (ImGui::Button("Start Render", ImVec2(-1, 0))) {
                startRender();
            }
        } else {
            if (ImGui::Button("Stop Render", ImVec2(-1, 0))) {
                cancelRender();
            }
            ImGui::ProgressBar(-1.0f * (float)ImGui::GetTime(), ImVec2(0.0f, 0.0f), "Rendering...");
        }
    } else {
        ImGui::TextColored(ImVec4(1,0,0,1), "No scene loaded.");
    }

    ImGui::Separator();
    ImGui::Text("Stats");
    ImGui::Text("Sample: %d", m_currentSample.load());
    ImGui::Text("Time: %.2fs", m_renderTime.load());

    ImGui::End();
}

void Application::renderViewport() {
    ImGui::Begin("Viewport");

    if (m_texture.id != 0) {
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        m_postProcessor.resize((int)viewportSize.x, (int)viewportSize.y);

        m_postProcessor.render(m_texture.id);

        float aspect = (float)m_texture.width / (float)m_texture.height;
        float viewAspect = viewportSize.x / viewportSize.y;
        ImVec2 imageSize = viewportSize;
        if (viewAspect > aspect) imageSize.x = viewportSize.y * aspect;
        else imageSize.y = viewportSize.x / aspect;

        ImGui::SetCursorPosX((viewportSize.x - imageSize.x) * 0.5f);

        ImGui::Image((ImTextureID)(uintptr_t)m_postProcessor.getOutputTexture(),
                     imageSize, ImVec2(0, 0), ImVec2(1, 1));
    }

    ImGui::End();
}

void Application::loadScene(const std::string& path) {
    cancelRender();
    try {
        std::shared_ptr<gnd::GndObject> root = gnd::Parser::loadFromXML(path);
        m_scene = std::static_pointer_cast<gnd::Scene>(root);
        m_scene->activate();
        m_textureDirty = true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading scene: " << e.what() << std::endl;
    }
}

void Application::startRender() {
    if (!m_scene) return;

    m_isRendering = true;
    m_currentSample = 0;

    m_startTime = std::chrono::high_resolution_clock::now();
    m_renderTime = 0.0;

    if (auto integrator = m_scene->getIntegrator()) {
        integrator->setCallback([this](int sample, const gnd::Bitmap& film) {
             this->onRenderUpdate(sample, film);
        });
    }

    m_renderThread = std::thread([this]() {
        m_scene->render();
        m_isRendering = false;
    });
    m_renderThread.detach();
}

void Application::cancelRender() {
    if (m_isRendering && m_scene && m_scene->getIntegrator())
        m_scene->getIntegrator()->cancel();
}

void Application::onRenderUpdate(int sample, const gnd::Bitmap& film) {
    std::lock_guard<std::mutex> lock(m_textureMutex);
    m_currentSample = sample + 1;
    m_textureDirty = true;
}

void Application::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(m_window);
    glfwTerminate();
}
