// System health/status monitoring module

#ifndef _UGEAR_HEALTH_H
#define _UGEAR_HEALTH_H


bool health_init();
bool health_update();
void health_update_command_sequence( int sequence );
void health_update_target_waypoint( int index );


#endif // _UGEAR_HEALTH_H
