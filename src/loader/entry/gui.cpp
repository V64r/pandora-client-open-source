#include "gui.h"
#include "../imgui/font_data.h"
#include "../imgui/logo_data.h"
#include "../injector/injector.h"
#include <iostream>
#include <tlhelp32.h>
#include <psapi.h>
#include <random>
#include <filesystem>
#include <cmath>
#include <thread>
#include <chrono>
#define STB_IMAGE_IMPLEMENTATION
#include "../imgui/stb_image.h"

#pragma comment(lib, "psapi.lib")

namespace gui {

    c_gui::c_gui() : rng(std::random_device{}()) {}

    c_gui::~c_gui() {
        shutdown();
    }

    bool c_gui::create_device_d3d(HWND hwnd) {
        RECT rect;
        GetClientRect(hwnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.Width = width;
        sd.BufferDesc.Height = height;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;

        UINT create_device_flags = 0;
        D3D_FEATURE_LEVEL feature_level;
        const D3D_FEATURE_LEVEL feature_level_array[2] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_0,
        };

        HRESULT res = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            create_device_flags,
            feature_level_array,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &swap_chain,
            &d3d_device,
            &feature_level,
            &d3d_device_context
        );

        return res == S_OK;
    }

    void c_gui::cleanup_device_d3d() {
        cleanup_render_target();
        if (swap_chain) { swap_chain->Release(); swap_chain = nullptr; }
        if (d3d_device_context) { d3d_device_context->Release(); d3d_device_context = nullptr; }
        if (d3d_device) { d3d_device->Release(); d3d_device = nullptr; }
    }

    void c_gui::create_render_target() {
        ID3D11Texture2D* back_buffer = nullptr;
        swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
        if (back_buffer) {
            d3d_device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view);
            back_buffer->Release();
        }
    }

    void c_gui::cleanup_render_target() {
        if (render_target_view) { render_target_view->Release(); render_target_view = nullptr; }
    }

    bool c_gui::load_logo_from_memory(const unsigned char* data, size_t dataSize) {
        int width, height, channels;
        unsigned char* image_data = stbi_load_from_memory(data, (int)dataSize, &width, &height, &channels, 4);

        if (!image_data) {
            return false;
        }

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = image_data;
        initData.SysMemPitch = width * 4;

        ID3D11Texture2D* texture = nullptr;
        HRESULT hr = d3d_device->CreateTexture2D(&desc, &initData, &texture);

        stbi_image_free(image_data);

        if (FAILED(hr)) {
            return false;
        }

        ID3D11ShaderResourceView* shaderResourceView = nullptr;
        hr = d3d_device->CreateShaderResourceView(texture, nullptr, &shaderResourceView);
        texture->Release();

        if (FAILED(hr)) {
            return false;
        }

        logoTexture = (ImTextureID)shaderResourceView;
        logoWidth = width;
        logoHeight = height;

        return true;
    }

    bool c_gui::check_dll_exists() {
        std::string dllPath = std::filesystem::current_path().string() + "\\Swift.dll";
        return std::filesystem::exists(dllPath);
    }

    void c_gui::refresh_processes() {
        processes.clear();

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return;

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(snapshot, &pe32)) {
            do {
                std::string processName = pe32.szExeFile;

                if (processName == "javaw.exe") {
                    ProcessInfo info;
                    info.name = "javaw.exe";
                    info.pid = pe32.th32ProcessID;
                    info.window_title = "";
                    info.is_selected = false;
                    info.hover_animation = 0.0f;
                    info.is_hovered = false;

                    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
                        auto* info = reinterpret_cast<ProcessInfo*>(lParam);
                        DWORD pid;
                        GetWindowThreadProcessId(hwnd, &pid);
                        if (pid == info->pid && IsWindowVisible(hwnd)) {
                            wchar_t title[256];
                            GetWindowTextW(hwnd, title, 256);
                            std::wstring wtitle(title);
                            std::string stitle(wtitle.begin(), wtitle.end());
                            if (!stitle.empty()) {
                                info->window_title = stitle;
                            }
                            return FALSE;
                        }
                        return TRUE;
                        }, reinterpret_cast<LPARAM>(&info));

                    if (info.window_title.empty()) {
                        info.window_title = "Minecraft " + std::to_string(processes.size() + 1);
                    }

                    processes.push_back(info);
                }
            } while (Process32Next(snapshot, &pe32));
        }

        CloseHandle(snapshot);

        if (selected_index >= (int)processes.size()) {
            selected_index = -1;
        }
    }

    bool c_gui::initialize(HWND hwnd) {
        window_handle = hwnd;

        if (!create_device_d3d(hwnd))
            return false;

        create_render_target();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        ImFont* customFont = io.Fonts->AddFontFromMemoryTTF(
            (void*)fontData,
            fontDataSize,
            18.0f
        );

        if (customFont) {
            fontMain = customFont;
            fontSmall = io.Fonts->AddFontFromMemoryTTF((void*)fontData, fontDataSize, 14.0f);
            fontBig = io.Fonts->AddFontFromMemoryTTF((void*)fontData, fontDataSize, 32.0f);
        }
        else {
            io.Fonts->AddFontDefault();
            MessageBoxW(nullptr, L"Failed to load embedded font! Using default font.", L"Warning", MB_ICONWARNING);
        }

        io.Fonts->Build();

        if (!load_logo_from_memory(logoData, logoDataSize)) {
            MessageBoxW(nullptr, L"Failed to load logo!", L"Warning", MB_ICONWARNING);
        }

        ImGuiStyle& style = ImGui::GetStyle();

        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_Border] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
        style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        style.Colors[ImGuiCol_Button] = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);

        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);

        style.WindowRounding = 0.0f;
        style.ChildRounding = 0.0f;
        style.FrameRounding = 10.0f;
        style.GrabRounding = 8.0f;
        style.PopupRounding = 0.0f;
        style.ScrollbarRounding = 8.0f;

        style.WindowPadding = ImVec2(0.0f, 0.0f);
        style.FramePadding = ImVec2(8.0f, 6.0f);
        style.ItemSpacing = ImVec2(10.0f, 10.0f);
        style.ItemInnerSpacing = ImVec2(8.0f, 8.0f);

        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(d3d_device, d3d_device_context);

        slide_started = true;
        refresh_processes();

        logo_slide_offset = 0.0f;
        logo_sliding = false;
        window_fade = 0.0f;
        instances_fade = 0.0f;
        instances_fading = false;
        loading_fade = 0.0f;
        loading_fading = false;

        if (processes.size() == 1 && !is_loading && !injection_done) {
            selected_index = 0;
            selected_process = processes[0].window_title;
            is_loading = true;
            load_timer = 0.0f;
            load_progress = 0.0f;
            load_complete = false;
            loading_fade = 0.0f;
            phase_timer = 0.0f;
            current_phase = 0;
            phase_paused = false;
            pause_timer = 0.0f;
            injection_executed = false;
        }

        return true;
    }

    void c_gui::start_injection() {
        if (selected_index >= 0 && selected_index < (int)processes.size()) {
            DWORD pid = processes[selected_index].pid;
            std::string dllPath = std::filesystem::current_path().string() + "\\Swift.dll";

            if (std::filesystem::exists(dllPath)) {
                injector::instance = std::make_unique<injector::c_injector>(pid);
                injector::instance->dll_path = dllPath;
                injector::instance->inject();
                injection_done = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                ExitProcess(0);
            }
            else {
                MessageBoxA(NULL, "Swift.dll not found in the loader directory!", "Error", MB_ICONERROR);
                ExitProcess(0);
            }
        }
    }

    void c_gui::perform_injection() {
        if (selected_index >= 0 && selected_index < (int)processes.size()) {
            DWORD pid = processes[selected_index].pid;
            std::string dllPath = std::filesystem::current_path().string() + "\\Swift.dll";

            if (std::filesystem::exists(dllPath)) {
                injector::instance = std::make_unique<injector::c_injector>(pid);
                injector::instance->dll_path = dllPath;
                injector::instance->inject();
            }
            else {
                MessageBoxA(NULL, "Swift.dll not found in the loader directory!", "Error", MB_ICONERROR);
                ExitProcess(0);
            }
        }
    }

    void c_gui::render() {
        if (should_close) {
            return;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        float deltaTime = ImGui::GetIO().DeltaTime;
        float time = ImGui::GetTime();

        if (window_fade < 1.0f) {
            window_fade += deltaTime * 1.2f;
            if (window_fade > 1.0f) {
                window_fade = 1.0f;
                if (!instances_fading && !is_loading && !injection_done) {
                    instances_fading = true;
                }
            }
        }

        if (instances_fading && !is_loading && !injection_done && window_fade >= 1.0f) {
            instances_fade += deltaTime * 2.0f;
            if (instances_fade > 1.0f) {
                instances_fade = 1.0f;
                instances_fading = false;
                instances_shown = true;
            }
        }

        if (is_loading && !injection_done) {
            loading_fade += deltaTime * 2.5f;
            if (loading_fade > 1.0f) {
                loading_fade = 1.0f;
            }
        }

        if (is_loading && !injection_done) {
            phase_timer += deltaTime;

            if (current_phase == 0) {
                if (!phase_paused) {
                    float speed = 0.5f;
                    load_progress += deltaTime * speed;
                    if (load_progress >= 0.50f) {
                        load_progress = 0.50f;
                        phase_paused = true;
                        pause_timer = 0.0f;
                    }
                }
                else {
                    pause_timer += deltaTime;
                    if (pause_timer >= 0.3f) {
                        phase_paused = false;
                        current_phase = 1;
                    }
                }
            }
            else if (current_phase == 1) {
                if (!injection_executed) {
                    perform_injection();
                    injection_executed = true;
                }
                if (!phase_paused) {
                    float speed = 0.4f;
                    load_progress += deltaTime * speed;
                    if (load_progress >= 0.80f) {
                        load_progress = 0.80f;
                        phase_paused = true;
                        pause_timer = 0.0f;
                    }
                }
                else {
                    pause_timer += deltaTime;
                    if (pause_timer >= 0.3f) {
                        phase_paused = false;
                        current_phase = 2;
                    }
                }
            }
            else if (current_phase == 2) {
                float speed = 0.4f;
                load_progress += deltaTime * speed;
                if (load_progress >= 1.0f) {
                    load_progress = 1.0f;
                    load_complete = true;
                    injection_done = true;
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    ExitProcess(0);
                }
            }
        }

        ImGui::SetNextWindowSize(ImVec2(730, 460), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, window_fade);

        ImGui::Begin("##MainWindow", nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoBringToFrontOnFocus
        );

        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        int bgAlpha = (int)(255 * window_fade);
        drawList->AddRectFilled(
            windowPos,
            ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
            IM_COL32(0, 0, 0, bgAlpha)
        );

        float pulseAlpha = 0.65f + 0.35f * (sinf(time * 1.2f) * 0.5f + 0.5f);
        const ImVec2 center = ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y);
        const float maxRadius = 900.0f;
        const int steps = 150;
        const float decay = 15.5f;

        for (int i = 0; i < steps; ++i) {
            float t = (float)i / (float)(steps - 1);
            float radius = t * maxRadius;
            float alpha = 255.0f * expf(-t * t * decay);
            alpha *= pulseAlpha;
            alpha *= window_fade;
            int a = (int)alpha;
            if (a > 0) {
                ImU32 color = IM_COL32(69, 69, 93, a);
                drawList->AddCircleFilled(center, radius, color, 64);
            }
        }

        float centerX = 365.0f;

        if (logoTexture) {
            float logoDisplaySize = 120.0f;
            float logoY = 45.0f;
            ImGui::SetCursorPosX(centerX - logoDisplaySize / 2);
            ImGui::SetCursorPosY(logoY);
            ImGui::Image(logoTexture, ImVec2(logoDisplaySize, logoDisplaySize));
            ImGui::Dummy(ImVec2(0, 5));
        }

        if (is_loading || injection_done) {
            float alpha = loading_fade;
            if (injection_done) alpha = 1.0f;

            if (alpha > 0.01f) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

                float barWidth = 300.0f;
                float barHeight = 3.0f;
                float barX = centerX - barWidth / 2;
                float barY = (460.0f / 2.0f) - 10.0f;

                ImGui::SetCursorPosX(barX);
                ImGui::SetCursorPosY(barY);

                ImVec2 barPos = ImVec2(windowPos.x + barX, windowPos.y + barY);
                ImVec2 barEnd = ImVec2(barPos.x + barWidth, barPos.y + barHeight);

                const int glowSteps = 30;
                for (int i = glowSteps; i > 0; --i) {
                    float t = (float)i / (float)glowSteps;
                    float glowRadius = t * 15.0f;
                    int glowAlpha = (int)(20 * (1.0f - t * t));
                    ImU32 glowColor = IM_COL32(69, 69, 93, glowAlpha);
                    drawList->AddRect(
                        ImVec2(barPos.x - glowRadius, barPos.y - glowRadius),
                        ImVec2(barEnd.x + glowRadius, barEnd.y + glowRadius),
                        glowColor,
                        1.5f,
                        0,
                        1.0f
                    );
                }

                drawList->AddRectFilled(
                    barPos,
                    barEnd,
                    IM_COL32(30, 30, 40, 255),
                    1.5f
                );

                float progress = injection_done ? 1.0f : load_progress;
                if (progress > 0.0f) {
                    ImVec2 progressEnd = ImVec2(
                        barPos.x + barWidth * progress,
                        barPos.y + barHeight
                    );

                    int r1 = 69, g1 = 69, b1 = 93;
                    int r2 = 255, g2 = 255, b2 = 255;

                    for (float x = barPos.x; x < progressEnd.x; x += 1.0f) {
                        float t2 = (x - barPos.x) / barWidth;
                        int r = (int)(r1 + (r2 - r1) * t2);
                        int g = (int)(g1 + (g2 - g1) * t2);
                        int b = (int)(b1 + (b2 - b1) * t2);
                        ImU32 color = IM_COL32(r, g, b, 255);
                        drawList->AddRectFilled(
                            ImVec2(x, barPos.y),
                            ImVec2(x + 1.0f, barPos.y + barHeight),
                            color,
                            0.0f
                        );
                    }
                }

                std::string statusText;
                if (injection_done) {
                    statusText = "Injection Successful!";
                }
                else if (current_phase == 0) {
                    statusText = "Initializing Swift Client...";
                }
                else if (current_phase == 1) {
                    statusText = "Injecting...";
                }
                else if (current_phase == 2) {
                    statusText = "Finalizing...";
                }

                float statusWidth = ImGui::CalcTextSize(statusText.c_str()).x;
                float textY = barY + barHeight + 15.0f;
                ImGui::SetCursorPosX(centerX - statusWidth / 2);
                ImGui::SetCursorPosY(textY);

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                if (fontSmall) {
                    ImGui::PushFont(fontSmall);
                }
                ImGui::Text("%s", statusText.c_str());
                if (fontSmall) {
                    ImGui::PopFont();
                }
                ImGui::PopStyleColor();

                ImGui::PopStyleVar();
            }
        }

        if (!is_loading && !injection_done && processes.size() >= 2) {
            float alpha = instances_fade;
            if (alpha > 0.01f) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

                float totalHeight = 0.0f;
                totalHeight += processes.size() * 30.0f;
                totalHeight += (processes.size() > 0 ? (processes.size() - 1) * 5.0f : 0.0f);

                float startY = (460.0f - totalHeight) / 2.0f;
                if (startY < 150.0f) startY = 150.0f;

                ImGui::SetCursorPosY(startY);

                for (int i = 0; i < (int)processes.size(); i++) {
                    std::string label = processes[i].window_title;
                    if (processes[i].window_title.find("Minecraft") != std::string::npos) {
                        label += " (javaw.exe)";
                    }

                    bool is_selected = (selected_index == i);

                    ImVec4 bgColor = is_selected ?
                        ImVec4(0.95f, 0.95f, 0.95f, 1.0f) :
                        ImVec4(0.05f, 0.05f, 0.05f, 1.0f);

                    ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bgColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, bgColor);

                    ImGui::PushID(i);

                    ImGui::SetCursorPosX(centerX - 160);
                    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                    ImVec2 buttonSizeProc(320, 28);

                    if (fontMain) {
                        ImGui::PushFont(fontMain);
                    }

                    ImVec4 textColor = is_selected ?
                        ImVec4(0.0f, 0.0f, 0.0f, 1.0f) :
                        ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

                    ImGui::PushStyleColor(ImGuiCol_Text, textColor);

                    if (ImGui::Button(label.c_str(), buttonSizeProc)) {
                        selected_index = i;
                        selected_process = processes[i].window_title;

                        if (check_dll_exists()) {
                            instances_fade = 0.0f;
                            instances_fading = false;
                            is_loading = true;
                            load_timer = 0.0f;
                            load_progress = 0.0f;
                            load_complete = false;
                            loading_fade = 0.0f;
                            phase_timer = 0.0f;
                            current_phase = 0;
                            phase_paused = false;
                            pause_timer = 0.0f;
                            injection_executed = false;
                        }
                        else {
                            MessageBoxA(NULL, "Swift.dll not found in the loader directory!", "Error", MB_ICONERROR);
                            ExitProcess(0);
                        }
                    }

                    ImGui::PopStyleColor();

                    if (fontMain) {
                        ImGui::PopFont();
                    }

                    bool isHovered = ImGui::IsItemHovered();

                    if (isHovered && !processes[i].is_hovered) {
                        processes[i].is_hovered = true;
                    }
                    else if (!isHovered && processes[i].is_hovered) {
                        processes[i].is_hovered = false;
                    }

                    if (processes[i].is_hovered && processes[i].hover_animation < 1.0f) {
                        processes[i].hover_animation += deltaTime * 5.0f;
                        if (processes[i].hover_animation > 1.0f) processes[i].hover_animation = 1.0f;
                    }
                    else if (!processes[i].is_hovered && processes[i].hover_animation > 0.0f) {
                        processes[i].hover_animation -= deltaTime * 5.0f;
                        if (processes[i].hover_animation < 0.0f) processes[i].hover_animation = 0.0f;
                    }

                    float hoverAlpha = processes[i].hover_animation;
                    if (hoverAlpha > 0.01f) {
                        int glowAlpha = (int)(hoverAlpha * 15);
                        ImU32 glowColor = IM_COL32(255, 255, 255, glowAlpha);

                        drawList->AddRect(
                            ImVec2(cursorPos.x - 2, cursorPos.y - 2),
                            ImVec2(cursorPos.x + buttonSizeProc.x + 2, cursorPos.y + buttonSizeProc.y + 2),
                            glowColor,
                            14.0f,
                            ImDrawFlags_RoundCornersAll,
                            2.0f
                        );

                        int borderAlpha = (int)(hoverAlpha * 80);
                        ImU32 borderColor = IM_COL32(255, 255, 255, borderAlpha);

                        drawList->AddRect(
                            ImVec2(cursorPos.x - 1, cursorPos.y - 1),
                            ImVec2(cursorPos.x + buttonSizeProc.x + 1, cursorPos.y + buttonSizeProc.y + 1),
                            borderColor,
                            12.0f,
                            ImDrawFlags_RoundCornersAll,
                            1.0f
                        );
                    }

                    ImGui::PopID();
                    ImGui::PopStyleColor(3);
                    ImGui::Dummy(ImVec2(0, 5));
                }

                ImGui::PopStyleVar();
            }
        }

        if (!is_loading && !injection_done && processes.empty()) {
            float alpha = instances_fade;
            if (alpha > 0.01f) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

                std::string emptyText = "No Minecraft clients found";
                float emptyWidth = ImGui::CalcTextSize(emptyText.c_str()).x;
                float emptyY = (460.0f - 30.0f) / 2.0f;
                ImGui::SetCursorPosY(emptyY);
                ImGui::SetCursorPosX(centerX - emptyWidth / 2);
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), emptyText.c_str());

                ImGui::PopStyleVar();
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::Render();

        ImVec4 clearColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        d3d_device_context->OMSetRenderTargets(1, &render_target_view, nullptr);
        d3d_device_context->ClearRenderTargetView(render_target_view, (float*)&clearColor);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void c_gui::shutdown() {
        if (logoTexture != (ImTextureID)0) {
            ID3D11ShaderResourceView* view = (ID3D11ShaderResourceView*)logoTexture;
            view->Release();
            logoTexture = (ImTextureID)0;
        }

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        cleanup_device_d3d();
    }

    void c_gui::check_window_focus() {
        auto now = std::chrono::steady_clock::now();
        if (!first_check_done) {
            first_check_done = true;
            last_check_time = now;
            return;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_check_time);
        if (elapsed.count() < 100) {
            return;
        }
        last_check_time = now;

        HWND foreground_hwnd = GetForegroundWindow();
        if (!foreground_hwnd) return;

        DWORD foreground_pid = 0;
        GetWindowThreadProcessId(foreground_hwnd, &foreground_pid);
        if (foreground_pid == 0) return;

        if (foreground_pid != last_foreground_pid || force_refresh) {
            last_foreground_pid = foreground_pid;
            force_refresh = false;

            HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, foreground_pid);
            if (process_handle) {
                char process_name[MAX_PATH] = { 0 };
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageNameA(process_handle, 0, process_name, &size)) {
                    std::string fullPath(process_name);
                    std::string fileName = fullPath.substr(fullPath.find_last_of("\\/") + 1);

                    if (_stricmp(fileName.c_str(), "javaw.exe") == 0) {
                        bool found = false;
                        for (const auto& proc : processes) {
                            if (proc.pid == foreground_pid) {
                                found = true;
                                break;
                            }
                        }

                        if (!found) {
                            refresh_processes();
                            std::cout << "[Loader] New Minecraft instance detected: PID " << foreground_pid << std::endl;
                        }
                    }
                }
                CloseHandle(process_handle);
            }
        }
    }

    void c_gui::refresh_single_process(DWORD pid) {
        for (auto& proc : processes) {
            if (proc.pid == pid) {
                EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
                    auto* info = reinterpret_cast<ProcessInfo*>(lParam);
                    DWORD window_pid;
                    GetWindowThreadProcessId(hwnd, &window_pid);
                    if (window_pid == info->pid && IsWindowVisible(hwnd)) {
                        wchar_t title[256];
                        GetWindowTextW(hwnd, title, 256);
                        std::wstring wtitle(title);
                        std::string stitle(wtitle.begin(), wtitle.end());
                        if (!stitle.empty()) {
                            info->window_title = stitle;
                        }
                        return FALSE;
                    }
                    return TRUE;
                    }, reinterpret_cast<LPARAM>(&proc));
                return;
            }
        }
    }

}