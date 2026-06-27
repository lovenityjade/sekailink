#include "imgui_runtime.hpp"

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>

namespace sekaiemu::spike {
namespace {

SekaiemuImGuiRuntime* g_active_runtime = nullptr;

}  // namespace

SekaiemuImGuiRuntime::~SekaiemuImGuiRuntime() {
  Shutdown();
}

void ApplySekaiemuImGuiStyle() {
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 8.0f;
  style.ChildRounding = 6.0f;
  style.FrameRounding = 6.0f;
  style.PopupRounding = 6.0f;
  style.ScrollbarRounding = 8.0f;
  style.GrabRounding = 6.0f;
  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.WindowPadding = ImVec2(14.0f, 12.0f);
  style.FramePadding = ImVec2(10.0f, 7.0f);
  style.ItemSpacing = ImVec2(10.0f, 8.0f);
  style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);

  ImVec4* colors = style.Colors;
  colors[ImGuiCol_Text] = ImVec4(0.93f, 0.97f, 0.98f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.48f, 0.55f, 0.58f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.03f, 0.05f, 0.08f, 0.96f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.06f, 0.08f, 0.11f, 0.92f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.04f, 0.06f, 0.09f, 0.98f);
  colors[ImGuiCol_Border] = ImVec4(0.18f, 0.30f, 0.33f, 0.78f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.15f, 0.18f, 0.90f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.34f, 0.35f, 0.90f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.80f, 0.74f, 0.88f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.06f, 0.09f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.13f, 0.17f, 1.00f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.31f, 0.80f, 0.77f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.31f, 0.80f, 0.77f, 0.90f);
  colors[ImGuiCol_Button] = ImVec4(0.13f, 0.20f, 0.24f, 0.94f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.92f, 0.38f, 0.22f, 0.90f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.31f, 0.80f, 0.77f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.13f, 0.25f, 0.28f, 0.85f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.48f, 0.47f, 0.88f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.31f, 0.80f, 0.77f, 0.95f);
  colors[ImGuiCol_Tab] = ImVec4(0.09f, 0.13f, 0.16f, 1.00f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.92f, 0.38f, 0.22f, 0.92f);
  colors[ImGuiCol_TabActive] = ImVec4(0.14f, 0.31f, 0.33f, 1.00f);
}

bool SekaiemuImGuiRuntime::Initialize(SDL_Window* window, SDL_GLContext gl_context, std::string& error) {
  if (active_) {
    return true;
  }
  if (window == nullptr || gl_context == nullptr) {
    error = "ImGui requires an active SDL/OpenGL window.";
    return false;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  ApplySekaiemuImGuiStyle();

  if (!ImGui_ImplSDL2_InitForOpenGL(window, gl_context)) {
    error = "ImGui SDL2 backend initialization failed.";
    ImGui::DestroyContext();
    return false;
  }
  if (!ImGui_ImplOpenGL3_Init("#version 130")) {
    error = "ImGui OpenGL3 backend initialization failed.";
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    return false;
  }
  active_ = true;
  g_active_runtime = this;
  return true;
}

void SekaiemuImGuiRuntime::ProcessEvent(const SDL_Event& event) {
  if (active_) {
    ImGui_ImplSDL2_ProcessEvent(&event);
  }
}

void SekaiemuImGuiRuntime::Render(const std::function<void()>& draw_callback) {
  if (!active_) {
    return;
  }
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
  if (draw_callback) {
    draw_callback();
  }
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void SekaiemuImGuiRuntime::Shutdown() {
  if (!active_) {
    return;
  }
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  if (g_active_runtime == this) {
    g_active_runtime = nullptr;
  }
  active_ = false;
}

void ProcessSekaiemuImGuiEvent(const SDL_Event& event) {
  if (g_active_runtime != nullptr) {
    g_active_runtime->ProcessEvent(event);
  }
}

}  // namespace sekaiemu::spike
