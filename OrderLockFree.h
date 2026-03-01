#pragma once

#include <atomic>
#include "OrderType.h"
#include "Side.h"
#include "Usings.h"
#include "Constants.h"

enum class OrderStatus {
    Active,
    Cancelled,
    Filled
};

class LockFreeOrder {
public:
    const OrderId orderId_;
    const Side side_;
    const OrderType orderType_;
    const Quantity initialQuantity_;

    std::atomic<Price> price_;
    std::atomic<Quantity> remainingQuantity_;
    std::atomic<OrderStatus> status_{OrderStatus::Active};
    std::atomic<uint32_t> version_{0};
    std::atomic<uint64_t> lastUpdatetime_;
    std::atomic<LockFreeOrder*> next_{nullptr};

    // Constructors
    LockFreeOrder(OrderType orderType, OrderId orderId, Side side,
                  Price price, Quantity quantity);
    LockFreeOrder(OrderId orderid, Side side, Quantity quantity);

    // Getters (keep inline for performance)
    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    OrderType GetOrderType() const { return orderType_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    
    Price GetPrice() const;
    Quantity GetRemainingQuantity() const;
    Quantity GetFilledQuantity() const;
    bool IsFilled() const;

    // Core operations
    bool TryFill(Quantity quantity);
    bool FastFill(Quantity quantity);
    bool ConvertToGTC(Price price);

    // Linked list operations
    LockFreeOrder* GetNext() const;
    void SetNext(LockFreeOrder* next);
    bool CompareAndSwapNext(LockFreeOrder* expected, LockFreeOrder* desired);
};