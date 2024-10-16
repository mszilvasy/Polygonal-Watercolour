#include <deque>
#include <framework/window.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <nativefiledialog/nfd.h>
#include <stb/stb_image_write.h>

#include "canvas.hpp"
#include "splat.hpp"
#include "style.hpp"

const glm::ivec2 workspace_offset { 300, 0 };

const std::vector<float> zoom_steps = { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f };

enum class DebugMode {
    Fill,
    Points,
    Wetness
};

int main()
{
    glm::ivec2 win_size { 1300, 1000 };
    glm::ivec2 workspace_size { win_size.x - 300, win_size.y }; // The area where the canvas sits (leaving the GUI out)
    Window window { "Watercolour Painting", win_size, OpenGLVersion::GL2, true };

    const glm::ivec2 canvas_size { 900, 600 };
    const glm::ivec2 canvas_pos { (workspace_size - canvas_size) / 2 + workspace_offset };
    Canvas canvas { canvas_pos, canvas_size };
    int zoom_idx = 3;

    glm::vec3 brush_color = { 1.0f, 0.0f, 0.0f };
    int brush_size = 10;
    float roughness = 1;
    float flow = 1.0f;
    int lifetime = 60;
    int vertices = 25;
    
    float unfixing_strength = 0.75f;
    int stamp_spacing = 5.0f;
    int drying_time = 600;

    bool ctrl = false;
    bool debug = false;
    bool show_wetness = true;
    DebugMode debug_mode = DebugMode::Fill;

    bool show_new_canvas_window = false;
    bool show_save_canvas_window = false;

    glm::vec2 cursor_pos;
    glm::vec2 last_stamp;
    bool stroke = false;
    bool wetting = false;
    bool pan = false;
    int stroke_id = 0;

    std::list<Splat> live_splats = {};
    std::deque<Splat> undone_splats = {};

    float time_step = 0.0167f; // Roughly corresponds to 60 fps
    float time_accum = 0.0f;
    int resample_period = 1;
    int resample_counter = 0;

    glEnable(GL_BLEND);
    glStencilFunc(GL_EQUAL, 1, 1);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(5.0f);

    GLuint bg_fbo, bg;
    glGenFramebuffers(1, &bg_fbo);

    GLuint wet_map_fbo, wet_map;
    glGenFramebuffers(1, &wet_map_fbo);

    const auto generate_canvas = [&](const glm::vec3& bg_color) {
        // Canvas texture
        glBindFramebuffer(GL_FRAMEBUFFER, bg_fbo);

        glGenTextures(1, &bg);
        glBindTexture(GL_TEXTURE_2D, bg);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, canvas.size.x, canvas.size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLuint bg_stencil;
        glGenRenderbuffers(1, &bg_stencil);
        glBindRenderbuffer(GL_RENDERBUFFER, bg_stencil);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, canvas.size.x, canvas.size.y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, bg_stencil);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bg, 0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        glClearColor(bg_color.r, bg_color.g, bg_color.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Wet map
        glBindFramebuffer(GL_FRAMEBUFFER, wet_map_fbo);

        glGenTextures(1, &wet_map);
        glBindTexture(GL_TEXTURE_2D, wet_map);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, canvas.size.x, canvas.size.y, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, wet_map, 0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.27f, 0.27f, 0.27f, 1.0f);
    };

    generate_canvas(glm::vec3(0.9f, 0.9f, 0.9f));

    // Actions
    const auto new_canvas = [&](const glm::ivec2& new_size, const glm::vec3& bg_color) {
        live_splats.clear();
        undone_splats.clear();
        zoom_idx = 3;

        canvas = Canvas((workspace_size - new_size) / 2 + workspace_offset, new_size);
        generate_canvas(bg_color);
    };

    const auto save_canvas = [&]() {
        nfdchar_t* p_out_path = nullptr;
        nfdresult_t result = NFD_SaveDialog("png", nullptr, &p_out_path);
        if (result == NFD_OKAY) {

            std::filesystem::path out_path { p_out_path };
            out_path.replace_extension(".png");

            glBindFramebuffer(GL_FRAMEBUFFER, bg_fbo);
            glViewport(0, 0, canvas.size.x, canvas.size.y);
            auto bg_data = new unsigned char[3 * canvas.size.x * canvas.size.y];
            for (int i = 0; i < canvas.size.y; i++) // The image data must be flipped vertically
                glReadPixels(0, canvas.size.y - i - 1, canvas.size.x, 1, GL_RGB, GL_UNSIGNED_BYTE, bg_data + 3 * i * (int)canvas.size.x);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            stbi_write_png(out_path.string().c_str(), canvas.size.x, canvas.size.y, 3, bg_data, 3 * canvas.size.x);
        }
        free(p_out_path);
    };

    const auto undo = [&]() {
        if (live_splats.size() > 0) {
            const int last_stroke_id = live_splats.back().stroke_id;
            while (live_splats.size() > 0 && live_splats.back().stroke_id == last_stroke_id) {
                undone_splats.push_back(live_splats.back());
                live_splats.pop_back();
            }
        }
    };

    const auto redo = [&]() {
        if (undone_splats.size() > 0) {
            const int last_stroke_id = undone_splats.back().stroke_id;
            while (undone_splats.size() > 0 && undone_splats.back().stroke_id == last_stroke_id) {
                live_splats.push_back(undone_splats.back());
                undone_splats.pop_back();
            }
        }
    };

    const auto zoom = [&](const bool zoom_in, const glm::vec2& center) {
        zoom_idx += zoom_in ? 1 : -1;
        const float scale = zoom_steps[zoom_idx] / canvas.zoom;
        canvas.zoom = zoom_steps[zoom_idx];
        canvas.pos = (canvas.pos - center) * scale + center;
    };

    const auto add_water = [&](const glm::vec2& pos) {
        glBegin(GL_TRIANGLE_FAN);
        for (int i = 0; i < vertices; i++) {
            const float angle = i * 2.0f * glm::pi<float>() / vertices;
            const glm::vec2 dir = glm::vec2(std::cos(angle), std::sin(angle));
            const glm::vec2 point = pos + (float)brush_size * dir;
            const glm::vec2 point_proj = canvas.proj * glm::vec4(point, 0.0f, 1.0f);
            glColor4f(dir.x, dir.y, 0.0f, 1.0f);
            glVertex2f(point_proj.x, point_proj.y);
        }
        glEnd();
    };

    // Callbacks
    window.registerKeyCallback([&](const int key, const int scancode, const int action, const int mods) {
        // Hold Ctrl
        if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL) {
            if (action == GLFW_PRESS)
                ctrl = true;
            else if (action == GLFW_RELEASE)
                ctrl = false;
        }

        // Ctrl+N: New canvas
        if (key == GLFW_KEY_N && ctrl && action == GLFW_PRESS)
            show_new_canvas_window = true;

        // Ctrl+S: Save canvas
        if (key == GLFW_KEY_S && ctrl && action == GLFW_PRESS)
            show_save_canvas_window = true;

        // Ctrl+Z: Undo
        if (key == GLFW_KEY_Z && ctrl && action == GLFW_PRESS)
            undo();

        // Ctrl+Y: Redo
        if (key == GLFW_KEY_Y && ctrl && action == GLFW_PRESS)
            redo();

        // Toggle debug info
        if (key == GLFW_KEY_D && action == GLFW_PRESS)
            debug = !debug;
    });

    window.registerMouseButtonCallback([&](const int button, const int action, const int mods) {
        // Left mouse button draws strokes
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                stroke = true;
                wetting = false;

                // Place the first stamp
                last_stamp = canvas.canvas_coords(cursor_pos);
                if (canvas.contains_canvas_point(last_stamp))
                    live_splats.push_back(Splat(canvas, last_stamp, brush_color, brush_size, roughness, flow, stroke_id, lifetime, vertices)); // TODO: use stamps instead of splats
                undone_splats.clear();

                // Bind wet map
                glBindFramebuffer(GL_FRAMEBUFFER, wet_map_fbo);
                glViewport(0, 0, canvas.size.x, canvas.size.y);

                // Update wet map
                add_water(last_stamp);

                // Unbind wet map
                glBindFramebuffer(GL_FRAMEBUFFER, 0);

            } else if (action == GLFW_RELEASE) {
                stroke = false;
                stroke_id++;
            }
        }

        // Right mouse button adds water (without adding stamps)
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                wetting = true;
                stroke = false;

                last_stamp = canvas.canvas_coords(cursor_pos);

                // Bind wet map
                glBindFramebuffer(GL_FRAMEBUFFER, wet_map_fbo);
                glViewport(0, 0, canvas.size.x, canvas.size.y);

                // Update wet map
                add_water(last_stamp);

                // Unbind wet map
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            } else if (action == GLFW_RELEASE) {
                wetting = false;
            }
        }

        // Middle mouse button pans the canvas
        if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
            if (action == GLFW_PRESS)
                pan = true;
            else if (action == GLFW_RELEASE)
                pan = false;
        }
    });

    window.registerMouseMoveCallback([&](const glm::vec2& new_pos) {
        if (stroke || wetting) {

            const glm::vec2 cur_pos = canvas.canvas_coords(new_pos);
            const float dist = glm::distance(last_stamp, cur_pos);

            // Iterate along the stroke, updating the wet map and placing stamps
            if (dist >= stamp_spacing) {

                // Bind wet map
                glBindFramebuffer(GL_FRAMEBUFFER, wet_map_fbo);
                glViewport(0, 0, canvas.size.x, canvas.size.y);

                const glm::vec2 dir = glm::normalize(glm::vec2(cur_pos - last_stamp));
                glm::vec2 pos = last_stamp;

                for (float i = 1.0f; i <= dist; i += 1.0f) {

                    pos += dir;

                    // Update wet map
                    add_water(pos);

                    // Place stamp
                    if (std::fmod(i, stamp_spacing) == 0.0f) {
                        last_stamp = pos;
                        if (stroke && canvas.contains_canvas_point(last_stamp))
                            live_splats.push_back(Splat(canvas, last_stamp, brush_color, brush_size, roughness, flow, stroke_id, lifetime, vertices)); // TODO: use stamps instead of splats
                    }
                }

                // Unbind wet map
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
        }

        if (pan)
            // Pan the canvas
            canvas.pos -= cursor_pos - new_pos;

        cursor_pos = new_pos;
    });

    window.registerWindowResizeCallback([&](const glm::ivec2& size) {
        // Resizing window and its contents
        win_size = size;
        workspace_size = win_size - workspace_offset;

        // Recenter the canvas
        canvas.pos = (glm::vec2(workspace_size) - canvas.zoom * canvas.size) / 2.0f + glm::vec2(workspace_offset);
    });

    window.registerScrollCallback([&](const glm::vec2& offset) {
        // Increment zoom
        bool zoom_in = offset.y > 0;
        if (zoom_in && zoom_idx < zoom_steps.size() - 1 || !zoom_in && zoom_idx > 0)
            zoom(zoom_in, cursor_pos);
    });

    // ImGui setup
    darkStyle();

    auto t = std::chrono::system_clock::now();
    float fps = 0.0f;

    // Main loop
    while (!window.shouldClose()) {

        // Calculate time delta
        const auto new_t = std::chrono::system_clock::now();
        const float dt = std::chrono::duration<float>(new_t - t).count();
        time_accum += dt;
        t = new_t;

        // Time step
        while (time_accum >= time_step) {
            time_accum -= time_step;
            fps = 1.0f / dt;

            glBindFramebuffer(GL_FRAMEBUFFER, wet_map_fbo);
            glViewport(0, 0, canvas.size.x, canvas.size.y);

            static auto wet_map_data = new float[4 * canvas.size.x * canvas.size.y];
            glReadPixels(0, 0, canvas.size.x, canvas.size.y, GL_RGBA, GL_FLOAT, wet_map_data);
            for (auto it = live_splats.begin(); it != live_splats.end(); it++) {
                if (it->life >= 0) {
                    // Advect flowing splats
                    if ((it->advect(canvas, wet_map_data) || resample_counter == resample_period) && resample_period > 0)
                        it->resample(); // Resample boundary periodically or when a splat becomes fixed
                } else if (it->life >= -drying_time)
                    // Age fixed splats
                    it->age(canvas, wet_map_data, lifetime, unfixing_strength);
            }

            if (resample_period > 0)
                resample_counter = resample_counter % resample_period + 1;

            // Reduce wetness
            glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
            glBlendFunc(GL_ONE, GL_ONE);

            glColor4f(0.0f, 0.0f, 0.0f, 0.005f);
            glBegin(GL_QUADS);
            glVertex2f(-1.0f, -1.0f);
            glVertex2f(1.0f, -1.0f);
            glVertex2f(1.0f, 1.0f);
            glVertex2f(-1.0f, 1.0f);
            glEnd();

            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        window.updateInput();

        // GUI
        {
            // Main menu
            ImGui::BeginMainMenuBar();
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New", "Ctrl+N", nullptr))
                    show_new_canvas_window = true;
                if (ImGui::MenuItem("Save", "Ctrl+S", nullptr))
                    show_save_canvas_window = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", nullptr, live_splats.size() > 0))
                    undo();
                if (ImGui::MenuItem("Redo", "Ctrl+Y", nullptr, undone_splats.size() > 0))
                    redo();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Centre canvas", nullptr, nullptr))
                    canvas.pos = (glm::vec2(workspace_size) - canvas.zoom * canvas.size) / 2.0f + glm::vec2(workspace_offset);
                if (ImGui::MenuItem("Zoom in", nullptr, nullptr, zoom_idx < zoom_steps.size() - 1))
                    zoom(true, workspace_size / 2 + workspace_offset);
                if (ImGui::MenuItem("Zoom out", nullptr, nullptr, zoom_idx > 0))
                    zoom(false, workspace_size / 2 + workspace_offset);
                ImGui::Separator();
                ImGui::MenuItem("Show wetness", nullptr, &show_wetness);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Darkens the canvas where there is water present");
                ImGui::MenuItem("Debug", "D", &debug);
                ImGui::EndMenu();
            }

            // Print canvas dimensions on the right-hand side of the menu bar
            const std::string canvas_size_str = std::to_string((int)(100 * canvas.zoom)) + "%%   " + std::to_string((int)canvas.size.x) + "x" + std::to_string((int)canvas.size.y);
            const char* canvas_size_cstr = canvas_size_str.c_str();
            const int canvas_size_str_width = ImGui::CalcTextSize(canvas_size_cstr).x;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - canvas_size_str_width - 8);
            ImGui::Text(canvas_size_cstr);

            const int main_menu_height = ImGui::GetWindowHeight();
            ImGui::EndMainMenuBar();

            // Settings
            ImGui::SetNextWindowPos(ImVec2(-1, main_menu_height - 1), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(300, window.getWindowSize().y - main_menu_height + 2), ImGuiCond_Always);
            ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
            {
                // Brush settings
                ImGui::Text("Brush");
                ImGui::SliderInt("Size", &brush_size, 1, 50);
                ImGui::SliderFloat("Roughness", &roughness, 0.0f, 2.0f);
                SliderPercent("Flow", &flow, 0.0f, 2.0f);
                ImGui::SliderInt("Lifetime", &lifetime, 0, 300);
                ImGui::SliderInt("Vertices", &vertices, 6, 50);
                ImGui::Separator();
                ImGui::ColorPicker3("Colour", &brush_color.r);

                // Simulation settings
                ImGui::Separator();
                ImGui::Text("Canvas");
                ImGui::SliderInt("Stamp spacing", &stamp_spacing, 1, 10);
                ImGui::SliderInt("Drying time", &drying_time, 0, 3600);
                ImGui::SliderInt("Resampling period", &resample_period, 0, 60);
                ImGui::SliderFloat("Unfixing strength", &unfixing_strength, 0.0f, 1.0f);

                // Debug info
                if (debug) {
                    ImGui::Separator();
                    ImGui::Text("Debug (%d fps)", (int)fps);
                    ImGui::RadioButton("Fill", (int*)&debug_mode, (int)DebugMode::Fill);
                    ImGui::SameLine();
                    ImGui::RadioButton("Points", (int*)&debug_mode, (int)DebugMode::Points);
                    ImGui::SameLine();
                    ImGui::RadioButton("Wet map", (int*)&debug_mode, (int)DebugMode::Wetness);
                    ImGui::Text("Strokes: %d", stroke_id);
                    ImGui::Text("Live splats: %d", live_splats.size());
                    ImGui::Text("Last stamp: (%f, %f)", last_stamp.x, last_stamp.y);
                }
            }
            ImGui::End();

            // New canvas window
            if (show_new_canvas_window) {
                ImGui::SetNextWindowPos(ImVec2(workspace_size.x / 2 + workspace_offset.x, workspace_size.y / 2), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(175, 0), ImGuiCond_Appearing);
                ImGui::Begin("New canvas", &show_new_canvas_window, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
                {
                    static int width = canvas.size.x;
                    static int height = canvas.size.y;
                    static glm::vec3 bg_color = { 0.9f, 0.9f, 0.9f };

                    ImGui::InputInt("Width", &width);
                    ImGui::InputInt("Height", &height);
                    ImGui::ColorEdit3("", &bg_color.r);

                    width = std::clamp(width, 1, 4000);
                    height = std::clamp(height, 1, 4000);

                    if (ImGui::Button("OK")) {
                        new_canvas(glm::ivec2(width, height), bg_color);
                        show_new_canvas_window = false;
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Cancel"))
                        show_new_canvas_window = false;
                }
                ImGui::End();
            }

            // Save canvas window
            if (show_save_canvas_window) {
                if (live_splats.size() == 0) {
                    show_save_canvas_window = false;
                    save_canvas();
                } else {
                    ImGui::SetNextWindowPos(ImVec2(workspace_size.x / 2 + workspace_offset.x, workspace_size.y / 2), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_Appearing);
                    ImGui::Begin("Save canvas", &show_save_canvas_window, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
                    {
                        ImGui::Text("Waiting for paint to dry...");
                        ImGui::Text("Remaining splats: %d", live_splats.size());

                        if (ImGui::Button("Cancel"))
                            show_save_canvas_window = false;
                    }
                    ImGui::End();
                }
            }
        }

        // Drawing logic
        glm::mat4 proj;

        const auto draw_splat = [&](const Splat& splat, bool draw_to_window = true) {
            if (draw_to_window && debug) {
                // Debug vertices
                glColor4f(splat.color.r, splat.color.g, splat.color.b, 0.1f);
                glBegin(GL_TRIANGLE_FAN);

                for (int i = 0; i <= splat.vertices.size(); i++) {
                    const int j = i % splat.vertices.size();
                    const glm::vec2 point = canvas.window_coords(splat.vertices[j].pos);
                    const glm::vec4 point_proj = proj * glm::vec4(point, 0.0f, 1.0f);
                    glVertex2f(point_proj.x, point_proj.y);
                }

                glEnd();

            } else {
                // Two pass stencil buffer approach
                glEnable(GL_STENCIL_TEST);
                float x_min = draw_to_window ? win_size.x : canvas.size.x, x_max = 0, y_min = draw_to_window ? win_size.y : canvas.size.y, y_max = 0; // Bounding box

                // First pass: mask out color buffer, draw splat to stencil buffer
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                glStencilOp(GL_INVERT, GL_INVERT, GL_INVERT);

                glBegin(GL_TRIANGLE_FAN);

                for (int i = 0; i <= splat.vertices.size(); i++) {
                    const int j = i % splat.vertices.size();
                    const glm::vec2 point = draw_to_window ? canvas.window_coords(splat.vertices[j].pos) : splat.vertices[j].pos;
                    const glm::vec4 point_proj = proj * glm::vec4(point, 0.0f, 1.0f);
                    glVertex2f(point_proj.x, point_proj.y);

                    // Update bounding box
                    x_min = std::min(point_proj.x, x_min);
                    x_max = std::max(point_proj.x, x_max);
                    y_min = std::min(point_proj.y, y_min);
                    y_max = std::max(point_proj.y, y_max);
                }

                glEnd();

                // Second pass: draw quad to color buffer, clear stencil buffer
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                glColor4f(splat.color.r, splat.color.g, splat.color.b, 0.1f);
                glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);

                // Draw quad around bounding box
                glBegin(GL_QUADS);
                glVertex2f(x_min, y_min);
                glVertex2f(x_max, y_min);
                glVertex2f(x_max, y_max);
                glVertex2f(x_min, y_max);
                glEnd();

                glDisable(GL_STENCIL_TEST);
            }
        };

        // Draw dried splats to the canvas texture
        while (live_splats.size() > 0 && live_splats.front().life < -drying_time) {
            glBindFramebuffer(GL_FRAMEBUFFER, bg_fbo);
            glViewport(0, 0, canvas.size.x, canvas.size.y);
            proj = canvas.proj;
            while (live_splats.size() > 0 && live_splats.front().life < -drying_time) {
                draw_splat(live_splats.front(), false);
                live_splats.pop_front();
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // Draw canvas
        glViewport(0, 0, win_size.x, win_size.y);
        glClear(GL_COLOR_BUFFER_BIT);
        proj = glm::ortho(0.0f, (float)win_size.x, 0.0f, (float)win_size.y, -1.0f, 1.0f);

        canvas.draw_backdrop(proj);
        if (!debug || debug_mode != DebugMode::Wetness) {
            // Draw the canvas texture
            canvas.draw_texture(proj, bg);

            // Draw "live" splats to the canvas
            if (debug && debug_mode == DebugMode::Points)
                glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
            std::for_each(live_splats.begin(), live_splats.end(), draw_splat);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            // Darkening effect of the wet map
            if (show_wetness) {
                glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
                canvas.draw_texture(proj, wet_map, 0.05f);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }

        } else
            canvas.draw_texture(proj, wet_map);

        // Draw brush
        if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && canvas.contains_point(cursor_pos)) {
            // Hide cursor
            window.setMouseCapture(true);

            // Draw a circle
            glColor4f(0.0f, 0.0f, 0.0f, stroke ? 0.8f : 0.6f);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < vertices; i++) {
                const float angle = i * 2.0f * glm::pi<float>() / vertices;
                const glm::vec2 point = canvas.clamp_point(glm::vec2(cos(angle) * canvas.zoom * brush_size, sin(angle) * canvas.zoom * brush_size) + cursor_pos);
                const glm::vec4 point_proj = proj * glm::vec4(point, 0.0f, 1.0f);
                glVertex2f(point_proj.x, point_proj.y);
            }
            glEnd();

        } else
            window.setMouseCapture(false);

        window.swapBuffers();
    }

    return 0;
}