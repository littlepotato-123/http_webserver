#include <unistd.h>
#include "server.h"
int main() {
    
    Server server(9527, 3, 60000, 0, 
    3306,"root", "Root12345", "yourdb",12,
    8);
    server.Start(); 

    //printf("a\n");
}