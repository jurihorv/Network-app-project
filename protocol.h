#define OK 1
#define NIJEOK 0

#define INTRO 1
#define FULL_SERVER 0
#define WAIT 2
#define HOR 3
#define VERT 4
#define KORD 5
///koordinate saljem kao "x y"
#define WRONG_INPUT 6
#define READY 7
#define POTEZ 8
#define OPP 9
#define ISHOD 10
#define NOTI 11
#define BROD 'A'
#define BOMBA '*'
#define EMPTY '.'

#define IP_len 30


int primiPoruku(int sock, int *vrsta_poruke, char **poruka);
int posaljiPoruku(int sock, int vrsta_poruke, const char *poruka);

