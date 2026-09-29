#pragma once
#include <cstdint>
#include "../sdk/dota_structs.h"

namespace Visuals {

    struct Config {
        bool enabled        = false;
        bool showHealthBar  = true;
        bool showManaBar    = true;
        bool showNames      = true;
        bool showDistance   = false;
        bool showEnemyOnly  = false;
        float espBoxAlpha   = 0.85f;
    };
    extern Config g_Cfg;

    void Tick();
    void Render();

} // namespace Visuals