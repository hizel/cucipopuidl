/*$Id: dbops.h,v 1.3 1998/05/13 16:57:40 srb Exp $*/

struct stateinfo {
 time_t lastsuccess,lastabort;
 int brokenstate;
 unsigned char readbulletin[MAXBULLETINS/8];
};

extern const char statedbfile[],cplib[];

void initappdb();
void exitappdb();
void opendb();
void closedb();
struct stateinfo*getstate(const char*key,time_t curt);
void putstate(const char*key,const struct stateinfo*state);
