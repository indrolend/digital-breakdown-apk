#pragma once

#include <string>
#include <vector>

struct StaticModelBatch {
    std::size_t start = 0;
    std::size_t count = 0;
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct StaticModelData {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<StaticModelBatch> batches;

    bool load(const std::string&) { return false; }
    bool valid() const { return !vertices.empty() && !batches.empty(); }
};
