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
    void advect(const Canvas& canvas, unsigned char* wet_map)
    {
        // d = (1 - alpha) * b + alpha * (1 / U(1, 1 + r)) * v
        // x* = x_t + f * d + g + U(-r, r)
        // x_t+1 = x* if w(x*) > 0 else x_t
        // Where U(a, b) is a uniform random variable between a and b
        for (auto it = vertices.begin(); it != vertices.end(); it++) {
            const glm::vec2 d = (1.0f - alpha) * bias + alpha * (1.0f / U(1.0f, 1.0f + roughness)) * it->vel;
            const glm::vec2 x_star = canvas.clamp_canvas_point(it->pos + flow * d + g + glm::vec2(U(-roughness, roughness), U(-roughness, roughness)));
            const glm::ivec2 x_star_i = glm::ivec2(x_star);
            if (wet_map[3 * ((int)canvas.size.x * x_star_i.y + x_star_i.x)] > 0)
                it->pos = x_star;
        }

        life--;
    }
};