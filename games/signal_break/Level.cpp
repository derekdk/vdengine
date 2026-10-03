#include "Level.h"

namespace signalbreak {

const std::vector<Platform>& platforms() {
    static const std::vector<Platform> layout{
        {{20.0f, -1.0f}, {20.0f, 1.0f}},  {{61.5f, -1.0f}, {18.5f, 1.0f}},
        {{101.5f, -1.0f}, {18.5f, 1.0f}}, {{13.0f, 2.0f}, {3.0f, 0.2f}},
        {{23.0f, 4.2f}, {3.0f, 0.2f}},    {{33.0f, 2.0f}, {3.0f, 0.2f}},
        {{50.0f, 2.0f}, {3.0f, 0.2f}},    {{60.0f, 4.2f}, {3.0f, 0.2f}},
        {{71.0f, 2.0f}, {3.0f, 0.2f}},    {{90.0f, 2.0f}, {3.0f, 0.2f}},
        {{100.0f, 4.2f}, {3.0f, 0.2f}},   {{109.0f, 2.0f}, {3.0f, 0.2f}},
        {{-0.5f, 6.0f}, {0.5f, 8.0f}},    {{120.5f, 6.0f}, {0.5f, 8.0f}},
    };
    return layout;
}

const std::vector<EnemySpawn>& enemySpawns() {
    static const std::vector<EnemySpawn> spawns{
        {{13.0f, 2.9f}},  {{18.0f, 0.7f}},        {{23.0f, 5.1f}}, {{33.0f, 2.9f}},
        {{37.0f, 0.7f}},  {{50.0f, 2.9f}},        {{56.0f, 0.7f}}, {{60.0f, 5.1f}},
        {{71.0f, 2.9f}},  {{76.0f, 0.7f}},        {{90.0f, 2.9f}}, {{100.0f, 5.1f}},
        {{109.0f, 2.9f}}, {{114.0f, 0.7f}, true},
    };
    return spawns;
}

}  // namespace signalbreak