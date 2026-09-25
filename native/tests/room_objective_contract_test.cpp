#include "gameplay/RoomObjective.hpp"
#include "gameplay/RunPressure.hpp"
#include <cassert>
#include <cstring>

int main() {
    auto objective = gameplay::makeHarvestObjective(5);
    assert(objective.type == gameplay::RoomObjectiveType::Harvest);
    assert(!gameplay::updateHarvestObjective(objective, 4, 1.0f));
    assert(objective.progress == 4 && !objective.complete);
    assert(gameplay::updateHarvestObjective(objective, 5, 0.1f));
    assert(objective.complete && objective.progress == 5);
    assert(std::strcmp(gameplay::roomObjectiveName(gameplay::RoomObjectiveType::Escape), "ESCAPE") == 0);

    const auto early = gameplay::runPressureForRoom(1, 0, 0);
    const auto late = gameplay::runPressureForRoom(25, 3, 5);
    assert(early.depth == 0.0f);
    assert(late.depth > early.depth);
    assert(late.population > early.population);
    assert(late.complexity > early.complexity);
    return 0;
}
