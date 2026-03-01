#include "OrderbookLockFree.h"
#include "OrderLockFree.h"
#include <iostream>

int main() {
    LockFreeOrderbook book;
    
    auto trades1 = book.addOrder(OrderType::GoodTillCancel, 
                                 Side::Buy, 100, 1000);
    
    std::cout << "Buy order added, trades: " << trades1.size() << std::endl;

    auto trades2 = book.addOrder(OrderType::GoodTillCancel, 
                                 Side::Sell, 100, 500);
    std::cout << "Sell order added, trades: " << trades2.size() << std::endl;

    std::cout << "Best bid price: " << book.getBestBidPrice() 
              << " Qty: " << book.getBestBidQuantity() << std::endl;
    std::cout << "Best ask price: " << book.getBestAskPrice() 
              << " Qty: " << book.getBestAskQuantity() << std::endl;
    
    return 0;
}