#include "server.h"
#include <sys/stat.h>
#include <errno.h>


int main ()
{
    server server(7123);

    while (true) {
         server.update();
    }

    return 0;
}
