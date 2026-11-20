static void put(uint8_t *p,unsigned off,uint32_t value){
  for(unsigned i=0;i<4;i++)p[off+i]=(uint8_t)(value>>(8u*i));
}
static void ptr(uint8_t *p,unsigned off,const void *value){put(p,off,(uintptr_t)value);}
static void copy(uint8_t *a,const uint8_t *b,unsigned n){for(unsigned i=0;i<n;i++)a[i]=b[i];}
#define CHECK(expr) do {if(!(expr))return __LINE__;} while(0)
