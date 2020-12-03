#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <pthread.h>

#include "protocol.h"

#define merror(s) printf(s); exit(0)

#define N_dretve 3
#define N_igraca 6
#define N_name 15
#define IP_len 30 ///duljina rezervirana za dekadsku adresu

typedef unsigned int uint;

typedef struct{
  int commSocketPrvi;
  int commSocketDrugi;
  int thread_index;
  char *adresa_prvi;
  char *adresa_drugi;
} igra_info;

typedef struct{
  int soba, mapa;
  int socket;
} popuni_info;

pthread_mutex_t lokot_dretve = PTHREAD_MUTEX_INITIALIZER;

int active[N_dretve];
igra_info parametri[N_dretve];
popuni_info param[2 * N_dretve]; ///ovo je za istovremeno postavljanje brodova na plocu

char ploca[N_dretve][2][10][10];  ///u svakoj sobi postavim 2 ploce na stol

int inside(int x, int y){
  if(x >= 10 || y >= 10 || x < 0 || y < 0) return 0;
  return 1;
}

int check(int soba, int mapa, int x, int y, int brod){
  x--; y--; ///input je 1 indeksiran, ja 0
  if(brod == 3){
    int delta = 0;
    for(; delta <= 2; delta++){
      int tx = x + delta, ty = y;
      if(!inside(tx, ty) || ploca[soba][mapa][tx][ty] != EMPTY) return 0;
    }
    return 1;
  }

  else if(brod == 2){
    int delta = 0;
    for(; delta <= 1; delta++){
      int tx = x, ty = delta + y;
      if(!inside(tx, ty) || ploca[soba][mapa][tx][ty] != EMPTY) return 0;
    }

    return 1;
  }

  return 0; ///bezveze
}

void *popuniPlocu(void *parametar_popuni){
    popuni_info *opis = (popuni_info *) parametar_popuni;
    int sock = opis -> socket;
    int mapa = opis -> mapa;
    int soba = opis -> soba;
    int trice = 0, type;
    char *get;
    while(trice != 2){
      posaljiPoruku(sock, HOR, "koordinate lijevog kraja 1x3 horizontalnog broda? (10x10 je ploca)\n");
      primiPoruku(sock, &type, &get);
      int x, y;
      int c = sscanf(get, "%d %d", &x, &y);
      if(c != 2 || type != KORD){
        posaljiPoruku(sock, WRONG_INPUT, "krivi format ulaza, unesite ponovo\n");
        continue;
      }
      int moj_x = ntohl(x);
      int moj_y = ntohl(y);
      x = moj_x;
      y = moj_y;

      if(!check(soba, mapa, x, y, 3)){
        posaljiPoruku(sock, WRONG_INPUT, "krive koordinate, unesite ponovo\n");
        continue;
      }

      x--; y--;
      int d = 0;
      for(; d < 3; d++)
        ploca[soba][mapa][x][y + d] = BROD;
      trice++;
    }


    int dvice = 0;
    while(dvice != 2){
      posaljiPoruku(sock, HOR, "koordinate gornjeg kraja vertikalnog 2x1 broda? (10x10 je ploca)\n");
      primiPoruku(sock, &type, &get);
      int x, y;
      int c = sscanf(get, "%d %d", &x, &y);
      if(c != 2){
        posaljiPoruku(sock, WRONG_INPUT, "krivi format ulaza, unesite ponovo\n");
        continue;
      }

      int moj_x = ntohl(x);
      int moj_y = ntohl(y);
      x = moj_x;
      y = moj_y;

      if(!check(soba, mapa, x, y, 2)){
        posaljiPoruku(sock, WRONG_INPUT, "krive koordinate, unesite ponovo\n");
        continue;
      }

      x--; y--;
      int d = 0;
      for(; d < 2; d++){
        ploca[soba][mapa][x + d][y] = BROD;
      }
      dvice++;
    }
}

void *pokreniIgru(void *parametar){
  igra_info *igra = (igra_info *) parametar;
  int uticnice[2];
  uticnice[0] = igra -> commSocketPrvi;
  uticnice[1] = igra -> commSocketDrugi;
  char uvod[100];
  sprintf(uvod, "Igrate protiv igraca na racunalu %s\n", igra -> adresa_drugi);
  posaljiPoruku(uticnice[0], INTRO, uvod);
  sprintf(uvod, "igrate protiv igraca na racunalu %s\n", igra -> adresa_prvi);
  posaljiPoruku(uticnice[1], INTRO, uvod);

  ///postavljajte tablice molim
  ///prvo ocistim ploce u ovoj sobi
  int i;
  for(i = 0; i < 2; i++)
    memset(ploca[igra -> thread_index][i], '.', sizeof(ploca[igra -> thread_index][i]));

  char *get;
  int type;
  ///ovdje cu uvijek naci slobodnu dretvu jer ih imam 2 * br_soba, tako da svaki igrac ima svoju
  int prva, druga;
  prva = 2 * igra -> thread_index;
  druga = 2 * igra -> thread_index + 1;
  pthread_t dret[2];

  param[prva].soba = igra -> thread_index;
  param[prva].mapa = 0;
  param[prva].socket = igra -> commSocketPrvi;
  pthread_create(&dret[0], NULL, popuniPlocu, &param[prva]);

  param[druga].soba = igra -> thread_index;
  param[prva].mapa = 1;
  param[prva].socket = igra -> commSocketDrugi;
  pthread_create(&dret[1], NULL, popuniPlocu, &param[druga]);

  pthread_join(dret[0], NULL);
  pthread_join(dret[1], NULL);


  ///pocinje borba

}

int main(int argc, char **argv){
  //perror("socket");
  if(argc != 2){
    merror("Program za pokretanje treba jos parametara\n");
  }

  int port;
  sscanf(argv[1], "%d", &port);

  int listenerSocket = socket(PF_INET, SOCK_STREAM, 0);
  if(listenerSocket == -1){
    perror("socket");
    ///exit(0);
  }

  struct sockaddr_in moja_adresa;
  moja_adresa.sin_family = AF_INET;
  moja_adresa.sin_port = htons(port);
  moja_adresa.sin_addr.s_addr = INADDR_ANY;
  memset(moja_adresa.sin_zero, '\0', 8);

  if(bind(listenerSocket, (struct sockaddr *) &moja_adresa, sizeof(moja_adresa)) == -1){
    perror("bind");
    exit(0);
  }

  if(listen(listenerSocket, N_dretve) == -1){
    perror("listen");
  }

  pthread_t dretve[N_dretve];
  memset(active, 0, sizeof(active)) ;


  char *ready_user;
  ready_user = (char*)malloc(sizeof(char) * IP_len);

  int ready_user_socket;

  memset(ready_user, '\0', sizeof(ready_user));

  while(1){
    struct sockaddr_in klijentAdresa;
    unsigned int addres_len = sizeof(klijentAdresa);
    int commSocket = accept(listenerSocket, (struct sockaddr *) &klijentAdresa, &addres_len);

    if(commSocket == -1){
       perror("accept");
    }

    char *dekadskiIP = inet_ntoa(klijentAdresa.sin_addr);
    //printf("Stigao je igrac s adresom %s\n", dekadskiIP);
    if(ready_user[0] == '\0'){
      strcpy(ready_user, dekadskiIP);
      ready_user_socket = commSocket;
      posaljiPoruku(commSocket, WAIT, "Cekamo dostupnog igraca...\n");
      continue;
    }

    ///inace mi jedan vec ceka, uparujem ovoga koji je dosao bas sa njime

    pthread_mutex_lock(&lokot_dretve);
    int i;
    int ind_free = -1;

    for(i = 0; i < N_dretve; i++){
      if(active[i] == 0){
        ind_free = i;
        break;
      }
      else if(active[i] == 2){
        ///ocistim i slobodna je
        pthread_join(dretve[i], NULL);
        active[i] = 0;
        ind_free = i;
        break;
      }
    }


    if(ind_free == -1){
      posaljiPoruku(commSocket, FULL_SERVER, "Server je pun, pokusajte kasnije.\n");
      close(commSocket);
    }


    ///ova dvojica sad imaju dretvu i mogu igrati
    else{
      igra_info nova;
      active[ind_free] = 1;
      nova.commSocketPrvi = ready_user_socket;
      nova.commSocketDrugi = commSocket;
      nova.thread_index = ind_free;
      nova.adresa_drugi = (char*) malloc(sizeof(char) * IP_len);
      nova.adresa_prvi = (char*) malloc(sizeof(char) * IP_len);
      strcpy(nova.adresa_drugi, dekadskiIP);
      strcpy(nova.adresa_prvi, ready_user);
      //printf("indeks dretve %d\n", ind_free);
      memset(dekadskiIP, '\0', sizeof(dekadskiIP));
      memset(ready_user, '\0', sizeof(ready_user));

      parametri[ind_free] = nova;

      printf("Zapocinje nova igra u sobi br %d.\n", ind_free);
      pthread_create(&dretve[ind_free], NULL, pokreniIgru, &parametri[ind_free]);
    }

    pthread_mutex_unlock(&lokot_dretve);
  }

  return 0;
}
