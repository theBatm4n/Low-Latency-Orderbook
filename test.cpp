#include "OrderbookLockFree.h"
#include <iostream>

int main() {
    LockFreeOrderbook book;
    
    auto trades = book.addOrder(OrderType::GoodTillCancel, 
                                 Side::Buy, 100, 1000);
    
    std::cout << "Order added, trades: " << trades.size() << std::endl;
    return 0;
}