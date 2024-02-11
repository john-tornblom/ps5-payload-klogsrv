/* Copyright (C) 2023 John Törnblom

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>


typedef struct notify_request {
  char useless[45];
  char message[3075];
} notify_request_t;


int sceKernelSendNotificationRequest(int, notify_request_t*, size_t, int);


static void
notify(const char *fmt, ...) {
  notify_request_t req;
  va_list args;

  bzero(&req, sizeof req);
  va_start(args, fmt);
  vsnprintf(req.message, sizeof req.message, fmt, args);
  va_end(args);

  sceKernelSendNotificationRequest(0, &req, sizeof req, 0);
  printf("[klogsrv.elf] %s\n", req.message);
}


static int
serve_file_while_connected(const char *path, int server_fd) {
  struct timeval timeout;
  size_t nb_connections;
  fd_set output_set;
  fd_set input_set;
  fd_set temp_set;
  int client_fd;
  int file_fd;
  int err = 0;
  char ch;

  if((file_fd=open(path, O_RDONLY)) < 0) {
    perror("[klogsrv.elf] open");
    return -1;
  }

  FD_ZERO(&input_set);
  FD_ZERO(&output_set);

  FD_SET(server_fd, &input_set);
  FD_SET(file_fd, &input_set);

  timeout.tv_sec = 0;
  timeout.tv_usec = 1000*10; //10ms
  nb_connections = 0;

  do {
    temp_set = input_set;
    switch(select(FD_SETSIZE, &temp_set, NULL, NULL, &timeout)) {
    case 0:
      continue;
    case -1:
      perror("[klogsrv.elf] select");
      close(file_fd);
      return -1;
    }

    // new connection
    if(FD_ISSET(server_fd, &temp_set)) {
      if((client_fd=accept(server_fd, NULL, NULL)) < 0) {
	perror("[klogsrv.elf] accept");
	err = -1;
	break;
      }
      FD_SET(client_fd, &output_set);
      nb_connections++;
    }

    // new data from file
    if(FD_ISSET(file_fd, &temp_set)) {
      if(read(file_fd, &ch, 1) != 1) {
	perror("[klogsrv.elf] read");
	err = -1;
	break;
      }

      for(client_fd=0; client_fd<FD_SETSIZE; client_fd++) {
	if(FD_ISSET(client_fd, &output_set)) {
	  if(write(client_fd, &ch, 1) != 1) {
	    FD_CLR(client_fd, &output_set);
	    close(client_fd);
	    nb_connections--;
	  }
	}
      }
    }
  } while(nb_connections > 0);

  for(client_fd=0; client_fd<FD_SETSIZE; client_fd++) {
    if(FD_ISSET(client_fd, &output_set)) {
      FD_CLR(client_fd, &output_set);
      close(client_fd);
    }
  }
  close(file_fd);

  return err;
}


static int
serve_file(const char *path, uint16_t port) {
  char ip[INET_ADDRSTRLEN];
  struct ifaddrs *ifaddr;
  struct sockaddr_in sin;
  int ifaddr_wait = 1;
  fd_set set;
  int sockfd;
  int flags;

  if(getifaddrs(&ifaddr) == -1) {
    perror("[klogsrv.elf] getifaddrs");
    return -1;
  }

  // Enumerate all AF_INET IPs
  for(struct ifaddrs *ifa=ifaddr; ifa!=NULL; ifa=ifa->ifa_next) {
    if(ifa->ifa_addr == NULL) {
      continue;
    }

    if(ifa->ifa_addr->sa_family != AF_INET) {
      continue;
    }

    // skip localhost
    if(!strncmp("lo", ifa->ifa_name, 2)) {
      continue;
    }

    struct sockaddr_in *in = (struct sockaddr_in*)ifa->ifa_addr;
    inet_ntop(AF_INET, &(in->sin_addr), ip, sizeof(ip));

    // skip interfaces without an ip
    if(!strncmp("0.", ip, 2)) {
      continue;
    }

    notify("Serving /dev/klog on %s:%d (%s)", ip, port, ifa->ifa_name);
    ifaddr_wait = 0;
  }

  freeifaddrs(ifaddr);

  if(ifaddr_wait) {
    return 0;
  }

  if((sockfd=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("[klogsrv.elf] socket");
    return -1;
  }

  if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
    perror("[klogsrv.elf] setsockopt");
    return -1;
  }

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  sin.sin_port = htons(port);

  if(bind(sockfd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
    perror("[klogsrv.elf] bind");
    return -1;
  }

  if(listen(sockfd, 5) < 0) {
    perror("[klogsrv.elf] listen");
    return -1;
  }

  while(1) {
    // wait for a connection
    FD_ZERO(&set);
    FD_SET(sockfd, &set);
    if(select(sockfd+1, &set, NULL, NULL, NULL) < 0) {
      perror("[klogsrv.elf] select");
      return -1;
    }

    // someone wants to connect
    if(FD_ISSET(sockfd, &set)) {
      if(serve_file_while_connected(path, sockfd) < 0) {
	close(sockfd);
	return -1;
      }
    }
  }

  return 0;
}


/**
 *
 **/
static void
init_stdio(void) {
  int fd = open("/dev/console", O_WRONLY);

  close(STDERR_FILENO);
  close(STDOUT_FILENO);

  dup2(fd, STDOUT_FILENO);
  dup2(fd, STDERR_FILENO);

  close(fd);
}



int
main() {
  uint16_t port = 3232;

  init_stdio();
  syscall(SYS_setsid);
  syscall(SYS_thr_set_name, -1, "klogsrv.elf");

  printf("[klogsrv.elf] KLOG server was compiled at %s %s\n", __DATE__, __TIME__);
  while(1) {
    serve_file("/dev/klog", port);
    sleep(3);
  }

  return 0;
}
