# High-Performance Lock-Free Orderbook

A low-latency C++ orderbook implementation achieving <50μs per trade latency through lock-free data structures and custom memory management.

## Overview

This project implements a financial orderbook that matches buy and sell orders with extreme low latency. The design evolved from a simple thread-safe implementation to a high-performance lock-free architecture suitable for HFT (High-Frequency Trading) systems.

## Evolution of Design

### V1: Single-Threaded Baseline
The initial implementation used straightforward C++ containers and mutex-based thread safety:

**Characteristics:**
- `std::map` for price levels (O(log n) lookup)
- `std::unordered_map` for order lookup
- `std::shared_ptr<Order>` for order management
- `std::list<OrderPointer>` for orders at each price level
- `std::mutex` for thread safety
- Separate background thread for order pruning


**Limitations:**
- Lock contention under load
- Cache misses from pointer chasing
- Dynamic allocations during trading
- Poor scalability with multiple threads

### V2: Lock-Free Architecture
Completely redesigned for in multi-threaded environments:

**Key Improvements:**

#### 1. Lock-Free Data Structures
- Removed all `std::mutex` instances
- Atomic operations for thread coordination
- Single-writer principle for critical paths

#### 2. Fixed-Size Arrays Replace Maps
```cpp
// Before: std::map<Price, OrderPointers>
// After:  std::array<PriceLevel, NUM_LEVELS>

static constexpr Price MIN_PRICE = 0.0;
static constexpr Price MAX_PRICE = 500.0;
static constexpr Price TICK_SIZE = 0.01;
static constexpr size_t NUM_LEVELS = 50000;
```

O(1) direct access vs O(log n) tree traversal

Cache-local memory layout

Predictable performance

3. Custom Memory Pool
cpp
struct Block {
    LockFreeOrder orders[1024];  // Pre-allocated
    Block* next;
};
Zero allocations during trading

Contiguous memory layout

20x faster order creation

4. Lock-Free Order Class
cpp
```
class alignas(64) LockFreeOrder {
    const OrderId orderId_;             
    std::atomic<Quantity> remaining_;    
    std::atomic<LockFreeOrder*> next_;   
};
```
4. Ring Buffer for Order Submission
cpp
```
template<typename T, size_t Capacity>
class LockFreeRingBuffer {
    std::array<T, Capacity> buffer_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
    // Lock-free producer/consumer
};
```
Decouples order submission from processing

No blocking between threads

Batch processing capability
