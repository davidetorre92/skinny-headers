/*  skn_fh.h — header-only file handler
 *  
 */

#include <stdio.h>
#include <stdlib.h>


static char* fh__read_file(const char* filename, PrintMode* p)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        print_log(*p, "")
    }

}