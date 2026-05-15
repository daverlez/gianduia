#include "app/application.h"
#include "app/IconsFontAwesome6.h"

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

#include "gianduia/core/fileResolver.h"

static Application* s_appInstance = nullptr;

void handleSignal(int signal) {
    if (s_appInstance) {
        std::cout << "\n[Gianduia Headless] Received interrupt (Ctrl+C). Shutting down..." << std::endl;
        s_appInstance->cancelRender();
    }
}

std::string FormatTime(double totalSeconds) {
    int h = static_cast<int>(totalSeconds) / 3600;
    int m = (static_cast<int>(totalSeconds) % 3600) / 60;
    int s = static_cast<int>(totalSeconds) % 60;

    char buf[64];
    if (h > 0) {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
    } else {
        snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
    }
    return std::string(buf);
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

    float baseFontSize = 16.0f;
    float iconFontSize = baseFontSize * 0.8f;

    io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Regular.ttf", baseFontSize);

    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = iconFontSize;

    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    io.Fonts->AddFontFromFileTTF("assets/fonts/fa-solid-900.ttf", iconFontSize, &icons_config, icons_ranges);

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 4.0f;

    style.WindowBorderSize  = 0.0f;
    style.FrameBorderSize   = 0.0f;

    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    style.Colors[ImGuiCol_TitleBg]  = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_Button]        = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.30f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]  = ImVec4(0.15f, 0.20f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_Header]        = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.30f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive]  = ImVec4(0.15f, 0.20f, 0.25f, 1.00f);

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
                } else if (m_viewMode == ViewMode::Depth) {
                    int w = film->width();
                    int h = film->height();

                    static std::vector<float> displayDepth;
                    displayDepth.resize(w * h * 3);

                    const float* rawDepth = film->getDepth().data();

                    float minZ = std::numeric_limits<float>::max();
                    float maxZ = std::numeric_limits<float>::lowest();

                    for (int i = 0; i < w * h; ++i) {
                        float d = rawDepth[i];
                        if (d > 0.0f) {
                            if (d < minZ) minZ = d;
                            if (d > maxZ) maxZ = d;
                        }
                    }

                    if (maxZ <= minZ) {
                        maxZ = minZ + 1.0f;
                    }

                    for (int i = 0; i < w * h; ++i) {
                        float d = rawDepth[i];
                        float val = 0.0f;

                        if (d > 0.0f) {
                            val = (d - minZ) / (maxZ - minZ);
                        }

                        displayDepth[i * 3 + 0] = val;
                        displayDepth[i * 3 + 1] = val;
                        displayDepth[i * 3 + 2] = val;
                    }

                    m_texture.update(w, h, displayDepth.data());
                } else if (m_viewMode == ViewMode::Metallic || m_viewMode == ViewMode::Roughness) {
                    int w = film->width();
                    int h = film->height();

                    static std::vector<float> displayMono;
                    displayMono.resize(w * h * 3);

                    const float* rawData = (m_viewMode == ViewMode::Metallic) ?
                                           film->getMetallic().data() :
                                           film->getRoughness().data();

                    for (int i = 0; i < w * h; ++i) {
                        float val = rawData[i];

                        displayMono[i * 3 + 0] = val;
                        displayMono[i * 3 + 1] = val;
                        displayMono[i * 3 + 2] = val;
                    }

                    m_texture.update(w, h, displayMono.data());
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

                ImGui::DockBuilderDockWindow(ICON_FA_SLIDERS " Controls", dock_id_left);
                ImGui::DockBuilderDockWindow(ICON_FA_IMAGE " Viewport", dock_main_id);

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
    ImGui::Begin(ICON_FA_SLIDERS " Controls");

    if (ImGui::CollapsingHeader(ICON_FA_FOLDER_OPEN " Scene Management", ImGuiTreeNodeFlags_DefaultOpen)) {
        static char buf[128] = "assets/scenes/cbox.xml";
        ImGui::InputText("Path", buf, IM_ARRAYSIZE(buf));

        if (ImGui::Button(ICON_FA_FILE_IMPORT " Load Scene", ImVec2(-1, 0))) {
            loadScene(buf);
        }
    }

    int currentSamples = m_currentSample.load();
    int totalSamples = 0;
    if (m_scene && m_scene->getIntegrator()) {
        totalSamples = m_scene->getSampler()->getSampleCount();
    }

    if (m_scene) {
        if (ImGui::CollapsingHeader(ICON_FA_CAMERA " Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!m_isRendering) {
                if (ImGui::Button(ICON_FA_PLAY " Start Render", ImVec2(-1, 30))) {
                    startRender();
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                if (ImGui::Button(ICON_FA_STOP " Stop Render", ImVec2(-1, 30))) {
                    cancelRender();
                }
                ImGui::PopStyleColor(3);

                float progressFraction = (totalSamples > 0) ? (float)currentSamples / totalSamples : 0.0f;
                char progressOverlay[32];
                snprintf(progressOverlay, sizeof(progressOverlay), "%.1f%% (%d/%d)", progressFraction * 100.0f, currentSamples, totalSamples);
                ImGui::ProgressBar(progressFraction, ImVec2(-1, 0), progressOverlay);
            }

            ImGui::Spacing();

            bool canDenoise = !m_isRendering && currentSamples > 0;
            ImGui::BeginDisabled(!canDenoise);
            if (ImGui::Button(ICON_FA_FILTER " Denoise", ImVec2(-1, 0))) {
                gnd::Denoiser denoiser;
                denoiser.execute(m_scene->getCamera()->getFilm());

                m_textureDirty = true;
                m_scene->getCamera()->getFilm()->saveEXR();
                m_scene->getCamera()->getFilm()->savePNG();
            }
            ImGui::EndDisabled();
        }

        if (ImGui::CollapsingHeader(ICON_FA_DISPLAY " Viewport Options", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text(ICON_FA_CUBE " Viewport Mode:");
            if (ImGui::RadioButton("Render", m_viewportMode == ViewportMode::Render)) m_viewportMode = ViewportMode::Render;
            ImGui::SameLine();
            if (ImGui::RadioButton("Interactive", m_viewportMode == ViewportMode::Interactive)) m_viewportMode = ViewportMode::Interactive;

            if (m_viewportMode == ViewportMode::Interactive) {
                ImGui::Separator();
                ImGui::Text(ICON_FA_CUBES " BVH Filters");
                if (ImGui::Checkbox("Show TLAS (Instances)", &m_showTLAS)) m_bvhFiltersDirty = true;
                if (ImGui::Checkbox("Show BLAS (Geometry)", &m_showBLAS)) m_bvhFiltersDirty = true;
                if (ImGui::SliderInt("Max Depth", &m_maxBvhDepth, 0, m_absoluteMaxDepth)) m_bvhFiltersDirty = true;
            }

            bool canViewBuffers = !m_isRendering && currentSamples > 0;
            if (canViewBuffers && m_viewportMode == ViewportMode::Render) {
                ImGui::Separator();
                ImGui::Text(ICON_FA_LAYER_GROUP " G-Buffer Visualization");

                if (ImGui::BeginTable("GBufferTable", 2, ImGuiTableFlags_SizingStretchProp)) {

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::RadioButton("Beauty", m_viewMode == ViewMode::Beauty)) { m_viewMode = ViewMode::Beauty; m_textureDirty = true; }
                    ImGui::TableSetColumnIndex(1);
                    if (ImGui::RadioButton("Depth", m_viewMode == ViewMode::Depth)) { m_viewMode = ViewMode::Depth; m_textureDirty = true; }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::RadioButton("Albedo", m_viewMode == ViewMode::Albedo)) { m_viewMode = ViewMode::Albedo; m_textureDirty = true; }
                    ImGui::TableSetColumnIndex(1);
                    if (ImGui::RadioButton("Roughness", m_viewMode == ViewMode::Roughness)) { m_viewMode = ViewMode::Roughness; m_textureDirty = true; }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::RadioButton("Normal", m_viewMode == ViewMode::Normal)) { m_viewMode = ViewMode::Normal; m_textureDirty = true; }
                    ImGui::TableSetColumnIndex(1);
                    if (ImGui::RadioButton("Metallic", m_viewMode == ViewMode::Metallic)) { m_viewMode = ViewMode::Metallic; m_textureDirty = true; }

                    ImGui::EndTable();
                }
            }
        }

        if (m_viewportMode == ViewportMode::Render && ImGui::CollapsingHeader(ICON_FA_IMAGE " Post Processing", ImGuiTreeNodeFlags_DefaultOpen)) {

            ImGui::Text("Tonemapping Operator");
            int currentTonemap = static_cast<int>(m_tonemapper);
            const char* items[] = {
                "Linear (None)",
                "Reinhard",
                "Uncharted 2",
                "ACES",
                "Khronos PBR Neutral",
                "AgX (Base)",
                "AgX (Punchy)"
            };

            if (ImGui::Combo("##Tonemapper", &currentTonemap, items, IM_ARRAYSIZE(items))) {
                m_tonemapper = static_cast<TonemapOperator>(currentTonemap);
            }

            ImGui::Spacing();

            bool canSave = !m_isRendering && currentSamples > 0;
            ImGui::BeginDisabled(!canSave);

            if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save edited image", ImVec2(-1, 30))) {
                auto cameraFilm = m_scene->getCamera()->getFilm();
                int renderW = cameraFilm->width();
                int renderH = cameraFilm->height();

                m_postProcessor.resize(renderW, renderH);
                m_postProcessor.render(m_texture.id, m_tonemapper);
                std::vector<float> rawFloatData = m_postProcessor.getTonemappedData();

                gnd::Film exportFilm(renderW, renderH);
                for (int y = 0; y < renderH; ++y) {
                    for (int x = 0; x < renderW; ++x) {
                        int gl_y = y;
                        int idx = (gl_y * renderW + x) * 3;

                        gnd::Color3f color(
                            rawFloatData[idx + 0],
                            rawFloatData[idx + 1],
                            rawFloatData[idx + 2]
                        );
                        exportFilm.setPixel(x, y, color);
                    }
                }

                exportFilm.saveEXR(gnd::FileResolver::getOutputName() + "-postprocessed");
                exportFilm.savePNG(gnd::FileResolver::getOutputName() + "-postprocessed");
            }
            ImGui::EndDisabled();
        }

    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION " No scene loaded.");
    }

    ImGui::Dummy(ImVec2(0.0f, ImGui::GetContentRegionAvail().y - 80.0f));
    ImGui::Separator();
    ImGui::Text(ICON_FA_CHART_SIMPLE " Stats");

    double elapsed = m_renderTime.load();

    ImGui::Text("Sample: %d", currentSamples);
    if (totalSamples > 0) {
        ImGui::SameLine();
        ImGui::Text("/ %d", totalSamples);
    }

    ImGui::Text("Time: %s", FormatTime(elapsed).c_str());

    if (m_isRendering && currentSamples > 0 && totalSamples > 0) {
        double timePerSample = elapsed / currentSamples;
        int samplesRemaining = totalSamples - currentSamples;
        double etaSeconds = timePerSample * samplesRemaining;

        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "ETA:  %s", FormatTime(etaSeconds).c_str());
    } else if (!m_isRendering && currentSamples > 0) {
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Status: Finished");
    }

    ImGui::End();
}

void Application::renderViewport() {
    ImGui::Begin(ICON_FA_IMAGE " Viewport");

    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    if (viewportSize.x > 0 && viewportSize.y > 0) {
        if (m_viewportMode == ViewportMode::Render) {
            if (m_texture.id != 0) {
                float aspect = (float)m_texture.width / (float)m_texture.height;
                float viewAspect = viewportSize.x / viewportSize.y;
                ImVec2 imageSize = viewportSize;
                if (viewAspect > aspect) imageSize.x = viewportSize.y * aspect;
                else imageSize.y = viewportSize.x / aspect;

                m_postProcessor.resize((int)imageSize.x, (int)imageSize.y);
                m_postProcessor.render(m_texture.id, m_tonemapper);

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
    if (m_renderThread.joinable()) m_renderThread.join();

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
    if (m_isRendering && m_scene && m_scene->getIntegrator()) m_scene->getIntegrator()->cancel();
    if (m_renderThread.joinable()) m_renderThread.join();
    m_isRendering = false;
    m_textureDirty = true;  // Force texture synchronization (for coherent post-processing)
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