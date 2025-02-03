#include "shm.hpp"

#include <iostream>

struct data {
    double ts_;
    int v_;

    double ts() const { return ts_; }
};

int main() {
    try {
        auto shm = std::move(shared_memory<data>::create("/test_shm"));
        std::string opt;
        while (std::cin >> opt) {
            if (opt == "break") {
                break;
            } else if (opt == "append") {
                double ts;
                int v;
                std::cin >> ts >> v;
                shm->append(data{ts, v});
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}