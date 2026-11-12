/* Forced include for ENet's protocol sources (host.c, protocol.c, peer.c, packet.c, list.c, callbacks.c,
 * compress.c; CMakeLists.txt): their socket and clock calls go to RAVEFORM's datagram layer (src/net/wire.cpp),
 * which puts the network condition simulator and the in-process loopback network under ENet and gives the
 * loopback soak a simulated clock. ENet's own unix.c / win32.c are compiled without this and keep the real
 * functions, which the datagram layer calls for UDP sockets. */
#ifndef RF_ENET_REMAP_H
#define RF_ENET_REMAP_H
#define enet_socket_create rf_enet_socket_create
#define enet_socket_bind rf_enet_socket_bind
#define enet_socket_get_address rf_enet_socket_get_address
#define enet_socket_set_option rf_enet_socket_set_option
#define enet_socket_destroy rf_enet_socket_destroy
#define enet_socket_send rf_enet_socket_send
#define enet_socket_receive rf_enet_socket_receive
#define enet_socket_wait rf_enet_socket_wait
#define enet_time_get rf_enet_time_get
#endif
