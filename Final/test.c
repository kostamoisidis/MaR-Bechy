#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main ( void )
{
    char a[] = "aa";
    char b[] = "aa";

    printf( "%d\n", ! strncmp( a, b, 5 ) );
    
    return 0;
}