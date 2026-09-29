#pragma once
#include <cstdint>
#include <cstdio>

// ============================================================
//  Dota 2 Item Schema — Item Definition Indexes
//
//  Источник: items_game.txt из pak01_dir.vpk
//  Онлайн:  https://www.dota2.com/store/itemdetails/<defIndex>
//
//  Слоты (m_hMyWearables):
//    0 = Weapon / Projectile
//    1 = Head
//    2 = Shoulder
//    3 = Arms / Bracers
//    4 = Back / Cape
//    5 = Armor / Chest
//    6 = Belt / Tail
//    7 = Legs / Feet
//
//  ВАЖНО: "Golden" / "Exalted" версии аркан — это ТОТ ЖЕ defIndex,
//         но другой m_nStyle. Отдельных defIndex'ов у них нет.
//         Здесь оставлены только реальные defIndex.
// ============================================================

struct ItemEntry {
    uint32_t    defIndex;
    const char* name;
    const char* heroName;
    int         slot;       // -1 = unknown
    uint32_t    style = 0;  // 0 = default, 1 = golden/exalted
};

// ─────────────────────────────────────────────────────────────
//  ARCANA (только проверенные defIndex по состоянию на 2025)
// ─────────────────────────────────────────────────────────────
static const ItemEntry g_ArcanasDB[] = {
    // Anti-Mage
    { 9258,   "Manifold Paradox",               "Anti-Mage",            0 },
    // Axe
    { 16808,  "Axe of Phractos",                "Axe",                  0 },
    // Crystal Maiden
    { 10037,  "Frost Avalanche",                "Crystal Maiden",       1 },
    // Lion
    { 15855,  "Finger of Death",                "Lion",                 0 },
    // Dragon Knight
    { 16789,  "Dragonforged",                   "Dragon Knight",        5 },
    // Earthshaker
    { 10056,  "Seismic Resonator",              "Earthshaker",          0 },
    // Enigma
    { 16702,  "Timebreaker",                    "Enigma",               0 },
    // Faceless Void
    { 10069,  "Dark Artistry",                  "Faceless Void",        0 },
    // Invoker
    { 6156,   "The Magus Apex",                 "Invoker",              0 },
    // Io
    { 12076,  "Blinding Light",                 "Io",                   0 },
    // Juggernaut
    { 10006,  "Bladeform Legacy",               "Juggernaut",           0 },
    // Lina
    { 7282,   "Lina Arcana",                    "Lina",                 0 },
    { 7282,   "Golden Lina Arcana",             "Lina",                 0, 1 },
    // Mirana
    { 9356,   "Mirana Arcana",                  "Mirana",               0 },
    // Monkey King
    { 16798,  "Wukong's Command",               "Monkey King",          0 },
    // Ogre Magi
    { 17124,  "Flockheart's Gamble",            "Ogre Magi",            1 },
    // Outworld Destroyer
    { 8025,   "Vigil Triumph",                  "Outworld Devourer",    0 },
    // Phantom Assassin
    { 9355,   "Blades of Voth Domosh",          "Phantom Assassin",     0 },
    // Pudge
    { 16716,  "Feast of Abscession",            "Pudge",                0 },
    // Queen of Pain
    { 16770,  "Queen of Pain Arcana",           "Queen of Pain",        0 },
    // Shadow Fiend
    { 10045,  "Arcana of Annihilation",         "Shadow Fiend",         0 },
    // Slark
    { 16782,  "Depth's Insurgent",              "Slark",                0 },
    { 16782,  "Golden Depth's Insurgent",       "Slark",                0, 1 },
    // Spectre
    { 16751,  "Spectre Arcana",                 "Spectre",              0 },
    // Storm Spirit
    { 16761,  "Storm Spirit Arcana",            "Storm Spirit",         0 },
    // Templar Assassin
    { 10028,  "Templar Assassin Arcana",        "Templar Assassin",     0 },
    // Terrorblade
    { 10023,  "Fractal Horns of Inner Abysm",   "Terrorblade",          1 },
    // Vengeful Spirit
    { 16723,  "Vengeful Spirit Arcana",         "Vengeful Spirit",      0 },
    // Void Spirit
    { 16739,  "Void Spirit Arcana",             "Void Spirit",          0 },
    // Windranger
    { 16730,  "Windranger Arcana",              "Windranger",           0 },
    // Wraith King
    { 6330,   "Bone of Wraithmourne",           "Wraith King",          0 },
    // Witch Doctor
    { 16744,  "Witch Doctor Arcana",            "Witch Doctor",         0 },
};

// ─────────────────────────────────────────────────────────────
//  IMMORTAL (проверенные defIndex, без фейковых "Golden")
// ─────────────────────────────────────────────────────────────
static const ItemEntry g_ImmortalsDB[] = {
    // Pudge
    { 4007,   "Dragonclaw Hook (Def #4007)",    "Pudge",                0 },
    { 7061,   "Dragonclaw Hook (Def #7061)",    "Pudge",                0 },
    { 15887,  "Lil' Shredder",                  "Pudge",                0 },
    // Anti-Mage
    { 7030,   "Manifold Paradox (Immortal)",    "Anti-Mage",            0 },
    { 15890,  "Mage Slayer",                    "Anti-Mage",            0 },
    // Juggernaut
    { 7035,   "Bladeform Legacy (Immortal)",    "Juggernaut",           0 },
    { 12083,  "Severing Crest",                 "Juggernaut",           0 },
    // Terrorblade
    { 7060,   "Fractal Horns (Immortal)",       "Terrorblade",          1 },
    { 15864,  "Fractal Horns of Outer Abysm",   "Terrorblade",          1 },
    // Windranger
    { 8014,   "Arcana of the Wind",             "Windranger",           0 },
    { 15897,  "Golden Windranger Bow",          "Windranger",           0, 1 },
    // Dragon Knight
    { 9241,   "Dragon Lance",                   "Dragon Knight",        0 },
    { 15943,  "Kindred of the Iron Dragon",     "Dragon Knight",        5 },
    // Axe
    { 7119,   "Axe of Phractos (Immortal)",     "Axe",                  0 },
    { 15862,  "Beadle's Bludgeon",              "Axe",                  0 },
    // Earthshaker
    { 7121,   "Genuine Outworld Stone",         "Earthshaker",          0 },
    { 15933,  "Planetfall",                     "Earthshaker",          0 },
    // Lifestealer
    { 7076,   "Feast of Greed",                 "Lifestealer",          0 },
    { 15940,  "Infernal Nasal Goo",             "Lifestealer",          0 },
    // Faceless Void
    { 7108,   "Golden Trove Carafe 2017",       "Faceless Void",        1, 1 },
    { 15869,  "Chrono Crafter",                 "Faceless Void",        0 },
    // Phantom Assassin
    { 9355,   "Golden Blades of Voth Domosh",   "Phantom Assassin",     0, 1 },
    { 15877,  "Seclusions of the Void",         "Phantom Assassin",     0 },
    // Shadow Fiend
    { 10045,  "Arcana of Annihilation (Imm.)",  "Shadow Fiend",         0 },
    { 15905,  "Sullen Hollow",                  "Shadow Fiend",         0 },
    // Crystal Maiden
    { 10037,  "Golden Frost Avalanche",         "Crystal Maiden",       1, 1 },
    // Queen of Pain
    { 15947,  "Instruments of Torment",         "Queen of Pain",        0 },
    // Bounty Hunter
    { 7097,   "Golden Shuriken of Veiled Ones", "Bounty Hunter",        0, 1 },
    // Dazzle
    { 7128,   "Vials of the Sunken Tribute",    "Dazzle",               0 },
    // Ogre Magi
    { 7082,   "Genuine Tribal Pathways",        "Ogre Magi",            1 },
    // Invoker
    { 6156,   "The Magus Apex (Immortal)",      "Invoker",              0 },
    // Sven
    { 12084,  "Thunder Hammer",                 "Sven",                 0 },
    // Tidehunter
    { 15930,  "Trident of the Deep Current",    "Tidehunter",           0 },
    // Timbersaw
    { 15937,  "Eternal Saw",                    "Timbersaw",            0 },
    // Troll Warlord
    { 15955,  "Berserker's Bone Cutter",        "Troll Warlord",        0 },
    // Ursa
    { 15961,  "Howling Wolf",                   "Ursa",                 0 },
};

// ─── Helper accessors ────────────────────────────────────────
inline const ItemEntry* GetArcanasDB(int& outCount) {
    outCount = static_cast<int>(sizeof(g_ArcanasDB) / sizeof(g_ArcanasDB[0]));
    return g_ArcanasDB;
}

inline const ItemEntry* GetImmortalsDB(int& outCount) {
    outCount = static_cast<int>(sizeof(g_ImmortalsDB) / sizeof(g_ImmortalsDB[0]));
    return g_ImmortalsDB;
}

inline int GetItemSlotFromDB(uint32_t defIndex) {
    if (defIndex == 0) return -1;
    // Known Pudge weapons/hooks
    if (defIndex == 4001 || defIndex == 125 || defIndex == 4007 || defIndex == 7061 ||
        defIndex == 4052 || defIndex == 4268 || defIndex == 7356 || defIndex == 7357 ||
        defIndex == 9662 || defIndex == 15887 || defIndex == 4312 || defIndex == 4568 ||
        defIndex == 4795 || defIndex == 4933) {
        return 0; // Weapon slot
    }
    for (const auto& e : g_ArcanasDB)   if (e.defIndex == defIndex && e.slot >= 0) return e.slot;
    for (const auto& e : g_ImmortalsDB) if (e.defIndex == defIndex && e.slot >= 0) return e.slot;
    return -1;
}

inline const char* FindItemName(uint32_t defIndex) {
    if (defIndex == 0) return "(Default)";
    for (const auto& e : g_ArcanasDB)   if (e.defIndex == defIndex) return e.name;
    for (const auto& e : g_ImmortalsDB) if (e.defIndex == defIndex) return e.name;
    static thread_local char buf[24];
    snprintf(buf, sizeof(buf), "#%u", defIndex);
    return buf;
}

// ─────────────────────────────────────────────────────────────
//  HERO DATABASE (heroID → name)
//
//  Используется в Inventory Changer для списка героев.
//  Имена должны совпадать с heroName в g_ArcanasDB / g_ImmortalsDB.
// ─────────────────────────────────────────────────────────────
struct HeroEntry {
    uint32_t    heroID;
    const char* name;
};

static const HeroEntry g_HeroesDB[] = {
    {   1, "Anti-Mage"         },
    {   2, "Axe"               },
    {   5, "Crystal Maiden"    },
    {   7, "Earthshaker"       },
    {   8, "Juggernaut"        },
    {   9, "Mirana"            },
    {  11, "Shadow Fiend"      },
    {  14, "Pudge"             },
    {  17, "Storm Spirit"      },
    {  18, "Sven"              },
    {  21, "Windranger"        },
    {  25, "Lina"              },
    {  26, "Lion"              },
    {  30, "Witch Doctor"      },
    {  33, "Enigma"            },
    {  35, "Sniper"            },
    {  39, "Queen of Pain"     },
    {  41, "Faceless Void"     },
    {  42, "Wraith King"       },
    {  44, "Phantom Assassin"  },
    {  46, "Templar Assassin"  },
    {  49, "Dragon Knight"     },
    {  54, "Lifestealer"       },
    {  59, "Huskar"            },
    {  62, "Bounty Hunter"     },
    {  67, "Spectre"           },
    {  70, "Ursa"              },
    {  74, "Invoker"           },
    {  76, "Outworld Devourer" },
    {  84, "Ogre Magi"         },
    {  91, "Io"                },
    {  93, "Slark"             },
    {  95, "Troll Warlord"     },
    {  98, "Timbersaw"         },
    { 104, "Legion Commander"  },
    { 109, "Terrorblade"       },
    { 114, "Monkey King"       },
    { 119, "Dark Willow"       },
    { 126, "Void Spirit"       },
    { 128, "Snapfire"          },
    { 129, "Mars"              },
    { 136, "Marci"             },
    { 138, "Muerta"            },
};

// ─────────────────────────────────────────────────────────────
//  FULL SETS DATABASE (Overplus style 1-click set bundles)
// ─────────────────────────────────────────────────────────────
struct SetSlotItem {
    int      slot;
    uint32_t defIndex;
    uint32_t style = 0;
};

struct FullSetEntry {
    const char* setName;
    const char* heroName;
    uint32_t    heroID;
    SetSlotItem items[6];
    int         itemCount;
};

static const FullSetEntry g_FullSetsDB[] = {
    // 1. Pudge - Full Feast Arcana & Dragonclaw Set
    { "Pudge - Dragonclaw & Feast Arcana", "Pudge", 14, {
        { 0, 4007, 0 },  // Weapon: Dragonclaw Hook #4007
        { 0, 16716, 0 }, // Back/Chest: Feast of Abscession Arcana #16716
        { 3, 15887, 0 }, // Arms: Lil' Shredder #15887
        { 1, 4052, 0 },  // Head: Black Death #4052
        { 6, 4268, 0 },  // Belt: Mad Harvester #4268
    }, 5 },

    // 2. Pudge - Full Abscesserator Set
    { "Pudge - The Abscesserator Immortal Set", "Pudge", 14, {
        { 0, 7061, 0 },  // Weapon: Dragonclaw Hook #7061
        { 1, 7356, 0 },  // Head: Abscesserator Head #7356
        { 2, 7357, 0 },  // Shoulder: Abscesserator Shoulder #7357
        { 3, 15887, 0 }, // Arms: Lil' Shredder #15887
    }, 4 },

    // 3. Juggernaut - Full Bladeform Legacy Arcana Set
    { "Juggernaut - Bladeform Legacy Full Arcana", "Juggernaut", 8, {
        { 0, 10006, 0 }, // Head: Bladeform Legacy Arcana #10006
        { 0, 7035, 0 },  // Weapon: Bladeform Sword #7035
        { 2, 12083, 0 }, // Shoulder: Severing Crest #12083
        { 5, 4100, 0 },  // Armor: Crest of the Lost Order #4100
    }, 4 },

    // 4. Phantom Assassin - Full Manifold Paradox Arcana Set
    { "Phantom Assassin - Manifold Paradox Arcana", "Phantom Assassin", 44, {
        { 0, 9355, 0 },  // Weapon: Manifold Paradox Arcana #9355
        { 1, 15877, 0 }, // Head: Seclusions of the Void #15877
        { 5, 4390, 0 },  // Armor: Creeping Shadow #4390
        { 2, 4520, 0 },  // Shoulder: Penumbral Vestments #4520
    }, 4 },

    // 5. Anti-Mage - Full Manifold Paradox Arcana Set
    { "Anti-Mage - Mage Slayer & Arcana Set", "Anti-Mage", 1, {
        { 0, 9258, 0 },  // Weapon: Manifold Paradox Arcana #9258
        { 0, 7030, 0 },  // Offhand: Basher of Mage Skulls #7030
        { 1, 15890, 0 }, // Head: Mage Slayer #15890
        { 5, 4200, 0 },  // Armor: Guilt of the Survivor #4200
    }, 4 },

    // 6. Shadow Fiend - Full Demon Eater Arcana Set
    { "Shadow Fiend - Demon Eater Full Arcana Set", "Shadow Fiend", 11, {
        { 0, 10045, 0 }, // Head: Arcana of Annihilation #10045
        { 3, 15905, 0 }, // Arms: Sullen Hollow #15905
        { 2, 4300, 0 },  // Shoulder: Eternal Harvest #4300
    }, 3 },

    // 7. Terrorblade - Full Fractal Horns Arcana Set
    { "Terrorblade - Fractal Horns Full Arcana Set", "Terrorblade", 109, {
        { 1, 10023, 0 }, // Head: Fractal Horns Arcana #10023
        { 0, 7060, 0 },  // Weapon: Scythes of Sorrow #7060
        { 5, 15864, 0 }, // Armor: Outer Abysm #15864
    }, 3 },

    // 8. Crystal Maiden - Full Frost Avalanche Arcana Set
    { "Crystal Maiden - Frost Avalanche Full Arcana", "Crystal Maiden", 5, {
        { 4, 10037, 0 }, // Back: Frost Avalanche Arcana #10037
        { 1, 4026, 0 },  // Head: White Sentry #4026
        { 0, 4150, 0 },  // Staff: Yulsaria's Glacier #4150
    }, 3 },

    // 9. Windranger - Full Compass of Rising Gale Set
    { "Windranger - Compass of Rising Gale Arcana", "Windranger", 21, {
        { 0, 16730, 0 }, // Back: Windranger Arcana #16730
        { 0, 8014, 0 },  // Bow: Arcana of the Wind #8014
        { 1, 15897, 0 }, // Head: Golden Windranger Bow #15897
    }, 3 },

    // 10. Queen of Pain - Full Eminence of Ristul Arcana
    { "Queen of Pain - Eminence of Ristul Arcana", "Queen of Pain", 39, {
        { 0, 16770, 0 }, // Back: QoP Arcana #16770
        { 0, 15947, 0 }, // Weapon: Instruments of Torment #15947
        { 1, 4211, 0 },  // Head: Crown of Torment #4211
    }, 3 },

    // 11. Invoker - Full Dark Artistry Persona Set
    { "Invoker - Dark Artistry Persona & Apex", "Invoker", 74, {
        { 0, 6156, 0 },  // Head: The Magus Apex #6156
        { 4, 10069, 0 }, // Cape: Dark Artistry Cape #10069
        { 2, 6156, 0 },  // Shoulder: Dark Artistry Hair #6156
    }, 3 },

    // 12. Faceless Void - Full Claszian Apostasy Arcana
    { "Faceless Void - Claszian Apostasy Arcana Set", "Faceless Void", 41, {
        { 1, 10069, 0 }, // Head: Dark Artistry / Void Arcana #10069
        { 0, 7108, 0 },  // Weapon: Timebreaker / Mace #7108
        { 3, 15869, 0 }, // Bracers: Chrono Crafter #15869
    }, 3 },
};

inline const FullSetEntry* GetFullSetsDB(int& outCount) {
    outCount = static_cast<int>(sizeof(g_FullSetsDB) / sizeof(g_FullSetsDB[0]));
    return g_FullSetsDB;
}

inline const HeroEntry* GetHeroesDB(int& outCount) {
    outCount = static_cast<int>(sizeof(g_HeroesDB) / sizeof(g_HeroesDB[0]));
    return g_HeroesDB;
}

inline const char* FindHeroName(uint32_t heroID) {
    for (const auto& h : g_HeroesDB)
        if (h.heroID == heroID) return h.name;
    return nullptr;
}