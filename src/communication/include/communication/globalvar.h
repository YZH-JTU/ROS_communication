#ifndef __GLOBALVAR_H__
#define __GLOBALVAR_H__

extern float heading_angle;
extern double latitude;
extern double longitude;

// PLC_DP_data
extern unsigned int DP_control[];
extern int DP_monitor[];

// server_socket
#define PLC_PORT "2000"      // 提供给PLC连接的port
#define MONITOR_PORT "2001"  // 提供给MONITOR连接的port
#define IP_FOR_PLC "192.168.0.168"
#define IP_FOR_MONITOR "192.168.1.190"
extern unsigned int stress_data;
extern int server_sockfd_PLC;
extern int server_sockfd_MONITOR;

#endif
