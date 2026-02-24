#pragma once

#include <thread>
#include <condition_variable>
#include <mutex>

#include "Usings.h"
#include "OrderLockFree.h"
#include "OrderModify.h"
#include "OrderbookLevelInfos.h"
#include "Trade.h"

using OrderId = uint64_t;
using Quantity = uint32_t;


struct LockFreeTrade {
    OrderId bidOrderId;
    OrderId askOrderId;
    Price price;
    Quantity quantity;
};

using LockFreeTrades = std::vector<LockFreeTrade>;

class LockFreeOrderbook{
private:
    static constexpr Price MIN_PRICE = 0;
    static constexpr Price MAX_PRICE = 50000;
    static constexpr Price TICK_SIZE = 1;
    static constexpr size_t NUM_LEVELS = static_cast<size_t>((MAX_PRICE - MIN_PRICE) / TICK_SIZE);
    static constexpr size_t ORDER_TABLE_SIZE = 65536; // power of 2

    struct PriceLevel {
        std::atomic<Quantity> totalQuantity{0};
        std::atomic<uint32_t> orderCount{0};
        std::atomic<LockFreeOrder*> head{nullptr};
    };

    struct OrderSlot{
        std::atomic<OrderId> id{0};
        std::atomic<LockFreeOrder*> order{nullptr};
    };

    std::array<PriceLevel, NUM_LEVELS> bids_;
    std::array<PriceLevel, NUM_LEVELS> asks_;
    std::array<OrderSlot, ORDER_TABLE_SIZE> orderTable_; 

    std::atomic<uint64_t> nextOrderId_{1};

    // memory pool
    static constexpr size_t BLOCK_SIZE = 1024;
    struct Block {
        alignas(LockFreeOrder) char orders[BLOCK_SIZE * sizeof(LockFreeOrder)];
        Block* next;
    };
    Block* currentBlock_{nullptr};
    std::atomic<size_t> nextOrderIndex_{0};
    std::vector<Block*> allBlocks_;

    LockFreeOrder* allocateOrder(OrderType type, OrderId id, Side side,
                                 Price price, Quantity quantity);  
    size_t pricetoIndex(Price price) const;
    OrderSlot& getOrderSlot(OrderId id);
    void addtoLevel(LockFreeOrder* order, PriceLevel& level);
    LockFreeTrades matchOrders(LockFreeOrder* incomingOrder);

public:
    LockFreeOrderbook();
    ~LockFreeOrderbook();

    LockFreeTrades addOrder(OrderType type, Side side, Price price, Quantity quantity);
    bool cancelOrder(OrderId id);

    Quantity getBestBidQuantity() const;
    Quantity getBestAskQuantity() const;
    Price getBestBidPrice() const;
    Price getBestAskPrice() const;
};

