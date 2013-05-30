/*$Id: hsort.h,v 1.3 1998/05/11 14:26:40 srb Exp $*/

#ifndef P
#define P(x)	x
#define Q(x)	()
#endif

void hsort Q((void*base,size_t nelem,size_t width,
 int(*fcmp)(const void*,const void*),void(*fswap)(const void*,const void*)));
