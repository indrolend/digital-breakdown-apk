#pragma once

#include <string>
#include <vector>

struct HumanModelData {
    std::vector<float> vertices;
    std::vector<int> bones;
    unsigned int frameCount = 0;
    float color[4] = {0.14f, 1.0f, 0.32f, 1.0f};

    bool load(const std::string&) { return false; }
    bool valid() const { return false; }
    void skin(float, float, int, std::vector<float>& out) const { out.clear(); }
};
