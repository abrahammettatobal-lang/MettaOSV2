#include <unistd.h>

int main(void)
{
    static const char m[] = "dyn ok\n";

    if (write(1, m, 7) != 7)
        return 1;
    return 0;
}
