#include "../include/communication/globalvar.h"
#include "../include/communication/pack_unpack.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BACKLOG 10  // 有多少个特定的连接队列（pending connections queue）
#define MAXDATASIZE 1024  // 我们一次可以收到的最大字节数量（number of bytes）

// 取得 sockaddr，IPv4 或 IPv6：
void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void *receive_from_PLC(void *ptr) {
  int numbytes;
  unsigned char buf[MAXDATASIZE];
  memset(&buf, 0, sizeof buf);

  printf("receive_from_PLC_child!!!\n");
  while (1) {
    if ((numbytes = recv(server_sockfd_PLC, buf, MAXDATASIZE, 0)) == -1) {
      perror("recv");
      close(server_sockfd_PLC);
      server_sockfd_PLC = -1;
      return NULL;
    }
    if (numbytes == 0) {
      close(server_sockfd_PLC);
      server_sockfd_PLC = -1;
      return NULL;
    }
    //    printf("received: %x \n", buf[0]);
    //    printf("received: %d bytes  ", numbytes);
    unpack(buf, "lllllll", &DP_monitor[0], &DP_monitor[1], &DP_monitor[2],
           &DP_monitor[3], &DP_monitor[4], &DP_monitor[5], &DP_monitor[6]);
    // printf("server received: %d\n", stress_data);

    //    packetsize = pack(buf, "d", stress_data);
    //    if (send(new_fd, buf, packetsize, 0) == -1) perror("send");
  }
}

void *send_to_PLC(void *ptr) {
  int numbytes;
  unsigned char buf[MAXDATASIZE];
  memset(&buf, 0, sizeof buf);

  printf("send_to_PLC_child!!!\n");
  while (1) {  // 主要的 accept() 循环
    numbytes = pack(buf, "LLLLLLL", DP_control[0], DP_control[1], DP_control[2],
                    DP_control[3], DP_control[4], DP_control[5], DP_control[6]);
    if ((numbytes = send(server_sockfd_PLC, buf, numbytes, 0)) == -1) {
      perror("send");
      close(server_sockfd_PLC);
      server_sockfd_PLC = -1;
      return NULL;
    }
    usleep(110000);
  }
}

void *server_receive_MONITOR(void *ptr) {
  int numbytes;
  unsigned char buf[MAXDATASIZE];
  memset(&buf, 0, sizeof buf);

  printf("server_receive_MONITOR_child!!!\n");
  while (1) {
    if ((numbytes = recv(server_sockfd_MONITOR, buf, MAXDATASIZE, 0)) == -1) {
      perror("recv");
      close(server_sockfd_MONITOR);
      server_sockfd_MONITOR = -1;
      return NULL;
    }
    if (numbytes == 0) {
      close(server_sockfd_MONITOR);
      server_sockfd_MONITOR = -1;
      return NULL;
    }
    //    printf("received: %x \n", buf[0]);
    //    printf("received: %d bytes  ", numbytes);
    unpack(buf, "LLLLLLL", &DP_control[0], &DP_control[1], &DP_control[2],
           &DP_control[3], &DP_control[4], &DP_control[5], &DP_control[6]);
    // printf("server received: %d\n", stress_data);

    //    packetsize = pack(buf, "d", stress_data);
    //    if (send(new_fd, buf, packetsize, 0) == -1) perror("send");
  }
}

void *send_to_MONITOR(void *ptr) {
  int numbytes;
  unsigned char buf[MAXDATASIZE];
  memset(&buf, 0, sizeof buf);

  printf("dend_to_PLC_child!!!\n");
  while (1) {  // 主要的 accept() 循环
    numbytes = pack(buf, "lllllll", DP_monitor[0], DP_monitor[1], DP_monitor[2],
                    DP_monitor[3], DP_monitor[4], DP_monitor[5], DP_monitor[6]);
    if ((numbytes = send(server_sockfd_MONITOR, buf, numbytes, 0)) == -1) {
      perror("send");
      close(server_sockfd_MONITOR);
      server_sockfd_MONITOR = -1;
      return NULL;
    }
    usleep(110000);
  }
}

int startup_socket_PLC(void) {
  int sockfd, new_fd;  // 在 sock_fd 进行 listen，new_fd 是新的连接
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;  // 连接者的地址资料
  socklen_t sin_size;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  pthread_t tid_1, tid_2;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  // hints.ai_flags = AI_PASSIVE;  // 使用我的 IP

  if ((rv = getaddrinfo(IP_FOR_PLC, PLC_PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // 以循环找出全部的结果，并绑定（bind）到第一个能用的结果
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }
    // 你可能有注意到，有时候你试着重新运行 server，而 bind() 却失败了，
    // 它声称＂Address already in use.＂（地址正在使用）。这是什麽意思呢？
    // 很好，有些连接到 socket 的连接还悬在 kernel 里面，而它占据了这个 port。
    // 你可以等待它自行清除［一分钟之类］，或者在你的程序中新增代码，
    // 让它重新使用这个 port，类似这样：
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      close(sockfd);
      return 0;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    close(sockfd);
    return 0;
  }

  freeaddrinfo(servinfo);  // 全部都用这个 structure

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    close(sockfd);
    return 0;
  }

  // sa.sa_handler = sigchld_handler;  // 收拾全部死掉的 processes
  // sigemptyset(&sa.sa_mask);
  // sa.sa_flags = SA_RESTART;

  // if (sigaction(SIGCHLD, &sa, NULL) == -1) {
  //   perror("sigaction");
  //   return 0;
  // }

  printf("server: waiting for connections...\n");

  sin_size = sizeof their_addr;

  while (1) {
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

    if (new_fd == -1) {
      perror("accept");
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s);
    printf("server: got connection from %s\n", s);

    server_sockfd_PLC = new_fd;

    if (pthread_create(&tid_1, NULL, receive_from_PLC, NULL) !=
        0) {  // 成功返回0，错误返回错误编号
      printf("Create pthread error!\n");
    }
    pthread_detach(tid_1);

    if (pthread_create(&tid_2, NULL, send_to_PLC, NULL) !=
        0) {  // 成功返回0，错误返回错误编号
      printf("Create pthread error!\n");
    }
    pthread_detach(tid_2);
  }

  // if (!fork()) { // 这个是 child process
  //   close(sockfd); // child 不需要 listener

  //   if (send(new_fd, "Hello, world!", 13, 0) == -1)
  //     perror("send");

  //   close(new_fd);

  //   exit(0);
  // }

  close(server_sockfd_PLC);
  close(sockfd);
  return 0;
}

int startup_socket_MONITOR(void) {
  int sockfd, new_fd;  // 在 sock_fd 进行 listen，new_fd 是新的连接
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;  // 连接者的地址资料
  socklen_t sin_size;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  pthread_t tid_1, tid_2;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  // hints.ai_flags = AI_PASSIVE;  // 使用我的 IP

  if ((rv = getaddrinfo(IP_FOR_MONITOR, MONITOR_PORT, &hints, &servinfo)) !=
      0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // 以循环找出全部的结果，并绑定（bind）到第一个能用的结果
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }
    // 你可能有注意到，有时候你试着重新运行 server，而 bind() 却失败了，
    // 它声称＂Address already in use.＂（地址正在使用）。这是什麽意思呢？
    // 很好，有些连接到 socket 的连接还悬在 kernel 里面，而它占据了这个 port。
    // 你可以等待它自行清除［一分钟之类］，或者在你的程序中新增代码，
    // 让它重新使用这个 port，类似这样：
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      close(sockfd);
      return 0;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    close(sockfd);
    return 0;
  }

  freeaddrinfo(servinfo);  // 全部都用这个 structure

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    close(sockfd);
    return 0;
  }

  // sa.sa_handler = sigchld_handler;  // 收拾全部死掉的 processes
  // sigemptyset(&sa.sa_mask);
  // sa.sa_flags = SA_RESTART;

  // if (sigaction(SIGCHLD, &sa, NULL) == -1) {
  //   perror("sigaction");
  //   return 0;
  // }

  printf("server: waiting for connections...\n");

  sin_size = sizeof their_addr;

  while (1) {
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

    if (new_fd == -1) {
      perror("accept");
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s);
    printf("server: got connection from %s\n", s);

    server_sockfd_MONITOR = new_fd;

    if(pthread_create(&tid_1,NULL,server_receive_MONITOR,NULL) != 0){// 成功返回0，错误返回错误编号
      printf ("Create pthread error!\n");
    }
    pthread_detach(tid_1);

    if (pthread_create(&tid_2, NULL, send_to_MONITOR, NULL) !=
        0) {  // 成功返回0，错误返回错误编号
      printf("Create pthread error!\n");
    }
    pthread_detach(tid_2);
  }

  // if (!fork()) { // 这个是 child process
  //   close(sockfd); // child 不需要 listener

  //   if (send(new_fd, "Hello, world!", 13, 0) == -1)
  //     perror("send");

  //   close(new_fd);

  //   exit(0);
  // }

  close(server_sockfd_MONITOR);
  close(sockfd);
  return 0;
}
