#pragma once

#include <cstdint>
#include <bitset>
#include <type_traits>

#include "consts.h"
#include "object_ids.h"

namespace game {
    // Combat assessment slots include players and non-player owners.
    using owner_bitmap = std::bitset<16>;

    class player_bitmap {
    public:
        static_assert(MAX_PLAYERS == 8);

        static constexpr player_bitmap From_Byte(uint8_t byte) { return player_bitmap(byte); }
        constexpr uint8_t To_Byte() const { return bits_; }

        constexpr player_bitmap() = default;
        static constexpr player_bitmap All() { return From_Byte(UINT8_MAX); }
        static constexpr player_bitmap Only(id_player player) { return From_Byte(Bit(player.id)); }
        static constexpr player_bitmap Only_Slot(uint32_t slot) { return Only(id_player{slot}); }

        constexpr bool any() const { return bits_ != 0; }
        constexpr bool none() const { return !any(); }
        constexpr explicit operator bool() const { return any(); }
        constexpr bool test(id_player player) const { return (bits_ & Bit(player.id)) != 0; }
        constexpr bool test_slot(uint32_t slot) const { return test(id_player{slot}); }
        constexpr void set(id_player player, bool value = true) {
            const auto bit = Bit(player.id);
            if (value) bits_ |= bit;
            else bits_ &= static_cast<uint8_t>(~bit);
        }
        constexpr void set_slot(uint32_t slot, bool value = true) { set(id_player{slot}, value); }
        constexpr void reset() { bits_ = 0; }
        constexpr void reset(id_player player) { set(player, false); }
        constexpr void reset_slot(uint32_t slot) { set_slot(slot, false); }
        constexpr void flip_slot(uint32_t slot) { bits_ ^= Bit(slot); }

        constexpr bool operator==(const player_bitmap&) const = default;
        constexpr player_bitmap& operator|=(player_bitmap other) { bits_ |= other.bits_; return *this; }
        constexpr player_bitmap& operator&=(player_bitmap other) { bits_ &= other.bits_; return *this; }
        friend constexpr player_bitmap operator|(player_bitmap a, player_bitmap b) { return a |= b; }
        friend constexpr player_bitmap operator&(player_bitmap a, player_bitmap b) { return a &= b; }
        constexpr player_bitmap operator~() const { return From_Byte(static_cast<uint8_t>(~bits_)); }

    private:
        constexpr explicit player_bitmap(uint8_t bits) : bits_(bits) {}
        static constexpr uint8_t Bit(uint32_t slot) {
            return slot < MAX_PLAYERS ? static_cast<uint8_t>(1u << slot) : 0;
        }
        uint8_t bits_ = 0;
    };

    static_assert(sizeof(player_bitmap) == 1);
    static_assert(std::is_trivially_copyable_v<player_bitmap>);
}
