/* nanodns v0.1 - an ultra-minimal authoritative DNS server
 * 
 * (c) 2014 by Florian 'floe' Echtler <floe@butterbrot.org>
 * 
 * Licensed under GPL v3
 *
 * By default, nanodns reads data from /var/lib/nanodns/.
 * For every domain, put a file into this directory which
 * has the FQDN as filename (e.g. 'foo.bar.baz.org.', note
 * the final dot) and contains the IPv4 address of the host.
 * Make sure the files are world-readable, as nanodns drops
 * all privileges and changes its UID/GID to "nobody".
 *
 */

#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define NANODNS_ROOT "/var/lib/nanodns/"
#define NANODNS_UID  65534 // nobody

struct __attribute__ ((__packed__)) dns_query {

		uint16_t id;
		uint16_t flags;
		uint16_t question_count;
		uint16_t answer_count;
		uint16_t auth_count;
		uint16_t add_count;
		uint8_t  payload[];
};

struct __attribute__ ((__packed__)) dns_answer_a {
  uint16_t a_type;
  uint16_t a_class;
  uint32_t ttl;
  uint16_t rdlength;
  uint32_t ipaddr;
};


void chkerr(int error, const char* message) {
  if (error < 0) {
    printf("%s\nError: %s\n",message,strerror(errno));
    exit(1);
  }
}

int main(int argc, char* argv[]) {

  int mysock, result, clientaddrsize;
  struct sockaddr_in clientaddr;
  struct sockaddr_in serveraddr;

	uint8_t buffer[4096];
	struct dns_query* query = (struct dns_query*)buffer;
 
	// UDP-Socket erzeugen
  mysock = socket(PF_INET,SOCK_DGRAM,0);
  chkerr(mysock,"Can't open udp socket");

  // und an beliebige lokale Adresse, Port 53 binden
  memset((char*)&serveraddr,0,sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons(53);

  result = bind(mysock,(struct sockaddr*)&serveraddr,sizeof(serveraddr));
  chkerr(result,"Can't bind udp socket");

  tzset();

  // safety first!
  chkerr( chdir(NANODNS_ROOT),"Can't change to data directory (" NANODNS_ROOT ")\n");
  chkerr(chroot(NANODNS_ROOT),"Can't chroot to data directory (" NANODNS_ROOT ")\n");

  chkerr(setgid(NANODNS_UID), "Can't drop group privileges.\n");
  chkerr(setuid(NANODNS_UID), "Can't drop user privileges.\n");

  while (1) {

    // Auf UDP-Broadcast warten
    clientaddrsize = sizeof(clientaddr);
    result = recvfrom(mysock,&buffer,sizeof(buffer),0,(struct sockaddr*)&clientaddr,(socklen_t*)&clientaddrsize);
    if (result < 0) {
      printf("recvfrom error: %s\n",strerror(errno));
      continue;
    }

    printf("\nReceived query from: %s\n",inet_ntoa(clientaddr.sin_addr));

    // we're really stupid and can only deal with one question at once. duh.
    if (ntohs(query->question_count) != 1) {
      printf("Sorry: only one question at a time, please.\n");
      continue;
    }

    // convert the query into a plain old null-terminated string
    char host[4096] = { 0 };
		uint8_t len, offs = 0;
    while ((offs < result-sizeof(struct dns_query)) && (len = query->payload[offs])) {
      strncat(host,(char*)(query->payload+offs+1),len);
      strncat(host,".",1);
		  //query->payload[offs] = '.';
      offs += len+1;
    }

    // check for maximum permitted name length (also prevents possible overflow in memcpy)
    if (offs > 253) {
      printf("Sorry: query name length exceeds maximum.\n");
      continue;
    }

    // build answer record directly from the query
    memcpy(query->payload+offs+5,query->payload,offs+5);
    dns_answer_a* answer = (dns_answer_a*)(query->payload+offs+5+offs+1);

    if (ntohs(answer->a_type) != 0x0001) {
      printf("Sorry: I can only answer questions for A records.\n");
      continue;
    }
    if (ntohs(answer->a_class) != 0x0001) {
      printf("Sorry: I can only answer questions for Internet records.\n");
      continue;
    }

		printf("Received valid query for: %s\n",host);

    // get the IP answer data
    for (unsigned int i = 0; i < strlen(host); i++) host[i] = tolower(host[i]);
    FILE* answer_file = fopen(host,"r");
    char answer_data[32];
    struct in_addr answer_addr = { 0 };
    struct stat filestat;
    if (!answer_file) {
      printf("Sorry: no data available for this query.\n");
    } else {
      fgets(answer_data,sizeof(answer_data),answer_file);
      fstat(fileno(answer_file),&filestat);
      fclose(answer_file);
      if (!inet_aton(answer_data,&answer_addr))
        printf("Sorry: unable to convert data to IPv4 address.\n");
      else
        printf("Preparing reply with %s.\n",inet_ntoa(answer_addr));
    }

    struct tm modtime; localtime_r(&filestat.st_mtime,&modtime);
    int fakettl =
      100000000 * (modtime.tm_year-100) +
      1000000   * (modtime.tm_mon+1   ) +
      10000     * (modtime.tm_mday    ) +
      100       * (modtime.tm_hour    ) +
      1         * (modtime.tm_min     );

    // fill rest of fields
    int answer_len = sizeof(dns_query)+sizeof(dns_answer_a)+offs+5+offs+1;
    query->flags = htons(0x8400);

    query->question_count = htons(1);
		query->answer_count = htons(1);
		query->auth_count = htons(0);
		query->add_count = htons(0);

    answer->ttl = htonl(fakettl);
    answer->rdlength = htons(4);
    answer->ipaddr = answer_addr.s_addr;

    // no answer available?
    if (!answer_addr.s_addr) {
      query->flags |= htons(0x0003);
      query->answer_count = htons(0);
      answer_len = sizeof(dns_query)+offs+5;
    }

    // send answer
    result = sendto(mysock,&buffer,answer_len,0,(struct sockaddr*)&clientaddr,sizeof(clientaddr));
	  if (result < 0) {
      printf("sendto error: %s\n",strerror(errno));
      continue;
    }
  }
}
