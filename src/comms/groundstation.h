/*******************************************************************************
 * FILE: ground_station.h
 * DESCRIPTION: routines to do ethernet based communication with the ground
 *              station.
 *
 * SOURCE: 
 * REVISED: 9/02/05 Jung Soon Jang
 * REVISED: 4/07/06 Jung Soon Jang
 ******************************************************************************/

#ifndef _UGEAR_GROUNDSTATION_H
#define _UGEAR_GROUNDSTATION_H


// global variables

extern char *HOST_IP_ADDR;  //default ground station IP address (hardcoded in
                            //groundstation.cpp)
extern char buf_err[50];
extern int gs_sock_fd;


// Ground station client communication routines

bool open_client();
void send_client();
void close_client();


#endif // _UGEAR_GROUNDSTATION_H
