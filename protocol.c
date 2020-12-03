#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "protocol.h"

#define merror(s) printf(s); exit(0)

int posaljiPoruku(int sock, int vrsta_poruke, const char *poruka){
  int len = strlen(poruka);
  int last;

  int convert_len = htonl(len);
  last = send(sock, &convert_len, sizeof(convert_len), 0);
  if(last != sizeof(convert_len)) return NIJEOK;

  int convert_vrsta = htonl(vrsta_poruke);
  last = send(sock, &convert_vrsta, sizeof(convert_vrsta), 0);
  if(last != sizeof(convert_vrsta)) return NIJEOK;

  int sent = 0;
  while(sent != len){
    last = send(sock, poruka + sent, len - sent, 0);
    if(last == -1 || last == 0) return NIJEOK;
    sent += last;
  }

  return OK;
}

int primiPoruku(int sock, int *vrsta_poruke, char **poruka){
  int last;
  int net_len, len;
  last = recv(sock, &net_len, sizeof(net_len), 0);
  if(last != sizeof(net_len)) return NIJEOK;
  len = ntohl(net_len);

  int net_vrsta;
  last = recv(sock, &net_vrsta, sizeof(net_vrsta), 0);
  if(last != sizeof(net_vrsta)) return NIJEOK;
  *vrsta_poruke = ntohl(net_vrsta);

  *poruka = (char*) malloc((len + 1) * sizeof(char));

  int got = 0;

  while(got != len){
    last = recv(sock, *poruka + got, len - got, 0);
    if(last == -1 || last == 0) return NIJEOK;
    got += last;
  }

  (*poruka)[len] = '\0';
  return OK;
}
