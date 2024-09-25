struct Splat {
    const glm::ivec2 pos;
    const int size, stroke_id;
    int age;
    Splat(const glm::ivec2& pos, int size, int stroke_id)
        : pos(pos)
        , size(size)
        , stroke_id(stroke_id)
        , age(0)
    {
    }
};