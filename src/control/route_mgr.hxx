// route_mgr.hxx - manage a route (i.e. a collection of waypoints)
//
// Written by Curtis Olson, started January 2004.
//
// Copyright (C) 2004  Curtis L. Olson  - http://www.flightgear.org/~curt
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// $Id: route_mgr.hxx,v 1.6 2008/05/09 00:34:28 curt Exp $


#ifndef _ROUTE_MGR_HXX
#define _ROUTE_MGR_HXX 1

#ifndef __cplusplus
# error This library requires C++
#endif

#include <string>
#include <vector>

using std::string;
using std::vector;

#include "props/props.hxx"
#include "route.hxx"


/**
 * Top level route manager class
 * 
 */

class FGRouteMgr {

public:

    enum fgRouteMode {
        GoHome = 0,
        FollowRoute = 1
    };

private:

    SGRoute *route;
    SGWayPoint home;

    // route configuration
    SGPropertyNode_ptr config_props;

    // automatic inputs
    SGPropertyNode_ptr lon;
    SGPropertyNode_ptr lat;
    SGPropertyNode_ptr alt;

    // automatic outputs
    SGPropertyNode_ptr true_hdg_deg;
    SGPropertyNode_ptr target_altitude_ft;
    SGPropertyNode_ptr target_agl_ft;

    bool home_set;
    bool altitude_set;
    bool agl_set;

    fgRouteMode mode;

    SGWayPoint make_waypoint( const string& target );

    bool build();

public:

    FGRouteMgr();
    ~FGRouteMgr();

    void init();

    void update();

    int new_waypoint( const string& tgt_alt, int n = -1 );
    void add_waypoint( const SGWayPoint& wp, int n = -1 );
    void replace_waypoint( const SGWayPoint &wp, int n );

    SGWayPoint get_waypoint( int i ) const {
        return route->get_waypoint(i);
    }

    SGWayPoint pop_waypoint( int i = 0 );

    int size() const {
        return route->size();
    }

    bool update_home( const SGWayPoint &wp, bool force_update );

    inline void set_route_mode() {
        mode = FollowRoute;
        FILE *debug = fopen("/mnt/mmc/debug.txt", "a");
        fprintf(debug, "mode: FollowRoute\n");
        fclose(debug);
    }

    inline void set_home_mode() {
        mode = GoHome;
        FILE *debug = fopen("/mnt/mmc/debug.txt", "a");
        fprintf(debug, "mode: GoHome\n");
        fclose(debug);
    }

    inline fgRouteMode get_route_mode() {
        return mode;
    }
};


extern FGRouteMgr route_mgr;           // global route manager object


#endif // _ROUTE_MGR_HXX
