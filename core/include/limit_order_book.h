#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>
#include "imp_macros.h"

// --- CONFIGURATION CONSTANTS ---
constexpr int MAX_PRICE_TICKS = 1000000; // Covers range $0.00 to $10,000.00
constexpr int MAX_ORDERS = 2000000;      // Max active orders limit
constexpr double MIN_PRICE = 0.00;
constexpr double TICK_SIZE = 0.01;

// --- DATA STRUCTURES ---

// 1. The Order (Managed by the Book)
struct Order {
    int system_id;       // Internal ID (0 to MAX_ORDERS) ==> why internal id ?? => because internal id which are small ints are fast for array indexin, cache friedly.
    long long client_id; // User's ID (Timestamp + UserID)
    int qty;
    double price;        // Cached price for reporting
    bool active;         // Lazy deletion flag
};

// 2. The Trade Report (Result of a match)
struct Trade {
    long long buyer_client_id;
    long long seller_client_id;
    int buy_system_id;
    int sell_system_id;
    double price;
    int qty;
    char aggressor_side; // 'B' or 'S'
};

// 3. One Price Level (A Queue of Orders)
// Implements priority queue logic using a vector + head cursor (Lazy Deletion)
/*

    Normal vector deletion is O(N) ==> slow.
    Lazy deletion with cursor is O(1) ==> fast.

    Memory layout:
    [ Dead | Dead | Dead | Head -> | Active | Active ]
                           ^
                           Cursor moves forward.
*/
struct LimitLevel {
    std::vector<Order> orders;
    int head = 0; // Cursor pointing to the first valid order

    LimitLevel() {
        orders.reserve(100); // Pre-reserve some space to avoid immediate realloc
    }

    // Add order to the back of the line (Time Priority)
    void add(const Order& o) {
        orders.push_back(o);
    }

    // Check if level is effectively empty
    bool isEmpty() const {
        return head >= static_cast<int>(orders.size());
    }
};

// 4. Lookup Table Entry for O(1) Access
struct OrderLocation {
    int price_idx;
    int vector_idx;
    bool is_buy;
    bool active;
};

// --- THE MATCHING ENGINE CORE ---
class LimitOrderBook {
private:
    // Static Arrays for High Performance (Data Oriented Design)
    // No pointer chasing, memory is contiguous where it counts.
    // why arrays ?? ==> because vectors have metadata overhead and can realloc, we want fixed memory region for cache coherancy.
    LimitLevel bids[MAX_PRICE_TICKS];
    LimitLevel asks[MAX_PRICE_TICKS];

    // O(1) Lookup Table: SystemID -> Memory Location
    // We can jump straight to the order in memory using this.
    OrderLocation order_lookup[MAX_ORDERS];

    // Trackers for the "Top of Book" to avoid scanning empty arrays
    int best_bid_idx = -1;
    int best_ask_idx = MAX_PRICE_TICKS;

    // Helper: Convert Price to Array Index
    // $100.50 -> 10050
    // Uses generic safe clamping
    inline int getIndex(double price) const {
        int idx = static_cast<int>((price - MIN_PRICE) / TICK_SIZE);
        if (UNLIKELY(idx < 0)) return 0;
        if (UNLIKELY(idx >= MAX_PRICE_TICKS)) return MAX_PRICE_TICKS - 1;
        return idx;
    }

public:
    LimitOrderBook() {
        // Initialize lookup table to inactive
        for(int i = 0; i < MAX_ORDERS; ++i) {
            order_lookup[i].active = false;
        }
    }

    // --- MAIN ACTION: ADD / MATCH ORDER ---
    // Returns a vector of Trades executed immediately
    // If qty remains, the order is added to the book
    std::vector<Trade> matchOrder(int system_id, long long client_id, char side, double price, int qty) {
        std::vector<Trade> trades;
        int price_idx = getIndex(price);

        // 1. MATCHING LOGIC (Aggressive Phase)
        if (side == 'B') {
            // BUYER: Looks for Sellers (Asks) <= Price
            // Start from lowest Ask (best_ask_idx) up to our Price
            while (qty > 0 && best_ask_idx < MAX_PRICE_TICKS) {
                
                // If the best ask is more expensive than our limit, stop.
                if (best_ask_idx > price_idx) break;

                // Match against this level
                matchLevel(system_id, client_id, side, bids[best_bid_idx] /*dummy*/, asks[best_ask_idx], 
                           qty, best_ask_idx, trades);

                // If level cleared, find next best ask
                if (asks[best_ask_idx].isEmpty()) {
                    best_ask_idx++;
                    while (best_ask_idx < MAX_PRICE_TICKS && asks[best_ask_idx].isEmpty()) {
                        best_ask_idx++;
                    }
                }
            }
        } 
        else {
            // SELLER: Looks for Buyers (Bids) >= Price
            // Start from highest Bid (best_bid_idx) down to our Price
            while (qty > 0 && best_bid_idx >= 0) {

                // If best bid is lower than our limit, stop.
                if (best_bid_idx < price_idx) break;

                matchLevel(system_id, client_id, side, bids[best_bid_idx], asks[best_ask_idx] /*dummy*/, 
                           qty, best_bid_idx, trades);

                if (bids[best_bid_idx].isEmpty()) {
                    best_bid_idx--;
                    while (best_bid_idx >= 0 && bids[best_bid_idx].isEmpty()) {
                        best_bid_idx--;
                    }
                }
            }
        }

        // 2. RESTING LOGIC (Passive Phase)
        // If qty > 0, place remainder in Book
        if (qty > 0) {
            addOrderToBook(system_id, client_id, side, price, price_idx, qty);
        }

        return trades;
    }

    // --- MODIFICATION LOGIC (Priority Fairness) ---
    // Returns true if successful
    bool modifyOrder(int system_id, int new_qty, double new_price) {
        if (UNLIKELY(system_id < 0 || system_id >= MAX_ORDERS)) return false;
        if (!order_lookup[system_id].active) return false;

        OrderLocation& loc = order_lookup[system_id];
        
        // Locate the order in memory
        LimitLevel& level = (loc.is_buy ? bids[loc.price_idx] : asks[loc.price_idx]);
        
        // Safety check for stale pointers (though active flag should handle this)
        if (loc.vector_idx >= static_cast<int>(level.orders.size())) return false;
        
        Order& ord = level.orders[loc.vector_idx];

        // LOGIC CHECK:
        // Case A: Qty Decrease -> In-Place Update (Preserve Priority)
        // Case B: Qty Increase or Price Change -> Cancel & Re-Add (Lose Priority)
        // Note: Re-Add requires logic external to this method normally (assigning new ID or moving).
        // For simplicity in this engine, we will move it to the back OF THE NEW/SAME LEVEL.
        
        bool price_changed = (std::abs(new_price - ord.price) > 0.00001);
        
        if (!// why is this allowed ? ==> because reducing risk is good for market so we don't punish priority.
             price_changed && new_qty < ord.qty) {
             // UPDATE IN PLACE
             ord.qty = new_qty;
             return true;
        }
        else {
            // CANCEL AND MOVE at this level (or new level)
            // 1. Mark old dead
            ord.active = false;
            
            // 2. Add as if new
            // Note: We are reusing the system_id here effectively moving the order.
            // But usually this generates a new system_id. 
            // For now, let's assume we reuse and update lookup.
            
            int new_price_idx = getIndex(new_price);
            addOrderToBook(system_id, ord.client_id, loc.is_buy ? 'B' : 'S', new_price, new_price_idx, new_qty);
            return true;
        }
    }

    // --- CANCELLATION ---
    bool cancelOrder(int system_id) {
        if (UNLIKELY(system_id < 0 || system_id >= MAX_ORDERS)) return false;
        if (!order_lookup[system_id].active) return false;

        OrderLocation& loc = order_lookup[system_id];
        LimitLevel& level = (loc.is_buy ? bids[loc.price_idx] : asks[loc.price_idx]);
        
        if (LIKELY(loc.vector_idx < static_cast<int>(level.orders.size()))) {
            level.orders[loc.vector_idx].active = false;
        }

        // Mark lookup dead
        order_lookup[system_id].active = false;
        
        // We do not cleanup level.head immediately, it happens lazily during matching
        return true;
    }

private:
    // Helper to add order to the specific queue
    void addOrderToBook(int system_id, long long client_id, char side, double price, int price_idx, int qty) {
        Order o;
        o.system_id = system_id;
        o.client_id = client_id;
        o.qty = qty;
        o.price = price;
        o.active = true;

        if (side == 'B') {
            bids[price_idx].add(o);
            
            // Update Lookup
            order_lookup[system_id] = {
                price_idx, 
                (int)bids[price_idx].orders.size() - 1, 
                true, // is_buy
                true  // active
            };

            // Update High Water Mark
            if (price_idx > best_bid_idx) best_bid_idx = price_idx;
        } 
        else {
            asks[price_idx].add(o);

            order_lookup[system_id] = {
                price_idx, 
                (int)asks[price_idx].orders.size() - 1, 
                false, // is_buy
                true   // active
            };

            // Update Low Water Mark
            if (price_idx < best_ask_idx) best_ask_idx = price_idx;
        }
    }

    // Helper to match an aggressor against a resting level
    void matchLevel(int agg_sys_id, long long agg_client_id, char agg_side,
                   LimitLevel& bids_level, LimitLevel& asks_level,
                   int& agg_qty, int /*price_idx*/, std::vector<Trade>& trades) 
    {
        // Select the relevant book side
        LimitLevel& level = (agg_side == 'B') ? asks_level : bids_level;

        // Iterate through the orders starting at HEAD (Lazy Deletion)
        while (!level.isEmpty() && agg_qty > 0) {
            
            Order& resting = level.orders[level.head];

            // Skip dead orders (Lazy Deleted)
            if (!resting.active) {
                level.head++;
                continue;
            }

            // Execute Trade
            int fill_qty = std::min(agg_qty, resting.qty);
            double trade_price = resting.price; // Maker's Price

            Trade t;
            t.buyer_client_id = (agg_side == 'B') ? agg_client_id : resting.client_id;
            t.seller_client_id = (agg_side == 'S') ? agg_client_id : resting.client_id;
            t.buy_system_id = (agg_side == 'B') ? agg_sys_id : resting.system_id;
            t.sell_system_id = (agg_side == 'S') ? agg_sys_id : resting.system_id;
            t.price = trade_price;
            t.qty = fill_qty;
            t.aggressor_side = agg_side;
            
            trades.push_back(t);

            // Update State
            agg_qty -= fill_qty;
            resting.qty -= fill_qty;

            // If resting order fully filled
            if (resting.qty == 0) {
                resting.active = false;
                order_lookup[resting.system_id].active = false; // Mark lookup dead
                level.head++; // Move cursor
            }
        }

        // Cleanup: If the vector is getting huge but mostly empty at front, 
        // in a real system we might compact it. 
        // For HFT sim, we often let it grow or reset overnight. 
        // Adding a simple check:
        if (level.head > 1000 && level.head > static_cast<int>(level.orders.size()) / 2) {
             // Compaction logic could go here, but avoiding copy is better.
             // leaving as strictly lazy for now.
        }
    }
};
