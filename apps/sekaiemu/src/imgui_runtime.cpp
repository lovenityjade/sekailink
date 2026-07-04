#include "imgui_runtime.hpp"

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>

#include <array>
#include <filesystem>

namespace sekaiemu::spike {
namespace {

SekaiemuImGuiRuntime* g_active_runtime = nullptr;

bool IsGameControllerEvent(const SDL_Event& event) {
  switch (event.type) {
    case SDL_CONTROLLERDEVICEADDED:
    case SDL_CONTROLLERDEVICEREMOVED:
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
    case SDL_CONTROLLERAXISMOTION:
    case SDL_JOYDEVICEADDED:
    case SDL_JOYDEVICEREMOVED:
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
    case SDL_JOYAXISMOTION:
    case SDL_JOYHATMOTION:
      return true;
    default:
      return false;
  }
}

void LoadSekaiemuFonts() {
  ImGuiIO& io = ImGui::GetIO();
  const std::array<const char*, 8> candidates{{
      "apps/client-core/public/assets/fonts/Inter-SemiBold.ttf",
      "apps/client-core/dist/assets/fonts/Inter-SemiBold.ttf",
      "apps/client-core/public/assets/fonts/Inter-Medium.ttf",
      "third_party/imgui/misc/fonts/Roboto-Medium.ttf",
      "runtime/poptracker/assets/DejaVuSans-Bold.ttf",
      "<local-home>/SekaiLink/canonical/apps/client-core/public/assets/fonts/Inter-SemiBold.ttf",
      "<local-home>/.fonts/Roboto-Medium.ttf",
      "/usr/share/fonts/google-noto/NotoSans-SemiBold.ttf",
  }};

  for (const char* path : candidates) {
    if (path != nullptr && std::filesystem::exists(path)) {
      io.Fonts->AddFontFromFileTTF(path, 17.0f);
      return;
    }
  }
  io.FontGlobalScale = 1.02f;
}

}  // namespace

SekaiemuImGuiRuntime::~SekaiemuImGuiRuntime() {
  Shutdown();
}

void ApplySekaiemuImGuiStyle() {
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 10.0f;
  style.ChildRounding = 10.0f;
  style.FrameRounding = 8.0f;
  style.PopupRounding = 8.0f;
  style.ScrollbarRounding = 8.0f;
  style.GrabRounding = 6.0f;
  style.WindowBorderSize = 1.0f;
  style.ChildBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.Alpha = 0.98f;
  style.WindowPadding = ImVec2(15.0f, 15.0f);
  style.FramePadding = ImVec2(8.0f, 5.0f);
  style.ItemSpacing = ImVec2(11.0f, 11.0f);
  style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
  style.WindowTitleAlign = ImVec2(0.02f, 0.50f);

  ImVec4* colors = style.Colors;
  colors[ImGuiCol_Text] = ImVec4(0.937f, 0.992f, 1.000f, 1.000f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.596f, 0.776f, 0.824f, 1.000f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.027f, 0.082f, 0.118f, 1.000f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.071f, 0.149f, 0.204f, 1.000f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.094f, 0.204f, 0.271f, 1.000f);
  colors[ImGuiCol_Border] = ImVec4(0.184f, 0.384f, 0.467f, 1.000f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.071f, 0.149f, 0.204f, 1.000f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.129f, 0.282f, 0.357f, 1.000f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.055f, 0.455f, 0.565f, 1.000f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.027f, 0.082f, 0.118f, 1.000f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.047f, 0.145f, 0.200f, 1.000f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.133f, 0.827f, 0.933f, 1.000f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.133f, 0.827f, 0.933f, 0.950f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.210f, 0.900f, 0.980f, 1.000f);
  colors[ImGuiCol_Button] = ImVec4(0.333f, 0.388f, 0.400f, 0.960f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.133f, 0.827f, 0.933f, 1.000f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.055f, 0.455f, 0.565f, 1.000f);
  colors[ImGuiCol_Header] = ImVec4(0.129f, 0.282f, 0.357f, 0.900f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.133f, 0.827f, 0.933f, 0.550f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.133f, 0.827f, 0.933f, 0.900f);
  colors[ImGuiCol_Separator] = ImVec4(0.184f, 0.384f, 0.467f, 0.900f);
  colors[ImGuiCol_Tab] = ImVec4(0.027f, 0.082f, 0.118f, 1.000f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.133f, 0.827f, 0.933f, 0.520f);
  colors[ImGuiCol_TabActive] = ImVec4(0.071f, 0.541f, 0.640f, 1.000f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.027f, 0.082f, 0.118f, 0.880f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.184f, 0.384f, 0.467f, 0.900f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.133f, 0.827f, 0.933f, 0.600f);
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
  LoadSekaiemuFonts();
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
  if (active_ && !IsGameControllerEvent(event)) {
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
