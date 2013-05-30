/************************************************************************
 *	Routine to convert from ASCII to time_t format			*
 *									*
 *	Copyright (c) 1997, S.R. van den Berg, The Netherlands		*
 *	#include "README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
 "$Id: atotime.c,v 1.5 1998/05/02 00:56:54 srb Exp $";
#endif
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "atotime.h"

static unsigned dec(const char*const s)
{ return strtol(s,(char**)0,10);
}

time_t atotime(const char*in)
{ struct tm tin;
  ;{ unsigned m;
     static const char month[][3]={"Jan","Feb","Mar","Apr","May","Jun","Jul",
      "Aug","Sep","Oct","Nov"};
     for(m=0;m<11&&strncmp(in,month[m],3);m++);
     tin.tm_mon=m;
   }
  tin.tm_mday=dec(in+=4);tin.tm_hour=dec(in+=3);tin.tm_min=dec(in+=3);
  tin.tm_sec=dec(in+=3);tin.tm_year=dec(in+=3)-1900;
  return mktime(&tin);
}

time_t atosec(const char*in,const char**const innew)
{ const char*p;time_t t;
  for(t=0;;)
   { static const char scale[]="sSmmhHdDwWMMyY";
     static time_t tscale[]={1,60,60*60,60*60*24,60*60*24*7,
      60*60*24*(365*4+1)/4/12,60*60*24*(365*4+1)/4};
     time_t i=strtol(p=in,(char**)&in,10);
     if(p!=in&&*in&&(p=strchr(scale,*in)))
	in++,t+=i*tscale[(p-scale)/2];
     else
      { *innew=in;
	return t+i;
      }
   }
}
