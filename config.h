/*$Id: config.h,v 1.20 1998/05/12 21:09:14 srb Exp $*/

#define USEdot_lock	/**/
/*#define USEfcntl_lock /**/	    /* to test which combinations make sense */
/*#define USElockf	/**/	      /* run the lockingtest program part of */
#define USEflock	/**/		/* the procmail installation process */

/*#define SHADOW_PASSWD /**/		  /* shadow password library support */

#define USER		/**/			  /* comment out, to disable */
#define UIDL		/**/		  /* the optional command completely */
/*#define APOP            /**/
#define TOP		/**/

#define LAST_HACK	/**/		   /* uncomment to enable dummy LAST */
							 /* violates RFC1939 */

/* to change the MAILSPOOLDIR, edit authenticate.c */

/************************************************************************
 * Only edit below this line if you *think* you know what you are doing *
 ************************************************************************/

#define LOG_FACILITY	LOG_MAIL
#define PIDFILE		"/var/run/%s.pid"
#define FROM		"From "			 /* to separate the messages */
#define XFROM		"X-From_: "  /* use From_: and M$Exchange falls over */
#define ROOTDIR		"/"	    /* this is where we go when we daemonise */
#define POP3_PORT	110			       /* from /etc/services */
#define TIMEOUT		(10*60)		 /* minimum as specified by RFC 1939 */
typedef unsigned long uidl_t;			    /* for the UIDL wordsize */
#define LOCKSLEEP	4
#define LOWEST_UID	1	   /* decrease to allow root to pick up mail */
						       /* without using APOP */
#define QUIET		'q'
#define PORT		'p'
#define DEBUG		'd'
#define AUDIT		'a'
#define FVERSION	'v'
#define SABOTAGE_UIDL	'S'
#define AUTODELETE	'D'
#define EXPIRE		'E'
#define BERKELEYFORMAT	'Y'			      /* nix Content-Length: */
#define USERF		'P'
#define UIDLF		'U'
#define APOPF		'A'
#define TOPF		'T'
#define HELP1		'h'
#define HELP2		'?'
#define USAGE		"Usage: %s [-vqaYdPUSDAT] [-E age] [-p port]\n"
#define XUSAGE		\
 "\t-v\tdisplay the version number and exit\
\n\t-q\tquiet\
\n\t-a\tturn on auditing\
\n\t-d\tinteractive debug mode\
\n\t-P\tdisable optional USER and PASS command\
\n\t-U\tdisable optional UIDL command\
\n\t-S\tsabotage UIDL command (violates RFC1939)\
\n\t-D\tautodelete RETReived messages (violates RFC1939)\
\n\t-E age\texpire messages through -S or -D older than age only\
\n\t-A\tdisable optional APOP command\
\n\t-T\tdisable optional TOP command\
\n\t-p port\tspecify port other than %d\n"

/*#define LOG_FAILED_PASSWORD			     /* log failed passwords */

#define COPYBUF		32768			/* when doing in-situ copies */
#define GROWSTEP	16
#define LOCKRACE	32
#define SPOOFMASK	0xffff
#define AGETOLERANCE	8192	       /* to allow for incorrect system time */
#define BSIZECATEGORY	16384
#define DIRECTFETCHAGE	32768

#define CONTLENGTH	"content-length:"
#define STATUSFIELD	"status:"
#define XSTATUSFIELD	"x-status:"
#define ARGLEN		40			 /* as specified by RFC 1939 */
#define UIDLLEN		70					/* same here */
#define LINEBUFLEN	(4+1+(ARGLEN+1)*2)    /* maximum command line length */
#define LISTEN_BACKLOG	128
#define TCP_PROT	6			      /* from /etc/protocols */

#define MAXBULLETINS	64
#define MAXSTATEAGE	8388608				       /* > 3 months */
#define MEMORY_CACHE	(64*1024)
#define CUCIPOP_LIB	"/var/spool/cucipop"
#define STATE_DB	"state.db"
#define BULLETINS_PATH	CUCIPOP_LIB"/bulletins"
