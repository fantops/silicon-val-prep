#include<stdio.h>
#include<stdint.h>
#include<stddef.h>

void swap(uint32_t *p, uint32_t *q){
    *p = *p + *q; *q=*p-*q; *p= *p-*q;
}

unit32_t my_strlen(char *s){
    uint32_t count;
    uint32_t i =0;
    while(s[i++]){
        count++;
    }
    return count;
}

void *mem_copy(void *dst,const void *src, size_t n){
    unsigned char *d = (unsigned char *)dst; 
    const unsigned char *s =(unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}


