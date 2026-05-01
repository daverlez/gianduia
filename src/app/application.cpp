#include <app/application.h>
#include "gianduia/core/integrator.h"
#include "gianduia/core/parser.h"
#include "gianduia/core/denoiser.h"

#include <iostream>
#include <chrono>
#include <csignal>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#include <GLFW/glfw3.h>

static Application* s_appInstance = nullptr;

void handleSignal(int signal) {
    if (s_appInstance) {
        std::cout << "\n[Gianduia Headless] Received interrupt (Ctrl+C). Shutting down..." << std::endl;
        s_appInstance->cancelRender();
    }
}

Application::Application(bool headless) : m_headless(headless) {
    s_appInstance = this;

    if (!m_headless) {
        initWindow();
        initImGui();
    }
}

Application::~Application() {
    cancelRender();
    if (!m_headless) {
        shutdown();
    }
    s_appInstance = nullptr;
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
    m_debugRenderer.init();
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
            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<double> elapsed = now - m_startTime;
            m_renderTime = elapsed.count();
        }

        if (m_textureDirty) {
            std::lock_guard<std::mutex> lock(m_textureMutex);
            if (m_scene && m_scene->getCamera()) {
                auto film = m_scene->getCamera()->getFilm();

                if (m_viewMode == ViewMode::Beauty) {
                    m_texture.update(film->width(), film->height(), reinterpret_cast<float*>(film->getRadiance().data()));
                } else if (m_viewMode == ViewMode::Albedo) {
                    m_texture.update(film->width(), film->height(), reinterpret_cast<float*>(film->getAlbedo().data()));
                } else if (m_viewMode == ViewMode::Normal) {
                    m_texture.update(film->width(), film->height(), reinterpret_cast<float*>(film->getNormal().data()));
                }
            }
            m_textureDirty = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        static bool first_time = true;
        if (first_time) {
            first_time = false;

            if (ImGui::DockBuilderGetNode(dockspace_id) == NULL) {
                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

                ImGuiID dock_main_id = dockspace_id;
                ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.25f, NULL, &dock_main_id);

                ImGui::DockBuilderDockWindow("Controls", dock_id_left);
                ImGui::DockBuilderDockWindow("Viewport", dock_main_id);

                ImGui::DockBuilderFinish(dockspace_id);
            }
        }

        ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport());
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
        ImGui::Separator();
        ImGui::Text("Post-Processing");

        bool canDenoise = !m_isRendering && m_currentSample > 0;
        ImGui::BeginDisabled(!canDenoise);
        if (ImGui::Button("Apply OIDN Denoise", ImVec2(-1, 0))) {
            gnd::Denoiser denoiser;
            denoiser.execute(m_scene->getCamera()->getFilm());

            m_textureDirty = true;

            m_scene->getCamera()->getFilm()->saveEXR();
            m_scene->getCamera()->getFilm()->savePNG();
        }
        ImGui::EndDisabled();

        ImGui::Separator();
        ImGui::Text("Viewport Mode");
        if (ImGui::RadioButton("Path Tracer", m_viewportMode == ViewportMode::Render)) {
            m_viewportMode = ViewportMode::Render;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Interactive 3D", m_viewportMode == ViewportMode::Interactive)) {
            m_viewportMode = ViewportMode::Interactive;
        }

        if (m_viewportMode == ViewportMode::Interactive) {
            ImGui::Separator();
            ImGui::Text("BVH Filters");

            if (ImGui::Checkbox("Show TLAS (Instances)", &m_showTLAS)) m_bvhFiltersDirty = true;
            if (ImGui::Checkbox("Show BLAS (Geometry)", &m_showBLAS)) m_bvhFiltersDirty = true;
            if (ImGui::SliderInt("Max Depth", &m_maxBvhDepth, 0, m_absoluteMaxDepth)) m_bvhFiltersDirty = true;
        }

        bool canViewBuffers = !m_isRendering && m_currentSample > 0;
        if (canViewBuffers) {
            ImGui::Separator();
            ImGui::Text("G-Buffer Visualization");

            if (ImGui::RadioButton("Beauty", m_viewMode == ViewMode::Beauty)) {
                m_viewMode = ViewMode::Beauty;
                m_textureDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Albedo", m_viewMode == ViewMode::Albedo)) {
                m_viewMode = ViewMode::Albedo;
                m_textureDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Normal", m_viewMode == ViewMode::Normal)) {
                m_viewMode = ViewMode::Normal;
                m_textureDirty = true;
            }
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

    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    if (viewportSize.x > 0 && viewportSize.y > 0) {
        if (m_viewportMode == ViewportMode::Render) {
            if (m_texture.id != 0) {
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
        }
        else if (m_viewportMode == ViewportMode::Interactive) {
            ImVec2 imageSize = viewportSize;

            if (m_bvhFiltersDirty) {
                updateBvhBuffers();
            }

            if (ImGui::IsWindowHovered()) {
                ImGuiIO& io = ImGui::GetIO();

                if (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    m_interactiveCamera.update(io.MouseDelta.x, io.MouseDelta.y, 0.0f);
                }
                if (io.MouseWheel != 0.0f) {
                    m_interactiveCamera.update(0.0f, 0.0f, io.MouseWheel);
                }
            }

            m_debugRenderer.resize((int)imageSize.x, (int)imageSize.y);
            m_debugRenderer.render(m_interactiveCamera);

            ImGui::Image((ImTextureID)(uintptr_t)m_debugRenderer.getOutputTexture(),
                         imageSize, ImVec2(0, 1), ImVec2(1, 0));
        }
    }

    ImGui::End();
}

void Application::loadScene(const std::string& path) {
    cancelRender();
    try {
        m_viewMode = ViewMode::Beauty;
        m_viewportMode = ViewportMode::Render;

        m_isRendering = false;
        m_renderTime = 0.0f;
        m_currentSample = 0;

        std::shared_ptr<gnd::GndObject> root = gnd::Parser::loadFromXML(path);
        m_scene = std::static_pointer_cast<gnd::Scene>(root);
        m_textureDirty = true;

        std::cout << m_scene->toString() << std::endl;

        m_cachedBvhNodes = m_scene->getBvhDebugData();

        m_maxTlasDepth = 0;
        m_maxBlasDepth = 0;

        for (const auto& node : m_cachedBvhNodes) {
            if (node.isBlas) {
                m_maxBlasDepth = std::max(m_maxBlasDepth, node.depth);
            } else {
                m_maxTlasDepth = std::max(m_maxTlasDepth, node.depth);
            }
        }

        m_absoluteMaxDepth = std::max(m_maxTlasDepth, m_maxBlasDepth);
        m_maxBvhDepth = m_absoluteMaxDepth;

        m_showTLAS = true;
        m_showBLAS = true;
        updateBvhBuffers();

        if (!m_cachedBvhNodes.empty()) {
            glm::vec3 globalMin(std::numeric_limits<float>::max());
            glm::vec3 globalMax(-std::numeric_limits<float>::max());

            for (const auto& node : m_cachedBvhNodes) {
                globalMin = glm::min(globalMin, glm::vec3(node.bounds.pMin.x(), node.bounds.pMin.y(), node.bounds.pMin.z()));
                globalMax = glm::max(globalMax, glm::vec3(node.bounds.pMax.x(), node.bounds.pMax.y(), node.bounds.pMax.z()));
            }

            glm::vec3 center = (globalMin + globalMax) * 0.5f;
            float sceneSize = glm::length(globalMax - globalMin);

            m_interactiveCamera.setTarget(center);
            m_interactiveCamera.setRadius(sceneSize * 0.8f);
            m_interactiveCamera.setAngles(30.0f, 45.0f);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error loading scene: " << e.what() << std::endl;
    }
}

void Application::updateBvhBuffers() {
    std::vector<gnd::BvhDebugNode> filteredNodes;
    filteredNodes.reserve(m_cachedBvhNodes.size());

    for (const auto& node : m_cachedBvhNodes) {
        if (node.depth > m_maxBvhDepth) continue;
        if (node.isBlas && !m_showBLAS) continue;
        if (!node.isBlas && !m_showTLAS) continue;

        filteredNodes.push_back(node);
    }

    m_debugRenderer.buildBuffers(filteredNodes, m_maxTlasDepth, m_maxBlasDepth);
    m_bvhFiltersDirty = false;
}

void Application::startRender() {
    if (!m_scene) return;

    m_viewMode = ViewMode::Beauty;
    m_isRendering = true;
    m_currentSample = 0;

    m_startTime = std::chrono::steady_clock::now();
    m_renderTime = 0.0;

    if (auto integrator = m_scene->getIntegrator()) {
        integrator->setCallback([this](int sample, const gnd::Film& film) {
             this->onRenderUpdate(sample, film);
        });
    }

    m_renderThread = std::thread([this]() {
        m_scene->render();
        m_isRendering = false;
    });
}

void Application::cancelRender() {
    if (m_isRendering && m_scene && m_scene->getIntegrator()) {
        m_scene->getIntegrator()->cancel();
        if (m_renderThread.joinable()) m_renderThread.join();
        m_isRendering = false;
    }
}

void Application::onRenderUpdate(int sample, const gnd::Film& film) {
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

void Application::runHeadless(const std::string& scenePath, bool applyDenoise) {
    std::signal(SIGINT, handleSignal);

    std::cout << "[Gianduia Headless] Loading scene: " << scenePath << std::endl;
    loadScene(scenePath);

    if (!m_scene) {
        std::cerr << "[Gianduia Headless] Cannot load scene " << scenePath << ". Shutting down" << std::endl;
        return;
    }

    std::cout << "[Gianduia Headless] Starting render... (Press Ctrl+C to interrupt and save)" << std::endl;
    auto start = std::chrono::steady_clock::now();

    m_isRendering = true;
    m_scene->render();

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    m_isRendering = false;
    std::cout << "[Gianduia Headless] Rendering finished in " << elapsed.count() << " seconds." << std::endl;

    if (applyDenoise) {
        gnd::Denoiser denoiser;
        denoiser.execute(m_scene->getCamera()->getFilm());

        m_scene->getCamera()->getFilm()->saveEXR();
        m_scene->getCamera()->getFilm()->savePNG();
    }
}
