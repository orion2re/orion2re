#pragma once

#include <cstddef>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

#include "consts.h"
#include "ship_data.h"

namespace game {
    class s_ship_roster {
    public:
        struct s_entry {
            int row;
            s_ship_data ship;
            bool operator==(const s_entry&) const = default;
        };

        [[nodiscard]] const auto& Keyed_Records() const { return ships_; }
        [[nodiscard]] const auto& Unkeyed_Rows() const { return unkeyed_; }
        [[nodiscard]] auto size() const { return order_.size(); }
        [[nodiscard]] bool empty() const { return order_.empty(); }

        // Visit materialized records without using the row order. The visitor
        // may edit records, but must not change their keys or the roster.
        template<class Visitor>
        void Visit_Records(int extent, Visitor visit) {
            if (extent <= 0) return;
            for (auto& [key, entry] : ships_) {
                if (entry.row < extent) visit(entry.ship);
            }
            for (auto& [row, ship] : unkeyed_) {
                if (row >= static_cast<size_t>(extent)) break;
                visit(ship);
            }
        }

        [[nodiscard]] int Row(id_ship key) const {
            const auto entry = ships_.find(key);
            return entry == ships_.end() ? -1 : entry->second.row;
        }

        [[nodiscard]] const s_ship_data* Find(id_ship key, int extent = MAX_SHIPS) const {
            const auto entry = ships_.find(key);
            return entry == ships_.end() || entry->second.row >= extent ? nullptr : &entry->second.ship;
        }

        [[nodiscard]] s_ship_data* Find(id_ship key, int extent = MAX_SHIPS) {
            const auto entry = ships_.find(key);
            return entry == ships_.end() || entry->second.row >= extent ? nullptr : &entry->second.ship;
        }

        // The row view keeps legacy ordering; the keyed record is authoritative.
        const s_ship_data& operator[](size_t row) const {
            const auto key = order_.at(row);
            if (key != NO_SHIP) return ships_.at(key).ship;
            const auto entry = unkeyed_.find(row);
            return entry != unkeyed_.end() ? entry->second : empty_row_;
        }

        s_ship_data& operator[](size_t row) {
            const auto key = order_.at(row);
            if (key != NO_SHIP) return ships_.at(key).ship;
            return unkeyed_.try_emplace(row).first->second;
        }

        void Replace_Row(int row, s_ship_data ship) {
            auto& slot = order_.at(row);
            if (ship.id == slot && slot != NO_SHIP) {
                ships_.at(slot).ship = ship;
                return;
            }
            if (ship.id != NO_SHIP) {
                if (!ships_.try_emplace(ship.id, s_entry{row, ship}).second) throw std::logic_error("duplicate ship key");
                unkeyed_.erase(row);
            } else if (ship == empty_row_) {
                unkeyed_.erase(row);
            } else {
                unkeyed_.insert_or_assign(row, ship);
            }
            if (slot != NO_SHIP) ships_.erase(slot);
            slot = ship.id;
        }

        void Set_Key_At_Row(int row, id_ship key) {
            auto ship = std::as_const(*this)[row];
            ship.id = key;
            Replace_Row(row, ship);
        }

        void Swap_Rows(int first, int second) {
            if (first == second) return;
            std::swap(order_.at(first), order_.at(second));
            for (const int row : {first, second}) {
                const auto key = order_[row];
                if (key != NO_SHIP) ships_.at(key).row = row;
            }
            auto left = unkeyed_.extract(first);
            auto right = unkeyed_.extract(second);
            if (left) {
                left.key() = second;
                unkeyed_.insert(std::move(left));
            }
            if (right) {
                right.key() = first;
                unkeyed_.insert(std::move(right));
            }
        }

        // Records imported without a key are used by old fixtures and codecs.
        [[nodiscard]] int Row_Of(const s_ship_data& ship) const {
            if (ship.id != NO_SHIP) return Find(ship.id) == &ship ? Row(ship.id) : -1;
            for (const auto& [row, entry] : unkeyed_) if (&entry == &ship) return static_cast<int>(row);
            return -1;
        }

        void resize(size_t count) {
            for (size_t row = count; row < size(); ++row) {
                if (order_[row] != NO_SHIP) ships_.erase(order_[row]);
                unkeyed_.erase(row);
            }
            order_.resize(count, NO_SHIP);
        }

        void assign(size_t count, const s_ship_data& ship) {
            *this = {};
            resize(count);
            if (ship == empty_row_) return;
            for (size_t row = 0; row < count; ++row) Replace_Row(static_cast<int>(row), ship);
        }

        template<class Iterator>
        void assign(Iterator begin, Iterator end) {
            s_ship_roster staged;
            for (; begin != end; ++begin) {
                const auto row = staged.size();
                staged.order_.push_back(NO_SHIP);
                staged.Replace_Row(static_cast<int>(row), *begin);
            }
            *this = std::move(staged);
        }

        [[nodiscard]] std::vector<s_ship_data> Copy_Rows() const {
            std::vector<s_ship_data> rows;
            rows.reserve(size());
            for (size_t row = 0; row < size(); ++row) rows.push_back((*this)[row]);
            return rows;
        }

        bool operator==(const s_ship_roster& other) const {
            if (order_ != other.order_ || ships_ != other.ships_) return false;
            for (const auto& [row, ship] : unkeyed_) if (ship != other[row]) return false;
            for (const auto& [row, ship] : other.unkeyed_) if (ship != (*this)[row]) return false;
            return true;
        }

    private:
        std::map<id_ship, s_entry> ships_;
        std::vector<id_ship> order_;
        std::map<size_t, s_ship_data> unkeyed_;
        static inline const s_ship_data empty_row_{};
    };
}
