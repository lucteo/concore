#include <concore/spawn.hpp>

int main() {
    concore::spawn_and_wait([] {
        printf("Hello, concurrent world!\n");
    });

    return 0;
}