#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <pthread.h>

#include "protocol.h"

#define merror(s) printf(s); exit(0);

#define N_dretve 4
#define N_igraca 8
#define N_name 15

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
  if(brod == 3){ ///horizontalno
    int delta = 0;
    for(; delta <= 2; delta++){
      int tx = x, ty = y + delta;
      if(!inside(tx, ty) || ploca[soba][mapa][tx][ty] != EMPTY){
        return 0;
      }
    }
    return 1;
  }

  else if(brod == 2){ ///vertikalni
    int delta = 0;
    for(; delta <= 1; delta++){
      int tx = x + delta, ty = y;
      if(!inside(tx, ty) || ploca[soba][mapa][tx][ty] != EMPTY){
        return 0;
      }
    }

    return 1;
  }

  return 0; //nebitno
}

int ucitaj(char *potez, int *x, int *y){
  int c = sscanf(potez, "%d %d", x, y);
  *x -= 1;
  *y -= 1;
  return c;
}

void salji(int sock, int tip, char *tijelo){
  int f = posaljiPoruku(sock, tip, tijelo);
  if(f == NIJEOK){
    close(sock);
    merror("neuspjela komunikacija od servera\n");
  }
}

void salji2(int sock, int sock2, int tip, char *tijelo){
  int f = posaljiPoruku(sock, tip, tijelo);
  if(f == NIJEOK){
    close(sock);
    close(sock2);
    merror("neuspjela komunikacija od servera prema nekom korisniku\n");
  }
}

void *popuniPlocu(void *parametar_popuni){
    popuni_info *opis = (popuni_info *) parametar_popuni;
    int sock = opis -> socket;
    int mapa = opis -> mapa;
    int soba = opis -> soba;

    char brojac = 'A';

    int trice = 0, type;
    char *get;
    int f;

    while(trice != 2){
      salji(sock, KORD, "unesite koordinate lijevog kraja 1x3 broda?\n");
      f = primiPoruku(sock, &type, &get);
      if(f == NIJEOK){
        close(sock);
        merror("neuspjela komunikacija od servera\n");
      }

      int *x, *y;
      x = (int *)malloc(sizeof(int));
      y = (int *)malloc(sizeof(int));
      int c = ucitaj(get, x, y);

      int tx = *x, ty = *y;

      while(!check(soba, mapa, tx, ty, 3) || c != 2 || type != KORD){
        salji(sock, WRONG_INPUT, "krive koordinate, unesite ponovo\n");
        f = primiPoruku(sock, &type, &get);
        if(f == NIJEOK){
          close(sock);
          merror("neuspjela komunikacija od servera\n");
        }

        c = ucitaj(get, x, y);
        tx = *x; ty = *y;
      }

      int d = 0;
      for(; d < 3; d++)
        ploca[soba][mapa][tx][ty + d] = brojac;

      trice++;
      brojac++;
      //printf("tu su trice\n");
    }


    int dvice = 0;
    while(dvice != 2){
      salji(sock, KORD, "koordinate gornjeg kraja 2x1 broda?\n");
      f = primiPoruku(sock, &type, &get);
      if(f == NIJEOK){
        close(sock);
        merror("neuspjela komunikacija od servera\n");
      }

      int *x, *y;
      x = (int *)malloc(sizeof(int));
      y = (int *)malloc(sizeof(int));

      int c = ucitaj(get, x, y);

      int tx = *x, ty = *y;
      while(!check(soba, mapa, tx, ty, 2) || c != 2){
        salji(sock, WRONG_INPUT, "krive koordinate, unesite ponovo\n");
        f = primiPoruku(sock, &type, &get);
        if(f == NIJEOK){
          close(sock);
          merror("neuspjela komunikacija od servera\n");
        }

        c = ucitaj(get, x, y);
        tx = *x; ty = *y;
      }

      int d = 0;
      for(; d < 2; d++){
        ploca[soba][mapa][tx + d][ty] = brojac;
      }

      dvice++;
      brojac++;
    }

    salji(sock, NOTI, "OK, pricekajte protivnika");
}

void ispisi_plocu(int soba, int mapa){
  printf("==============igrac %d==============\n\n", mapa);
  int i, j;
  for(i = 0; i < 10; i++){
    for(j = 0; j < 10; j++){
      printf("%c", ploca[soba][mapa][i][j]);
    }
    printf("\n");
  }
  printf("===================================\n\n");
}

void reci_svima(char *str, int soc1, int soc2){
  salji2(soc1, soc2, NOTI, str);
  salji2(soc2, soc1, NOTI, str);
}

void unisti(int soba, int mapa, int x, int y, int dx, int dy, int k){
  int d = 0;
  while(1){
    ploca[soba][mapa][x][y] = '*';
    x += dx;
    y += dy;
    if(d == k) break;
    d++;
  }
}

void *pokreniIgru(void *parametar){
  igra_info *igra = (igra_info *) parametar;
  int uticnice[2];
  uticnice[0] = igra -> commSocketPrvi;
  uticnice[1] = igra -> commSocketDrugi;
  char *uvod;
  uvod = (char*) malloc(sizeof(char) * 100);
  int f;
  sprintf(uvod, "Igrate protiv igraca na racunalu %s\n", igra -> adresa_drugi);
  salji2(uticnice[0], uticnice[1], INTRO, uvod);
  sprintf(uvod, "igrate protiv igraca na racunalu %s\n", igra -> adresa_prvi);
  salji2(uticnice[1], uticnice[0], INTRO, uvod);

  ///sada postavljaju tablice
  ///prvo ocistim ploce u ovoj sobi

  int i;
  for(i = 0; i < 2; i++)
    memset(ploca[igra -> thread_index][i], EMPTY, sizeof(ploca[igra -> thread_index][i]));

  char *get;
  int type;
  ///ovdje cu uvijek naci slobodnu dretvu jer ih imam 2 * br_soba, tako da svaki igrac ima svoju
  int prva, druga;
  prva = 2 * (igra -> thread_index);
  druga = (2 * igra -> thread_index) + 1;

  assert(prva < 2 * N_dretve);
  assert(druga < 2 * N_dretve);
  assert(prva != druga);

  pthread_t dret[2];
  int ozn[2] = {prva, druga};
  int socovi[2] = {igra -> commSocketPrvi, igra -> commSocketDrugi};

  for(i = 0; i < 2; i++){
    param[ozn[i]].soba = igra -> thread_index;
    param[ozn[i]].mapa = i;
    param[ozn[i]].socket = socovi[i];
    pthread_create(&dret[i], NULL, popuniPlocu, &param[ozn[i]]);
  }

  pthread_join(dret[0], NULL);
  pthread_join(dret[1], NULL);

  reci_svima("Ok, polja su rezervirana, igra pocinje\n", uticnice[0], uticnice[1]);
  reci_svima("Prvi na redu ce biti igrac koji je dosao prije na server.\n", uticnice[0], uticnice[1]);

  int ii;
  printf("provjera konfiguracija\n");
  for(ii = 0; ii < 2; ii++){
    ispisi_plocu(igra -> thread_index, ii);
  }

  int zivih[2] = {4, 4};

  int winner;
  ///pocinje borba
  int turn = 0;
  while(1){
    salji2(uticnice[turn], uticnice[1 - turn], POTEZ, "Vi ste na redu.\n");
    salji2(uticnice[1 - turn], uticnice[turn], OPP, "Protivnik je na redu.\n");

    char *potez;
    f = primiPoruku(uticnice[turn], &type, &potez);
    if(f == NIJEOK){
      close(uticnice[0]);
      close(uticnice[1]);
      merror("greska na serveru\n");
    }

    int *x, *y;
    x = (int *)malloc(sizeof(int));
    y = (int *)malloc(sizeof(int));
    int c = ucitaj(potez, x, y);

    while(!inside(*x, *y) || c != 2){
      if(c != 2){
        salji2(uticnice[turn], uticnice[1 - turn], WRONG_INPUT, "nepravilan format koordinata, pokusajte ponovo\n");
      }
      else{
        salji2(uticnice[turn], uticnice[1 - turn], WRONG_INPUT, "Pozicija izvan ploce, pokusajte ponovo.\n");
      }

      f = primiPoruku(uticnice[turn], &type, &potez);
      if(f == NIJEOK){
        close(uticnice[0]);
        close(uticnice[1]);
        merror("greska na serveru\n");
      }

      c = ucitaj(potez, x, y);
    }

    ///ok sad je odigrao pravilno
    int lef = 0, rig = 0, up = 0, down = 0;
    ///sad ako je pogodio neki dio broda, iz ovog gore saznajem o kojem je brodu rijec
    ///to slijedi iz provjere u inputu koje su gledale da se brodovi ne sijeku

    int d;
    int soba = igra -> thread_index;
    int tx = *x, ty = *y;
    char hit = ploca[soba][1 - turn][tx][ty];

    if(!(hit >= 'A' && hit <= 'D')){
      reci_svima("Polje ne sadrzi brod.\n", uticnice[0], uticnice[1]);
      turn = 1 - turn;
      continue;
    }

    char shoot[30];
    sprintf(shoot, "pogoden vam je brod na %d %d!\n", tx + 1, ty + 1);
    salji(uticnice[1 - turn], NOTI, shoot);
    sprintf(shoot, "pogodili ste brod na %d %d!\n", tx + 1, ty + 1);
    salji(uticnice[turn], NOTI, shoot);

    zivih[1 - turn]--;

    int dx, dy;
    for(dx = -1; dx <= 1; dx++){
      for(dy = -2; dy <= 2; dy++){
        tx = *x + dx, ty = *y + dy;
        if(!inside(tx, ty)) continue;
        if(ploca[soba][1 - turn][tx][ty] == hit)
          ploca[soba][1 - turn][tx][ty] = BOMBA;
      }
    }

    if(zivih[1 - turn] == 0){
      winner = turn;
      break;
    }

    turn = 1 - turn;
  }


  salji2(uticnice[winner], uticnice[1 - winner], ISHOD, "Cestitamo, pobjedili ste!\n");
  salji2(uticnice[1 - winner], uticnice[winner], ISHOD, "Nazalost, izgubili ste!\n");

  printf("konacni scenarij:\n======================\n\n");

  for(ii = 0; ii < 2; ii++){
    ispisi_plocu(igra -> thread_index, ii);
  }

  pthread_mutex_lock(&lokot_dretve);
	active[igra -> thread_index] = 2;
	pthread_mutex_unlock(&lokot_dretve);

	close(uticnice[0]);
	close(uticnice[1]);
}

int main(int argc, char **argv){
  //perror("socket");
  if(argc != 2){
    printf("Program za pokretanje treba jos parametara\n");
    exit(0);
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
  int ima = 0; ///ceka li netko na igru?

  int ready_user_socket;

  while(1){
    struct sockaddr_in klijentAdresa;
    unsigned int addres_len = sizeof(klijentAdresa);
    int commSocket = accept(listenerSocket, (struct sockaddr *) &klijentAdresa, &addres_len);

    if(commSocket == -1){
       perror("accept");
    }

    char *dekadskiIP = inet_ntoa(klijentAdresa.sin_addr);

    if(ima == 0){
      free(ready_user);
      ready_user = (char*)malloc(sizeof(char) * (1 + strlen(dekadskiIP)));
      strcpy(ready_user, dekadskiIP);
      ready_user_socket = commSocket;
      int f = posaljiPoruku(commSocket, WAIT, "Cekamo dostupnog igraca...\n");
      if(f != OK){
        printf("greska od servera\n");
        continue;
      }
      ima = 1;
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
      ima = 0;
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
