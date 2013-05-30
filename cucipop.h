/*$Id: cucipop.h,v 1.3 1997/08/01 01:00:33 srb Exp $*/

void castlower(char*string);
void blocksignals(void);
void setsignals(void);

extern FILE*sockin;
extern long spoofcheck;

#define STRLEN(x)	(sizeof(x)-1)
