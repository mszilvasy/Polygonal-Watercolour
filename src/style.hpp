// Style setup for ImGui
void darkStyle()
{
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScrollbarRounding = 8.0f;
    style.GrabMinSize = 8.0f;
    style.GrabRounding = 8.0f;

    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.9f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
}

// Helper to display a slider as a percentage
void SliderPercent(const char* label, float* v, float v_min, float v_max)
{
    float percent = *v * 100.0f;
    ImGui::SliderFloat(label, &percent, 100.0f * v_min, 100.0f * v_max, "%.0f%%");
    *v = percent / 100.0f;
}