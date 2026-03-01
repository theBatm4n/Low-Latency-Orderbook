#include "OrderLockFree.h"
#include <format>
#include <stdexcept>

LockFreeOrder::LockFreeOrder(OrderType orderType, OrderId orderId, Side side,
                             Price price, Quantity quantity)
    : orderId_(orderId)
    , side_(side)
    , orderType_(orderType)
    , price_(price)
    , initialQuantity_(quantity)
    , remainingQuantity_(quantity)
{}

LockFreeOrder::LockFreeOrder(OrderId orderid, Side side, Quantity quantity)
    : LockFreeOrder(OrderType::Market, orderid, side, Constants::InvalidPrice, quantity)
{}

Price LockFreeOrder::GetPrice() const {
    return price_.load(std::memory_order_acquire);
}

Quantity LockFreeOrder::GetRemainingQuantity() const {
    return remainingQuantity_.load(std::memory_order_acquire);
}

Quantity LockFreeOrder::GetFilledQuantity() const {
    return GetInitialQuantity() - GetRemainingQuantity();
}

bool LockFreeOrder::IsFilled() const {
    return GetRemainingQuantity() == 0;
}

bool LockFreeOrder::TryFill(Quantity quantity) {
    Quantity current = remainingQuantity_.load(std::memory_order_acquire);
    Quantity new_remaining;

    do {
        if (quantity > current) {
            return false;
        }
        new_remaining = current - quantity;
    } while (!remainingQuantity_.compare_exchange_weak(
        current, new_remaining,
        std::memory_order_release,
        std::memory_order_acquire));
    return true;
}

bool LockFreeOrder::FastFill(Quantity quantity) {
    Quantity current = remainingQuantity_.load(std::memory_order_acquire);
    if (quantity > current) {
        return false;
    }
    Quantity new_remaining = current - quantity;
    remainingQuantity_.store(new_remaining, std::memory_order_release);
    return true;
}

bool LockFreeOrder::ConvertToGTC(Price price) {
    if (orderType_ != OrderType::Market) {
        return false;
    }
    price_.store(price, std::memory_order_release);
    version_.fetch_add(1, std::memory_order_release);
    return true;
}

LockFreeOrder* LockFreeOrder::GetNext() const {
    return next_.load(std::memory_order_acquire);
}

void LockFreeOrder::SetNext(LockFreeOrder* next) {
    next_.store(next, std::memory_order_release);
}

bool LockFreeOrder::CompareAndSwapNext(LockFreeOrder* expected, LockFreeOrder* desired) {
    return next_.compare_exchange_weak(
        expected, desired,
        std::memory_order_release,
        std::memory_order_acquire
    );
}