const float alpha = 0.33f;
const glm::vec2 g = glm::vec2(0.0f, 0.0f);

// Random sample helper
float U(float a, float b)
{
    return a + (b - a) * rand() / RAND_MAX;
}

struct Vertex {
    glm::vec2 pos;
    glm::vec2 vel;
    bool rewetted = false;
    bool flowing = true;
};

struct Splat {

    std::vector<Vertex> vertices;
    const glm::vec2 pos;
    const glm::vec2 bias;
    const glm::vec3 color;
    const float size, roughness, flow;
    const int stroke_id;
    int life;

    Splat(const Canvas& canvas, const glm::vec2& pos, const glm::vec3& color, float size, float roughness, float flow, int stroke_id, int lifetime, int n_vertices)
        : pos(pos)
        , color(color)
        , bias(glm::vec2(0.0f, 0.0f))
        , size(size)
        , roughness(roughness)
        , flow(flow)
        , stroke_id(stroke_id)
        , life(lifetime)
    {
        vertices.reserve(n_vertices);
        for (int i = 0; i < n_vertices; i++) {
            const float angle = i * 2.0f * glm::pi<float>() / n_vertices;
            vertices.push_back({ canvas.clamp_canvas_point(pos + size * glm::vec2(std::cos(angle), std::sin(angle))),
                glm::vec2(std::cos(angle), std::sin(angle)) });
        }
    }

    // Advect each vertex and update the lifetime of the splat
    void advect(const Canvas& canvas, float* wet_map)
    {
        // d = (1 - alpha) * b + alpha * (1 / U(1, 1 + r)) * v
        // x* = x_t + f * d + g + U(-r, r)
        // x_t+1 = x* if w(x*) > 0 else x_t
        // Where U(a, b) is a uniform random variable between a and b
        for (auto it = vertices.begin(); it != vertices.end(); it++) {

            if (it->rewetted) { // Rewetted vertices have their velocity sampled from the wet map
                it->vel = glm::vec2(
                    wet_map[4 * ((int)canvas.size.x * (int)it->pos.y + (int)it->pos.x) + 0],
                    wet_map[4 * ((int)canvas.size.x * (int)it->pos.y + (int)it->pos.x) + 1]);
                if (!it->flowing && wet_map[4 * ((int)canvas.size.x * (int)it->pos.y + (int)it->pos.x) + 3] == 1.0f)
                    it->flowing = true;
            }

            if (it->flowing) {
                const glm::vec2 d = (1.0f - alpha) * bias + alpha * (1.0f / U(1.0f, 1.0f + roughness)) * it->vel;
                const glm::vec2 x_star = canvas.clamp_canvas_point(it->pos + flow * d + g + glm::vec2(U(-roughness, roughness), U(-roughness, roughness)));
                const glm::ivec2 x_star_i = glm::ivec2(x_star);
                if (wet_map[4 * ((int)canvas.size.x * x_star_i.y + x_star_i.x) + 3] > 0.0f)
                    it->pos = x_star;
            }
        }

        life--;
    }

    // If the splat has just been rewetted, reset its lifetime, otherwise age it
    void age(const Canvas& canvas, float* wet_map, int new_lifetime, float unfixing_strength = 0.75f)
    {
        for (auto it = vertices.begin(); it != vertices.end(); it++)
            if (wet_map[4 * ((int)canvas.size.x * (int)it->pos.y + (int)it->pos.x) + 3] == 1.0f) {
                life = new_lifetime - 1;
                for (auto it = vertices.begin(); it != vertices.end(); it++) {
                    it->vel = glm::vec2(0.0f, 0.0f);
                    it->rewetted = U(0.0f, 1.0f) < std::powf(unfixing_strength, -life);
                    it->flowing = wet_map[4 * ((int)canvas.size.x * (int)it->pos.y + (int)it->pos.x) + 3] == 1.0f;
                }
                return;
            }

        life--;
    }
};