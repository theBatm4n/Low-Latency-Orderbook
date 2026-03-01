#include "OrderbookLockFree.h"
#include <algorithm>
#include <stdexcept>

LockFreeOrderbook::LockFreeOrderbook() {
    currentBlock_ = new Block;
    currentBlock_->next = nullptr;
    allBlocks_.push_back(currentBlock_);
}

LockFreeOrderbook::~LockFreeOrderbook(){
    for (Block* block: allBlocks_){
        delete block;
    }
}

size_t LockFreeOrderbook::pricetoIndex(Price price) const {
    if(price < MIN_PRICE || price > MAX_PRICE){
        throw std::out_of_range("Price out of range");
    }
    return static_cast<size_t>((price - MIN_PRICE)/ TICK_SIZE);
}

LockFreeOrder* LockFreeOrderbook::allocateOrder(OrderType type, OrderId id, Side side,
        Price price, Quantity quantity){
    size_t index = nextOrderIndex_.fetch_add(1, std::memory_order_relaxed);
    
    if (index >= BLOCK_SIZE){
        Block* newBlock = new Block{};
        newBlock->next = currentBlock_;
        currentBlock_ = newBlock;
        allBlocks_.push_back(newBlock);
    
        index = 0;
        nextOrderIndex_.store(1, std::memory_order_relaxed);
    }

    LockFreeOrder* order = reinterpret_cast<LockFreeOrder*>(
        currentBlock_->orders + index * sizeof(LockFreeOrder));
    new (order) LockFreeOrder(type, id, side, price, quantity);
    return order;
}    

LockFreeOrderbook::OrderSlot& LockFreeOrderbook::getOrderSlot(OrderId id){
    size_t index = id & (ORDER_TABLE_SIZE -1); 
    return orderTable_[index];
}

void LockFreeOrderbook::addtoLevel(LockFreeOrder* order, PriceLevel& level) {
    LockFreeOrder* oldHead = level.head.load(std::memory_order_acquire);
    
    do {
        order->SetNext(oldHead);
    } while (!level.head.compare_exchange_weak(
        oldHead, order,
        std::memory_order_release,
        std::memory_order_acquire));
    
    level.totalQuantity.fetch_add(order->GetInitialQuantity(), std::memory_order_release);
    level.orderCount.fetch_add(1, std::memory_order_release);
}

LockFreeTrades LockFreeOrderbook::matchOrders(LockFreeOrder* incomingOrder) {
    LockFreeTrades trades;
    
    Side side = incomingOrder->GetSide();
    Price price = incomingOrder->GetPrice();
    Quantity remaining = incomingOrder->GetRemainingQuantity();
    
    auto& levels = (side == Side::Buy) ? asks_ : bids_;
    size_t startIdx = (side == Side::Buy) ? 0 : pricetoIndex(price);
    size_t endIdx = (side == Side::Buy) ? pricetoIndex(price) : NUM_LEVELS - 1;
    
    if (side == Side::Buy) {
        for (size_t i = startIdx; i <= endIdx && remaining > 0; ++i) {
            PriceLevel& level = levels[i];
            Quantity levelQty = level.totalQuantity.load(std::memory_order_acquire);
            
            if (levelQty > 0) {
                LockFreeOrder* current = level.head.load(std::memory_order_acquire);
                
                while (current && remaining > 0) {
                    if (current->GetOrderType() != OrderType::FillAndKill) {
                        Quantity fillQty = std::min(remaining, current->GetRemainingQuantity());
                        
                        if (current->TryFill(fillQty)) {
                            incomingOrder->TryFill(fillQty);
                            remaining -= fillQty;
                            
                            trades.push_back({
                                incomingOrder->GetOrderId(),
                                current->GetOrderId(),
                                static_cast<Price>(i) * TICK_SIZE + MIN_PRICE,
                                fillQty
                            });
                            
                            level.totalQuantity.fetch_sub(fillQty, std::memory_order_release);
                        }
                    }
                    current = current->GetNext();
                }
            }
        }
    } else {
        for (size_t i = endIdx; i >= startIdx && remaining > 0; --i) {
            PriceLevel& level = levels[i];
            Quantity levelQty = level.totalQuantity.load(std::memory_order_acquire);
            
            if (levelQty > 0) {
                LockFreeOrder* current = level.head.load(std::memory_order_acquire);
                
                while (current && remaining > 0) {
                    if (current->GetOrderType() != OrderType::FillAndKill) {
                        Quantity fillQty = std::min(remaining, current->GetRemainingQuantity());
                        
                        if (current->TryFill(fillQty)) {
                            incomingOrder->TryFill(fillQty);
                            remaining -= fillQty;
                            
                            trades.push_back({
                                current->GetOrderId(),
                                incomingOrder->GetOrderId(),
                                static_cast<Price>(i) * TICK_SIZE + MIN_PRICE,
                                fillQty
                            });
                            
                            level.totalQuantity.fetch_sub(fillQty, std::memory_order_release);
                        }
                    }
                    current = current->GetNext();
                }
            }
        }
    }
    
    return trades;
}

LockFreeTrades LockFreeOrderbook::addOrder(OrderType type, Side side, Price price, Quantity quantity) {
    OrderId id = nextOrderId_.fetch_add(1, std::memory_order_relaxed);
    
    LockFreeOrder* order = allocateOrder(type, id, side, price, quantity);
    
    // Register in lookup table
    OrderSlot& slot = getOrderSlot(id);
    slot.id.store(id, std::memory_order_release);
    slot.order.store(order, std::memory_order_release);
    
    // Add to appropriate price level
    size_t levelIdx = pricetoIndex(price);
    PriceLevel& level = (side == Side::Buy) ? bids_[levelIdx] : asks_[levelIdx];
    addtoLevel(order, level);
    
    // Try to match
    LockFreeTrades lockFreeTrades = matchOrders(order);
    return LockFreeTrades(lockFreeTrades.begin(), lockFreeTrades.end());
}

bool LockFreeOrderbook::cancelOrder(OrderId id) {
    OrderSlot& slot = getOrderSlot(id);
    
    if (slot.id.load(std::memory_order_acquire) != id) {
        return false;  // Order not found
    }
    
    LockFreeOrder* order = slot.order.load(std::memory_order_acquire);
    if (!order) {
        return false;
    }
    
    slot.id.store(0, std::memory_order_release);
    slot.order.store(nullptr, std::memory_order_release);
    
    return true;
}

Quantity LockFreeOrderbook::getBestBidQuantity() const {
    for (size_t i = bids_.size() - 1; i >= 0; --i) {
        Quantity qty = bids_[i].totalQuantity.load(std::memory_order_acquire);
        if (qty > 0) return qty;
    }
    return 0;
}

Quantity LockFreeOrderbook::getBestAskQuantity() const {
    for (size_t i = 0; i < asks_.size(); ++i) {
        Quantity qty = asks_[i].totalQuantity.load(std::memory_order_acquire);
        if (qty > 0) return qty;
    }
    return 0;
}

Price LockFreeOrderbook::getBestBidPrice() const {
    for (size_t i = bids_.size() -1 ; i > 0; --i){
        Quantity qty = bids_[i].totalQuantity.load(std::memory_order_acquire);
        if (qty > 0){
            return MIN_PRICE + (i * TICK_SIZE);
        }
    }
    Quantity qty = bids_[0].totalQuantity.load(std::memory_order_acquire);
    if (qty > 0) return MIN_PRICE;
    return -1;  
}

Price LockFreeOrderbook::getBestAskPrice() const {
    for (size_t i = 0; i < asks_.size(); ++i) {
        Quantity qty = asks_[i].totalQuantity.load(std::memory_order_acquire);
        if (qty > 0) {
            return MIN_PRICE + (i * TICK_SIZE);
        }
    }
    return -1;  
}