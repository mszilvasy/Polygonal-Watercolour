struct Splat {
    const glm::vec2 pos;
    const glm::vec3 color;
    const int size, stroke_id;
    int age;
    Splat(const glm::vec2& pos, const glm::vec3& color, int size, int stroke_id)
        : pos(pos)
        , color(color)
        , size(size)
        , stroke_id(stroke_id)
        , age(0)
    {
    }
};