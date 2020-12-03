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

#define merror(s) printf(s); exit(0)

#define N_dretve 3
#define N_igraca 6
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
  //x--; y--; ///input je 1 indeksiran, ja 0
  if(brod == 3){
    int delta = 0;
    for(; delta <= 2; delta++){
      int tx = x, ty = y + delta;
      if(!inside(tx, ty) || ploca[soba][mapa][tx][ty] != EMPTY){
        return 0;
      }
    }
    return 1;
  }

  else if(brod == 2){
    int delta = 0;
    for(; delta <= 1; delta++){
      int tx = x + delta, ty = y;
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

    char brojac = 'A';

    int trice = 0, type;
    char *get;
    int f;
    while(trice != 2){
      f = posaljiPoruku(sock, HOR, "koordinate lijevog kraja 1x3 broda?\n");
      if(f == NIJEOK){
        merror("neuspjela komunikacija od servera\n");
      }
      f = primiPoruku(sock, &type, &get);
      if(f == NIJEOK){
        merror("neuspjela komunikacija od servera\n");
      }
      int x, y;
      int c = sscanf(get, "%d %d", &x, &y);

      if(c != 2 || type != KORD){
        f = posaljiPoruku(sock, WRONG_INPUT, "krivi format ulaza, unesite ponovo\n");
        if(f == NIJEOK){
          merror("neuspjela komunikacija od servera\n");
        }
        continue;
      }

      //printf("dobio sam %d %d\n", x, y);
      x--; y--;

      if(!check(soba, mapa, x, y, 3)){
        f = posaljiPoruku(sock, WRONG_INPUT, "krive koordinate, unesite ponovo\n");
        if(f == NIJEOK){
          merror("neuspjela komunikacija od servera\n");
        }
        continue;
      }

      int d = 0;
      for(; d < 3; d++)
        ploca[soba][mapa][x][y + d] = brojac;
      trice++;

      brojac++;
      //printf("tu su trice\n");
    }


    int dvice = 0;
    while(dvice != 2){
      f = posaljiPoruku(sock, VERT, "koordinate gornjeg kraja 2x1 broda?\n");
      if(f == NIJEOK){
        merror("neuspjela komunikacija od servera\n");
      }
      f = primiPoruku(sock, &type, &get);
      if(f == NIJEOK){
        merror("neuspjela komunikacija od servera\n");
      }
      int x, y;
      int c = sscanf(get, "%d %d", &x, &y);
      if(c != 2 || type != KORD){
        f = posaljiPoruku(sock, WRONG_INPUT, "krivi format ulaza, unesite ponovo\n");
        if(f == NIJEOK){
          merror("neuspjela komunikacija od servera\n");
        }
        continue;
      }

      //printf("dobio sam %d %d\n", x, y);
      x--; y--;

      if(!check(soba, mapa, x, y, 2)){
        f = posaljiPoruku(sock, WRONG_INPUT, "krive koordinate, unesite ponovo\n");
        if(f == NIJEOK){
          merror("neuspjela komunikacija od servera\n");
        }
        continue;
      }

      int d = 0;
      for(; d < 2; d++){
        ploca[soba][mapa][x + d][y] = brojac;
      }
      dvice++;
      brojac++;
    }
}

int ucitaj(char *potez, int *x, int *y){
  int c = sscanf(potez, "%d %d", x, y);
  return c;
}

void reci_svima(char *str, int soc1, int soc2){
  int f = posaljiPoruku(soc1, NOTI, str);
  if(f == NIJEOK){
    merror("greska na serveru\n");
  }

  f = posaljiPoruku(soc2, NOTI, str);
  if(f == NIJEOK){
    merror("greska na serveru\n");
  }
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
  f = posaljiPoruku(uticnice[0], INTRO, uvod);
  if(f == NIJEOK){
    merror("greska na serveru\n");
  }
  sprintf(uvod, "igrate protiv igraca na racunalu %s\n", igra -> adresa_prvi);
  f = posaljiPoruku(uticnice[1], INTRO, uvod);
  if(f == NIJEOK){
    merror("greska na serveru\n");
  }

  ///postavljajte tablice molim
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

  param[prva].soba = igra -> thread_index;
  param[prva].mapa = 0;
  param[prva].socket = igra -> commSocketPrvi;
  pthread_create(&dret[0], NULL, popuniPlocu, &param[prva]);

  param[druga].soba = igra -> thread_index;
  param[druga].mapa = 1;
  param[druga].socket = igra -> commSocketDrugi;
  pthread_create(&dret[1], NULL, popuniPlocu, &param[druga]);

  pthread_join(dret[0], NULL);
  pthread_join(dret[1], NULL);

  f = posaljiPoruku(uticnice[0], READY, "Ok, polja su rezervirana, igra pocinje\n");
  if(f == NIJEOK){
    merror("greska na serveru\n");
  }

  f = posaljiPoruku(uticnice[1], READY, "Ok, polja su rezervirana, igra pocinje\n");
  if(f == NIJEOK){
    merror("greska na serveru\n");
  }

  int zivih[2] = {4, 4};

  int winner;
  ///pocinje borba
  int turn = 0;
  while(1){
    f = posaljiPoruku(uticnice[turn], POTEZ, "Vi ste na redu.\n");
    if(f == NIJEOK){
      merror("greska na serveru\n");
    }
    f = posaljiPoruku(uticnice[1 - turn], OPP, "Protivnik je na redu.\n");
    if(f == NIJEOK){
      merror("greska na serveru\n");
    }

    char *potez;
    type = POTEZ;
    f = primiPoruku(uticnice[turn], &type, &potez);
    if(f == NIJEOK){
      merror("greska na serveru\n");
    }

    int *x, *y;
    int c = ucitaj(potez, x, y);
    while(!inside(*x, *y) || c != 2){
      if(c != 0){
        f = posaljiPoruku(uticnice[turn], WRONG_INPUT, "nepravilan format koordinata, pokusajte ponovo\n");
        if(f == NIJEOK){
          merror("greska na serveru\n");
        }
        continue;
      }
      else{
        f = posaljiPoruku(uticnice[turn], WRONG_INPUT, "Pozicija izvan ploce, pokusajte ponovo.\n");
        if(f == NIJEOK){
          merror("greska na serveru\n");
        }
      }

      f = primiPoruku(uticnice[turn], &type, &potez);

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

    for(d = 0; d <= 2; d++){
      if(inside(tx, ty + d) && ploca[soba][1 - turn][tx][ty + d] == hit) rig++;
      else break;
    }
    for(d = -2; d <= 0; d++){
      if(inside(tx, ty + d) && ploca[soba][1 - turn][tx][ty + d] == hit) lef++;
      else break;
    }
    for(d = 0; d <= 1; d++){
      if(inside(tx + d, ty) && ploca[soba][1 - turn][tx + d][ty] == hit) down++;
      else break;
    }

    for(d = -1; d <= 0; d++){
      if(inside(tx + d, ty) && ploca[soba][1 - turn][tx + d][ty] == hit) up++;
      else break;
    }

    if(lef == 3){
      unisti(soba, 1 - turn, tx, ty, 0, -1, 2);
      zivih[1 - turn]--;
    }
    else if(rig == 3){
      unisti(soba, 1 - turn, tx, ty, 0, 1, 2);
      zivih[1 - turn]--;
    }

    else if(up == 2){
      unisti(soba, 1 - turn, tx, ty, -1, 0, 1);
      zivih[1 - turn]--;
    }

    else if(down == 2){
      unisti(soba, 1 - turn, tx, ty, 1, 0, 1);
      zivih[1 - turn]--;
    }

    if(zivih[1 - turn] == 0){
      winner = turn;
      break;
    }

    turn = 1 - turn;
  }


  f = posaljiPoruku(uticnice[winner], ISHOD, "Cestitamo, pobjedili ste!\n");
  if(f == NIJEOK){
    merror("greska na serveru\n");
  }
  f = posaljiPoruku(uticnice[1 - winner], ISHOD, "Nazalost, izgubili ste!\n");
  if(f == NIJEOK){
    merror("greska na serveru\n");
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
    //printf("Stigao je igrac s adresom %s\n", dekadskiIP);
    if(ima == 0){
      free(ready_user);
      ready_user = (char*)malloc(sizeof(char) * (1 + strlen(dekadskiIP)));
      strcpy(ready_user, dekadskiIP);
      ready_user_socket = commSocket;
      int f = posaljiPoruku(commSocket, WAIT, "Cekamo dostupnog igraca...\n");
      if(f != OK){
        merror("greska od servera\n");
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
