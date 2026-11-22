#include <tp/thread_pool.hpp>

int main() {
    tp::ThreadPool pool(tp::Config{}.workers(2));
    auto f = pool.submit([](int a, int b) { return a + b; }, 2, 3);
    return f.get() == 5 ? 0 : 1;
}
