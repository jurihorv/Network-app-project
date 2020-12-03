#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "protocol.h"

#define merror(s) printf(s); exit(0)

void meni(){
  printf("1 -> ulaz u igricu\n");
  printf("2 -> izlaz\n");
}

int main(int argc, char **argv){

  if(argc != 3){
    merror("Program za pokretanje koristi 3 parametra\n");
  }

  //char username[N_name];
  //strcpy(username, argv[1]);
  //username[N_name - 1] = '\0';

  char dekadski[IP_len];
  strcpy(dekadski, argv[1]);
  int port;
  sscanf(argv[2], "%d", &port);

  int mySock = socket(PF_INET, SOCK_STREAM, 0);

  if(mySock == -1){
    merror("Greska u trazenju uticnice\n");
  }

  struct sockaddr_in adresa_servera;
  adresa_servera.sin_family = AF_INET;
  adresa_servera.sin_port = htons(port); ///htons ili htonl??

  if(inet_aton(dekadski, &adresa_servera.sin_addr) == 0){
    merror("kriva adresa\n");
  }

  memset(adresa_servera.sin_zero, '\0', 8);

  if(connect(mySock, (struct sockaddr *) &adresa_servera, sizeof(adresa_servera)) == -1){
    merror("Neuspjelo spajanje\n");
  }

  ///sad cekam na igru

  char *poruka;
  int vrsta;
  int f;
  f = primiPoruku(mySock, &vrsta, &poruka);
  if(f != OK){
    merror("komunikacijska greska\n");
  }

  if(vrsta == FULL_SERVER){
    printf("%s\n", poruka);
    close(mySock);
    return 0;
  }

  printf("%s\n", poruka);

  while(vrsta == WAIT){
    int nv;
    f = primiPoruku(mySock, &nv, &poruka);
    if(f != OK){
      merror("greska u komunikaciji\n");
    }
    if(nv == INTRO){
      printf("%s\n", poruka);
      break;
    }
  }

  ///sada sam u igri

  while(1){
    f = primiPoruku(mySock, &vrsta, &poruka);
    if(f != OK){
      merror("neuspjela komunikacija\n");
    }

    printf("%s\n", poruka);

    if(vrsta == HOR || vrsta == WRONG_INPUT || vrsta == VERT){
      int x, y;
      scanf("%d %d", &x, &y);
      int sx = htonl(x);
      int sy = htonl(y);
      char *odg;
      odg = (char*)malloc(sizeof(char) * 8);
      sprintf(odg, "%d %d", x, y);
      f = posaljiPoruku(mySock, KORD, odg);
      if(f != OK){
        merror("greska u komunikaciji\n");
      }
    }

    else if(vrsta == READY || vrsta == OPP || vrsta == NOTI){
      continue;
    }

    else if(vrsta == ISHOD){
      break;
    }


  }


  close(mySock);

  return 0;
}
