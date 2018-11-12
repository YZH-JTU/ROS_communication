#include "ros/ros.h"
#include "std_msgs/String.h"

#include <unistd.h>
#include <thread>
#include "../include/communication/globalvar.h"
#include "../include/communication/pack_unpack.h"
#include "../include/communication/server.h"

int server_sockfd_PLC = -1;
int server_sockfd_MONITOR = -1;
unsigned int DP_control[7] = {};
int DP_monitor[7] = {};

int main(int argc, char **argv) {
  ros::init(argc, argv, "communication");
  std::thread _thread_socket_accept_PLC(startup_socket_PLC);
  _thread_socket_accept_PLC.detach();
  std::thread _thread_socket_accept_MONITOR(startup_socket_MONITOR);
  _thread_socket_accept_MONITOR.detach();

  while (ros::ok()) {
    printf("DP_monitor =%3d %3d  %3d  %3d  %3d  %3d  %3d  %3d\n",
           server_sockfd_MONITOR, DP_monitor[0], DP_monitor[1], DP_monitor[2],
           DP_monitor[3], DP_monitor[4], DP_monitor[5], DP_monitor[6]);
    printf("DP_control =%3d %3d  %3d  %3d  %3d  %3d  %3d  %3d\n",
           server_sockfd_PLC, DP_control[0], DP_control[1], DP_control[2],
           DP_control[3], DP_control[4], DP_control[5], DP_control[6]);
    usleep(500000);
  }

  return 0;
}