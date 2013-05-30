/************************************************************************
 *	cucipop - The Cubic Circle POP3 daemon implementation		*
 *									*
 *	A fully compliant RFC1939 POP3 daemon implementation.		*
 *	Features: light load, fast query path, NFS resistant,		*
 *	in-situ mailbox rewrite, no tempfiles, short locking,		*
 *	standard BSD mailbox format, SysV Content-Length extension	*
 *									*
 *	Copyright (c) 1996-1998, S.R. van den Berg, The Netherlands	*
 *	#include "README"						*
 ************************************************************************/
#ifdef RCS
static /*const*/char rcsid[]=
    "$Id: cucipop.c,v 1.31 1998/05/13 16:57:39 srb Exp $";
#endif

#include "patchlevel.h"
#include "config.h"

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <fcntl.h>
#include <string.h>
#include <pwd.h>
#include <syslog.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#ifdef INET6
#include <netdb.h>
#endif

#include "cucipop.h"
#include "atotime.h"
#include "dbops.h"
#include "hsort.h"

#ifdef APOP
#include <md5.h>
#endif

#define MD5_DIGLEN	16
#define BIN2HEXLEN	(MD5_DIGLEN*2)
#define Signal(sig,fn)	signal(sig,(void(*)())(fn))
#define Vuser		(auth_logname?auth_logname:user)

#define Bintohex(arg,checkendian)	(sizeof(arg)>MD5_DIGLEN?\
 (fail(#arg" too large"),terminate(EX_SOFTWARE),""):\
 bintohex(&(arg),sizeof(arg),checkendian))

static const char*const nullp,From_[]=FROM,XFrom_[]=XFROM,cucipopn[]="cucipop",
                                      Received[]=
                                              "Received: by %s (mbox %s)\r\n (with Cubic Circle's %s (%s) %.24s)\r\n",
                                              version[]=VERSION,bulletins_path[]=BULLETINS_PATH;
static char linebuf[LINEBUFLEN+1],*arg1,*arg2,user[ARGLEN+1],*host,*timestamp;
static FILE*fmbox,*sockout;
static size_t hostnduser;
FILE*sockin;
long spoofcheck;
#ifdef INET6
static struct sockaddr_storage peername;
#else
static struct sockaddr_in peername;
#endif
static int namelen;

struct msg
{   size_t order;
    off_t start;
    int file;
    time_t mtime;
#ifdef UIDL
    uidl_t uidl;
    unsigned statskip;
#endif
    long virtsize,realsize;
    unsigned char deleted,tooold;
}*msgs;

#define MSGS(i) (msgs[msgs[i].order])

static size_t msgs_filled,priv_msgs_filled,msgs_max,totmsgs,transfmsgs;
static long totsize,transfsize;
static off_t oldend;
static unsigned audit,berkeleyformat;
static time_t tsabotage,tsessionstart;
static const char*mailbox;
static struct stateinfo si;

#define TBUL_READ(x)	(si.readbulletin[(x)/8]&1<<((x)&7))
#define SBUL_READ(x)	(si.readbulletin[(x)/8]|=1<<((x)&7))
#define SBUL_UNREAD(x)	(si.readbulletin[(x)/8]&=~(1<<((x)&7)))

static void zombiecollect(void)
{   while(waitpid((pid_t)-1,(int*)0,WNOHANG)>0);	      /* collect any zombies */
}

static size_t findmsg(void)
{   size_t i;
    return arg1&&(i=strtol(arg1,(char**)0,10)-1)<msgs_filled&&
           !(MSGS(i).deleted&1)?i+1:0;
}

static const char*bintohex(const void*ptr,size_t size,const int checkendian)
{   static char hex[BIN2HEXLEN+1],*hp;
    int inc=1;
    {   const int test=1;
        if(checkendian&&!*(const char*)&test)
            ptr=(const char*)ptr+size+(inc=-1);
    }
    for(hp=hex; size--; ptr=(const char*)ptr+inc,hp+=2)
        sprintf(hp,"%02x",*(unsigned char*)ptr);
    return hex;
}

#ifdef INET6
static const char *peer(void)
{
    static char hbuf[NI_MAXHOST];
#ifdef NI_WITHSCOPEID
    const int niflags = NI_NUMERICHOST | NI_WITHSCOPEID;
#else
    const int niflags = NI_NUMERICHOST;
#endif
    if (getnameinfo((struct sockaddr *)&peername, namelen, hbuf, sizeof(hbuf),
                    NULL, 0, niflags)) {
        strncpy(hbuf, "invalid", sizeof(hbuf) - 1);
        hbuf[sizeof(hbuf) - 1] = '\0';
    }
    return hbuf;
}
#else
static const char*iptoascii(const struct sockaddr_in*addr)
{   static char ip[4*4];
    unsigned char*p=(unsigned char*)&addr->sin_addr.s_addr;
    sprintf(ip,"%d.%d.%d.%d",p[0],p[1],p[2],p[3]);
    return ip;
}

static const char*peer(void)
{   return iptoascii(&peername);
}
#endif

void castlower(char*p)
{   for(; *p=tolower(*p); p++);
}

static int cmpbuf(const char*p)
{   return!strcmp(p,linebuf);
}

static int readcommand(unsigned autodel)
{   fflush(sockout);
    alarm(TIMEOUT);
    if(!fgets(linebuf,LINEBUFLEN+1,sockin))
    {   if(autodel)
            return 0;
    }
    alarm(TIMEOUT);
    arg2=0;
    if((arg1=strchr(linebuf,'\r'))||(arg1=strchr(linebuf,'\n')))
        *arg1='\0';
    if(arg1=strchr(linebuf,' '))
    {   *arg1++='\0';
        if(arg2=strchr(arg1,' '))
        {   *arg2++='\0';
            if(strlen(arg2)>ARGLEN)
                arg2[ARGLEN]='\0';
        }			    /* some silly popclient called E-mail connection */
        else if(!*arg1)				     /* adds spurious spaces */
        {   arg1=0;				   /* at the end of the command line */
            goto noarg1;
        }
        if(strlen(arg1)>ARGLEN)
            arg1[ARGLEN]='\0';
    }
noarg1:
    castlower(linebuf);
    return 1;
}

static void fail(const char*msg)
{   fprintf(sockout,"-ERR %s\r\n",msg);
}

static void loginvalcmd(void)
{   static const char noarg[]="-";
    syslog(LOG_DEBUG,"Invalid command %s %s %s",linebuf,arg1?arg1:noarg,
           arg2?arg2:noarg);
}

static void invalcmd(const char*cmds)
{   loginvalcmd();
    fprintf(sockout,"-ERR Invalid command, try one of: %s\r\n",cmds);
}

static void outofmem(void)
{   syslog(LOG_ALERT,"out of memory");
    if(sockout)
        fail("Out of memory");
}

void wipestring(char*string)
{   while(*string)
        *string++='\0';
}


static void corrupted(off_t offset)
{   syslog(LOG_CRIT,"mailbox %s corrupted at %ld",user,(long)offset);
    fail("Mailbox corrupted");
}

static void addblock(const off_t start)
{   if(msgs_filled==msgs_max&&
            !(msgs=realloc(msgs,((msgs_max+=GROWSTEP)+1)*sizeof*msgs)))
        outofmem();
    msgs[msgs_filled].order=msgs_filled;
    msgs[msgs_filled].start=start;
    msgs[msgs_filled].virtsize=0;
    msgs[msgs_filled++].deleted=0;
}

char*bullname(int bullno)
{   static char buf[4];
#if MAXBULLETINS>1000
#error Increase the buf array length if you want to do this
#endif
    sprintf(buf,"%02d",bullno);
    return buf;
}

FILE*openbulletin(const int extrafile,FILE*fp)
{   if(extrafile>=0)
        fp=fopen(bullname(extrafile),"rb");
    return fp;
}

static void scanfile(const int extrafile,FILE*fp,const time_t agecutoff)
{   size_t virtsize,extra,omf;
    int fdsockin;
    off_t start,end;
    uidl_t uidl;
    long clen;
    unsigned inheader,statskip;
    clen=end=start=statskip=0;
    fdsockin=-1;
    omf=msgs_filled;
    virtsize=extra=STRLEN(XFrom_)-STRLEN(From_)+STRLEN(Received)+hostnduser-2*2+
                   STRLEN(cucipopn)-2+STRLEN(version)-2+24-5;
    if(!(fp=openbulletin(extrafile,fp)))
        if(extrafile<0)
            goto nomailbox;
        else
            return;
    if(extrafile<0)
        end=oldend,fdsockin=fileno(sockin);
    else
    {   struct stat stbuf;
        fstat(fileno(fp),&stbuf);
        end=stbuf.st_size;
    }
    fcntl(fdsockin,F_SETFL,(long)O_NONBLOCK);	/* non-blocking spoof enable */
    long pro;
    for(;;)
    {   unsigned newlines=1; if(!(++pro % 300)) fprintf(stderr, ".");
        if(clen<=0)
        {   const char*p;
            int c;
            for(p=From_;;)
            {   if(!end)
                    goto nomailbox;
                end--;
                if(*p!=(c=getc(fp)))
                {   ungetc(c,fp);
                    end++;
                    break;
                }
                newlines=2;
                if(!*++p)
                {   struct msg*msp;
                    inheader=1;
                    addblock(start);
                    (msp=msgs+msgs_filled-1)->deleted=0;
                    msp->tooold=1;
                    msp->file=extrafile;
                    msp->mtime=agecutoff;
                    if(msgs_filled-omf>1)
                    {   msp--;
                        totsize+=msp->virtsize=virtsize+
                                               (msp->realsize=start-msp->start);
#ifdef UIDL
                        msp->uidl=uidl;
                        msp->statskip=statskip;
#endif
                    }
                    statskip=0;
                    uidl=tsabotage;
                    virtsize=extra;
#define DATELEN 20
                    if(end>DATELEN+4)
                    {   char circ[DATELEN],circo[DATELEN+1];
                        unsigned rot,frot;
                        for(circo[DATELEN]=frot=rot=0;; rot=(rot+1)%DATELEN)
                        {   if(!end)
                                goto nomailbox;
                            end--;
                            virtsize++;
                            start++;
                            if((c=getc(fp))=='\n')
                                break;
                            circ[rot]=c;
                            frot++;
                        }
                        if(frot>=DATELEN)
                        {   for(frot=0; frot<DATELEN; frot++,rot=(rot+1)%DATELEN)
                                circo[frot]=circ[rot];
                            if(extrafile<0)
                            {   time_t mtime;
                                struct msg*msp=msgs+msgs_filled-1;
                                if((mtime=atotime(circo))-agecutoff>=0)
                                {   if(tsabotage)
                                        uidl=0;
                                    msp->tooold=0;
                                }
                                msp->mtime=mtime;
                            }
                        }
                    }
                    break;
                }
            }
            start+=p-From_;
        }
nomailbox:
        ;
        {
            off_t oend;
            register int c;
            int ololo=0;
            oend=end;
            do
            {   
               
                if(!end--)
                    goto thatsall;
                if(!inheader)
                    clen--;
                c=getc(fp);
                if(spoofcheck>=0&&!(++spoofcheck&SPOOFMASK))
                {   char c;
                    spoofcheck=0;
                    switch(read(fdsockin,&c,1))
                    {
                    case 1:
                        spoofcheck=-1;
                        ungetc(c,sockin);
                        break;
                    case 0:
                        goto thatsall;
                    }
                }
trynl:
#ifdef UIDL
                uidl=uidl*67067L+(unsigned char)c;
#endif
                if(c=='\n')
isnl:	    {
                    virtsize++;
                    if(--newlines&&inheader)
                    {   static const char contlength[]=CONTLENGTH,
                                                       statfield[]=STATUSFIELD,xstatfield[]=XSTATUSFIELD;
                        const char*p,*ps,*psx;
                        unsigned skiplen;
                        uidl_t luidl;
                        p=berkeleyformat?0:contlength;
                        ps=statfield;
                        psx=xstatfield;
                        for(skiplen=0,luidl=uidl;;)
                        {   if(!end)
                                goto thatsall;
                            end--;
                            c=getc(fp);
                            if(p&&*p!=tolower(c))
                                p=0;
                            if(ps&&*ps!=tolower(c))
                                ps=0;
                            if(psx&&*psx!=tolower(c))
                                psx=0;
                            if(!p&&!ps&&!psx)
                            {   uidl=luidl;
                                goto trynl;
                            }
#ifdef UIDL
                            luidl=luidl*67067L+(unsigned char)c;
#endif
                            newlines=2;
                            skiplen++;
                            if(ps&&!*++ps||psx&&!*++psx)
                            {   statskip+=skiplen+1;
                                do
                                {   if(!end)
                                        goto thatsall;
                                    end--;
                                    statskip++;
                                }
                                while(fgetc(fp)!='\n');
                                goto isnl;
                            }
                            if(p&&!*++p)
                            {   for(;;)
                                {   if(!end)
                                        goto thatsall;
                                    end--;
                                    switch(c=fgetc(fp))
                                    {
                                    case ' ':
                                    case '\t':
                                        continue;
                                    }
                                    break;
                                }
                                for(clen=0;; end--,c=fgetc(fp))
                                {   if(c<'0'||c>'9')
                                        goto trynl;
                                    clen=((unsigned long)clen*10)+c-'0';
                                    if(!end)   /* memory requirements to a perfect fit */
thatsall:		   {
                                        struct msg*msp;
                                        fcntl(fdsockin,F_SETFL,0L);      /* block again */
                                        if(clen>0)
                                            syslog(LOG_WARNING,"%s mailbox %ld short",
                                                   mailbox,(long)clen);
                                        msp=msgs+msgs_filled;
                                        start+=oend;
                                        msp->deleted=1;
                                        if(msgs_filled>omf)
                                        {   msp--;
                                            totsize+=msp->virtsize=virtsize+
                                                                   (msp->realsize=start-msp->start);
#ifdef UIDL
                                            msp->uidl=uidl;
                                            msp->statskip=statskip;
#endif
                                        }
                                        if(extrafile>=0)
                                            fclose(fp);
                                        return;
                                    }
                                }
                            }
                        }
                    }
                    if(!newlines&&!inheader&&clen>0)
                        newlines=1;
                }
                else
                    newlines=2;
            }
            while(newlines);
            inheader=0;
            start+=oend-end;
        }
    }
}

static int mcmp(const struct msg*a,const struct msg*b)
{   a=msgs+a->order;
    b=msgs+b->order;
    switch(si.brokenstate)
    {
    case 0:
        return -(a->order<b->order);
    case 1:
        return -(a->realsize<b->realsize);
    default:
        if(tsessionstart-a->mtime<DIRECTFETCHAGE||
                tsessionstart-b->mtime<DIRECTFETCHAGE)
            return -(a->mtime>b->mtime);
    case 2:
    {   unsigned sa=a->realsize/BSIZECATEGORY,sb=b->realsize/BSIZECATEGORY;
        return sa<sb?-1:sa>sb?1:-(a->mtime>b->mtime);
    }
    }
}

static void mswap(struct msg*a,struct msg*b)
{   int t;
    t=a->order;
    a->order=b->order;
    b->order=t;
}

void checkbulletins()
{   if(!chdir(bulletins_path))
    {   struct stat stbuf;
        static time_t lastmods;
        static int nextextrafile=-1;
        if(nextextrafile>=0)
        {   if(!stat(bullname(nextextrafile),&stbuf)&&stbuf.st_mtime>lastmods)
                goto scanem;
        }
        else
        {   int bul;
scanem:
            for(msgs_filled=0,bul=MAXBULLETINS; bul--;)
                if(!stat(bullname(bul),&stbuf))
                {   time_t mtime=stbuf.st_mtime;
                    if(!lastmods||mtime-lastmods>0)
                        lastmods=mtime;
                    scanfile(bul,0,mtime);
                }
            priv_msgs_filled=msgs_filled;
        }
        if(nextextrafile<=0)
            nextextrafile=MAXBULLETINS;
        nextextrafile--;
    }
    chdir(ROOTDIR);
}

static int rread(const int fd,char*a,const unsigned len)
{   int i;
    size_t left=len;
    do
        while(0>(i=read(fd,a,left))&&errno==EINTR);
    while(i>0&&(a+=i,left-=i));
    return len-left;
}

static int rwrite(const int fd,char*a,const unsigned len)
{   int i;
    size_t left=len;
    do
        while(0>(i=write(fd,a,left))&&errno==EINTR);
    while(i>0&&(a+=i,left-=i));
    return len-left;
}

#ifdef UIDL
static void printuidl(size_t i)
{   size_t sz=MSGS(i).virtsize-hostnduser-MSGS(i).statskip;
    (sizeof MSGS(i).uidl+sizeof MSGS(i).virtsize)*2>UIDLLEN?
    fail("uidl too long"),terminate(EX_SOFTWARE),0:0;
    fprintf(sockout,"%ld %s",(long)(i+1),Bintohex(MSGS(i).uidl,1));
    fprintf(sockout,"%s\r\n",Bintohex(sz,1));
}
#endif

void printusg(void)
{   fprintf(stderr,USAGE,cucipopn);
}

main(int argc,const char*const argv[])
{
    FILE *fp;
    time_t agecutoff=-AGETOLERANCE;
    int i;
    if(argc != 2) exit(0);
    sockin=stdin;
    sockin=stdout;
    fprintf(stderr, "open %s\n", argv[1]);
    if(!(fp = fopen(argv[1],"r+b"))) {
        fprintf(stderr, "fail open %s\n", argv[1]);
        exit(1);
    }ungetc(fgetc(fp),fp); 
    { struct stat stbuf;int fd;fstat(fd=fileno(fp),&stbuf);oldend=stbuf.st_size;
    }
    fprintf(stderr, "scan file %s\n", argv[1]);
    scanfile(-1,fp,agecutoff);
    fprintf(stderr, "hsort file %s\n", argv[1]);
    if(si.brokenstate>0)
        hsort(msgs,msgs_filled,sizeof*msgs,mcmp,mswap);
    totmsgs=msgs_filled;
    fprintf(stderr, "total msgs: %d\n", msgs_filled);
    for(i=0; i<msgs_filled; i++)
        if(!(MSGS(i).deleted&1)) {
            size_t sz=MSGS(i).virtsize-hostnduser-MSGS(i).statskip;
            printf("%ld %s",(long)(i+1),Bintohex(MSGS(i).uidl,1));
            printf("%s\r\n",Bintohex(sz,1));
        }

    fclose(fp);
}
