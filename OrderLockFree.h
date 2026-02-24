#pragma once

#include <list>
#include <exception>
#include <format>
#include <atomic>

#include "OrderType.h"
#include "Side.h"
#include "Usings.h"
#include "Constants.h"
#include <memory>

enum class OrderStatus
{
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

    std::atomic<Price> price_; // needs to change for market to GTC
    std::atomic<Quantity> remainingQuantity_;
    std::atomic<OrderStatus> status_{OrderStatus::Active};
    std::atomic<uint32_t> version_{0};
    std::atomic<uint64_t> lastUpdatetime_;

    std::atomic<LockFreeOrder*> next_{nullptr};

    LockFreeOrder(OrderType orderType, OrderId orderId, Side side,
                    Price price, Quantity quantity)
        :orderId_(orderId),
        side_(side),
        orderType_(orderType),
        price_(price),
        initialQuantity_(quantity),
        remainingQuantity_(quantity)
    { }


    LockFreeOrder(OrderId orderid, Side side, Quantity quantity)
        : LockFreeOrder(OrderType::Market, orderid, side, Constants::InvalidPrice, quantity)
    { }

    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const {
        return price_.load(std::memory_order_acquire);
    }
    OrderType GetOrderType() const { return orderType_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }

    Quantity GetRemainingQuantity() const {
        return remainingQuantity_.load(std::memory_order_acquire);
    }

    Quantity GetFilledQuantity() const { 
        return GetInitialQuantity() - GetRemainingQuantity();
    }
    
    bool IsFilled() const { 
        return GetRemainingQuantity() == 0;
    }

    bool TryFill(Quantity quantity){
        Quantity current = remainingQuantity_.load(std::memory_order_acquire);
        Quantity new_remaining;

        do{
            if (quantity > current){
                return false;
            }
            new_remaining = current - quantity;
        } while (!remainingQuantity_.compare_exchange_weak(
            current, new_remaining,
            std::memory_order_release,
            std::memory_order_acquire));
        return true;
    }

    bool FastFill(Quantity quantity){
        Quantity current = remainingQuantity_.load(std::memory_order_acquire);
        if(quantity > current){
            return false;
        }

        Quantity new_remaining = current - quantity;
        remainingQuantity_.store(new_remaining, std::memory_order_release);
        return true;
    }

    bool ConvertToGTC(Price price){
        if(orderType_ != OrderType::Market){
            return false;
        }
        price_.store(price, std::memory_order_release);
        version_.fetch_add(1, std::memory_order_release);
        return true;
    }

    LockFreeOrder* GetNext() const {
        return next_.load(std::memory_order_acquire);
    }

    void SetNext(LockFreeOrder* next){
        next_.store(next, std::memory_order_release);
    }

    bool CommpareAndSwapNext(LockFreeOrder* expected, LockFreeOrder* desired){
        return next_.compare_exchange_weak(
            expected, desired,
            std::memory_order_release,
            std::memory_order_acquire
        );
    }
};