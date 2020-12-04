#define OK 1
#define NIJEOK 0


#define FULL_SERVER 0
#define INTRO 1
#define WAIT 2
#define KORD 3
///koordinate saljem kao "x y"
#define WRONG_INPUT 4
#define READY 5
#define POTEZ 6
#define OPP 7
#define ISHOD 8
#define NOTI 9

///oznake u matrici
#define BOMBA '*'
#define EMPTY '.'

#define IP_len 30

int primiPoruku(int sock, int *vrsta_poruke, char **poruka);
int posaljiPoruku(int sock, int vrsta_poruke, const char *poruka);

