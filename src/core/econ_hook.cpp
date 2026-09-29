// ============================================================
//  EconHook — Local Inventory, Protobuf & SOCache Changer
// ============================================================
#include "econ_hook.h"
#include "skin_changer.h"
#include "../system/system_utils.h"
#include "pattern_scan.h"
#include <windows.h>
#include <vector>

namespace EconHook {
    static bool g_Installed = false;
    static std::vector<FakeEconItem> g_FakeItems;

    class ProtoWriter {
    public:
        std::vector<uint8_t> buffer;

        void WriteVarint32(uint32_t field_number, uint32_t value) {
            WriteKey(field_number, 0);
            WriteVarint32Val(value);
        }

        void WriteVarint64(uint32_t field_number, uint64_t value) {
            WriteKey(field_number, 0);
            WriteVarint64Val(value);
        }

        void WriteBytes(uint32_t field_number, const uint8_t* data, size_t size) {
            WriteKey(field_number, 2);
            WriteVarint32Val((uint32_t)size);
            buffer.insert(buffer.end(), data, data + size);
        }

    private:
        void WriteKey(uint32_t field_number, uint32_t wire_type) {
            WriteVarint32Val((field_number << 3) | wire_type);
        }

        void WriteVarint32Val(uint32_t value) {
            while (value >= 0x80) {
                buffer.push_back((uint8_t)(value | 0x80));
                value >>= 7;
            }
            buffer.push_back((uint8_t)value);
        }

        void WriteVarint64Val(uint64_t value) {
            while (value >= 0x80) {
                buffer.push_back((uint8_t)(value | 0x80));
                value >>= 7;
            }
            buffer.push_back((uint8_t)value);
        }
    };

    std::vector<uint8_t> SerializeCSOEconItem(uint64_t itemID, uint32_t defIndex, uint32_t heroID, uint32_t slot, uint32_t style) {
        ProtoWriter item;
        item.WriteVarint64(1, itemID);       // Field 1: id
        item.WriteVarint32(2, 0);            // Field 2: account_id
        item.WriteVarint32(3, defIndex);     // Field 3: def_index
        item.WriteVarint32(4, 1);            // Field 4: quantity
        item.WriteVarint32(5, 1);            // Field 5: level
        item.WriteVarint32(6, 4);            // Field 6: quality (4 = Genuine/Arcana)
        item.WriteVarint32(7, 1);            // Field 7: inventory
        item.WriteVarint32(8, 0);            // Field 8: origin
        item.WriteVarint32(9, style);        // Field 9: style

        // Field 16: equipped_state (CSOEconItemEquipped)
        ProtoWriter equipped;
        equipped.WriteVarint32(1, heroID);   // Field 1: new_class (Hero ID)
        equipped.WriteVarint32(2, slot);     // Field 2: new_slot (Slot)

        item.WriteBytes(16, equipped.buffer.data(), equipped.buffer.size());
        return item.buffer;
    }

    void InjectFakeItem(uint32_t heroID, uint32_t slot, uint32_t defIndex, uint32_t style) {
        if (defIndex == 0) return;
        FakeEconItem item;
        item.itemID   = 20000000000ULL + defIndex;
        item.defIndex = defIndex;
        item.heroID   = heroID;
        item.slot     = slot;
        item.style    = style;
        item.equipped = true;

        g_FakeItems.push_back(item);
        auto protoData = SerializeCSOEconItem(item.itemID, item.defIndex, item.heroID, item.slot, item.style);
        SystemUtils::Log("[EconHook] CSOEconItem Protobuf generated (%zu bytes) for DefIndex #%u (Hero %u, Slot %u)", 
                         protoData.size(), defIndex, heroID, slot);
    }

    void ClearFakeItems() {
        g_FakeItems.clear();
    }

    uint32_t FilterItemDefinitionIndex(uint32_t originalDefIndex) {
        if (!g_Installed || originalDefIndex == 0) return originalDefIndex;

        ItemOverrideRule rule;
        if (SkinChanger::GetInstance().FindItemOverride(originalDefIndex, rule)) {
            if (rule.enabled && rule.overrideDefIndex != 0) {
                return rule.overrideDefIndex;
            }
        }
        return originalDefIndex;
    }

    bool Install() {
        if (g_Installed) return true;
        SystemUtils::Log("[EconHook] SOCache & Protobuf Inventory Changer Active.");
        g_Installed = true;
        return true;
    }

    void Uninstall() {
        g_Installed = false;
        g_FakeItems.clear();
        SystemUtils::Log("[EconHook] Uninstalled.");
    }

    bool IsInstalled() { return g_Installed; }
}