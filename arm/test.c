#include <stdio.h>
#include "p2linux.h"

int main()
{
    int error; 
    unsigned long ptid, nbuf;
    void* seg_addr;
    
    seg_addr = ts_malloc(100);
    error = pt_create("pt1", seg_addr, 0, 100, 10, PT_NODEL, &ptid, &nbuf);
    printf("%d\n", ptid);
    
    seg_addr = ts_malloc(100);
    error = pt_create("pt2", seg_addr, 0, 100, 10, PT_NODEL, &ptid, &nbuf);
    printf("%d\n", ptid);
    
    seg_addr = ts_malloc(100);
    error = pt_create("pt3", seg_addr, 0, 100, 10, PT_NODEL, &ptid, &nbuf);
    printf("%d\n", ptid);
    return 0;
}
