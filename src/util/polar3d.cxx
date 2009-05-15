// polar.cxx -- routines to deal with polar math and transformations
//
// Written by Curtis Olson, started June 1997.
//
// Copyright (C) 1997  Curtis L. Olson  - http://www.flightgear.org/~curt
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// $Id: polar3d.cxx,v 1.1 2007/07/18 21:07:10 curt Exp $

#include <math.h>

#include <include/globaldefs.h>

#include "polar3d.hxx"

/**
 * Calculate new lon/lat given starting lon/lat, and offset radial, and
 * distance.  NOTE: starting point is specifed in radians, distance is
 * specified in meters (and converted internally to radians)
 * ... assumes a spherical world.
 * @param orig specified in polar coordinates
 * @param course offset radial
 * @param dist offset distance
 * @return destination point in polar coordinates
 */
Point3D calc_gc_lon_lat( const Point3D& orig, double course,
                                double dist ) {
    Point3D result;

    // lat=asin(sin(lat1)*cos(d)+cos(lat1)*sin(d)*cos(tc))
    // IF (cos(lat)=0)
    //   lon=lon1      // endpoint a pole
    // ELSE
    //   lon=mod(lon1-asin(sin(tc)*sin(d)/cos(lat))+pi,2*pi)-pi
    // ENDIF

    // printf("calc_lon_lat()  offset.theta = %.2f offset.dist = %.2f\n",
    //        offset.theta, offset.dist);

    dist *= SG_METER_TO_NM * SG_NM_TO_RAD;

    result.sety( asin( sin(orig.y()) * cos(dist) +
                       cos(orig.y()) * sin(dist) * cos(course) ) );

    if ( cos(result.y()) < SG_EPSILON ) {
        result.setx( orig.x() );      // endpoint a pole
    } else {
        result.setx(
            fmod(orig.x() - asin( sin(course) * sin(dist) /
                                  cos(result.y()) )
                 + SGD_PI, SGD_2PI) - SGD_PI );
    }

    return result;
}

/**
 * Calculate course/dist given two spherical points.
 * @param start starting point
 * @param dest ending point
 * @param course resulting course
 * @param dist resulting distance
 */
void calc_gc_course_dist( const Point3D& start, const Point3D& dest,
                                 double *course, double *dist )
{
    if ( start == dest) {
            *dist=0;
            *course=0;
            return;
    }
    // d = 2*asin(sqrt((sin((lat1-lat2)/2))^2 +
    //            cos(lat1)*cos(lat2)*(sin((lon1-lon2)/2))^2))
    double cos_start_y = cos( start.y() );
    double tmp1 = sin( (start.y() - dest.y()) * 0.5 );
    double tmp2 = sin( (start.x() - dest.x()) * 0.5 );
    double d = 2.0 * asin( sqrt( tmp1 * tmp1 +
                                 cos_start_y * cos(dest.y()) * tmp2 * tmp2));

    *dist = d * SG_RAD_TO_NM * SG_NM_TO_METER;

#if 1
    double c1 = atan2(
                cos(dest.y())*sin(dest.x()-start.x()),
                cos(start.y())*sin(dest.y())-
                sin(start.y())*cos(dest.y())*cos(dest.x()-start.x()));
    if (c1 >= 0)
      *course = SGD_2PI-c1;
    else
      *course = -c1;
#else
    // We obtain the initial course, tc1, (at point 1) from point 1 to
    // point 2 by the following. The formula fails if the initial
    // point is a pole. We can special case this with:
    //
    // IF (cos(lat1) < EPS)   // EPS a small number ~ machine precision
    //   IF (lat1 > 0)
    //     tc1= pi        //  starting from N pole
    //   ELSE
    //     tc1= 0         //  starting from S pole
    //   ENDIF
    // ENDIF
    //
    // For starting points other than the poles:
    //
    // IF sin(lon2-lon1)<0
    //   tc1=acos((sin(lat2)-sin(lat1)*cos(d))/(sin(d)*cos(lat1)))
    // ELSE
    //   tc1=2*pi-acos((sin(lat2)-sin(lat1)*cos(d))/(sin(d)*cos(lat1)))
    // ENDIF

    // if ( cos(start.y()) < SG_EPSILON ) {
    // doing it this way saves a transcendental call
    double sin_start_y = sin( start.y() );
    if ( fabs(1.0-sin_start_y) < SG_EPSILON ) {
        // EPS a small number ~ machine precision
        if ( start.y() > 0 ) {
            *course = SGD_PI;   // starting from N pole
        } else {
            *course = 0;        // starting from S pole
        }
    } else {
        // For starting points other than the poles:
        // double tmp3 = sin(d)*cos_start_y);
        // double tmp4 = sin(dest.y())-sin(start.y())*cos(d);
        // double tmp5 = acos(tmp4/tmp3);

        // Doing this way gaurentees that the temps are
        // not stored into memory
        double tmp5 = acos( (sin(dest.y()) - sin_start_y * cos(d)) /
                            (sin(d) * cos_start_y) );

        // if ( sin( dest.x() - start.x() ) < 0 ) {
        // the sin of the negative angle is just the opposite sign
        // of the sin of the angle  so tmp2 will have the opposite
        // sign of sin( dest.x() - start.x() )
        if ( tmp2 >= 0 ) {
            *course = tmp5;
        } else {
            *course = SGD_2PI - tmp5;
        }
    }
#endif
}


#if 0
/**
 * Calculate course/dist given two spherical points.
 * @param start starting point
 * @param dest ending point
 * @param course resulting course
 * @param dist resulting distance
 */
void calc_gc_course_dist( const Point3D& start, const Point3D& dest,
                                 double *course, double *dist ) {
    // d = 2*asin(sqrt((sin((lat1-lat2)/2))^2 +
    //            cos(lat1)*cos(lat2)*(sin((lon1-lon2)/2))^2))
    double tmp1 = sin( (start.y() - dest.y()) / 2 );
    double tmp2 = sin( (start.x() - dest.x()) / 2 );
    double d = 2.0 * asin( sqrt( tmp1 * tmp1 +
                                 cos(start.y()) * cos(dest.y()) * tmp2 * tmp2));
    // We obtain the initial course, tc1, (at point 1) from point 1 to
    // point 2 by the following. The formula fails if the initial
    // point is a pole. We can special case this with:
    //
    // IF (cos(lat1) < EPS)   // EPS a small number ~ machine precision
    //   IF (lat1 > 0)
    //     tc1= pi        //  starting from N pole
    //   ELSE
    //     tc1= 0         //  starting from S pole
    //   ENDIF
    // ENDIF
    //
    // For starting points other than the poles:
    //
    // IF sin(lon2-lon1)<0
    //   tc1=acos((sin(lat2)-sin(lat1)*cos(d))/(sin(d)*cos(lat1)))
    // ELSE
    //   tc1=2*pi-acos((sin(lat2)-sin(lat1)*cos(d))/(sin(d)*cos(lat1)))
    // ENDIF

    double tc1;

    if ( cos(start.y()) < SG_EPSILON ) {
        // EPS a small number ~ machine precision
        if ( start.y() > 0 ) {
            tc1 = SGD_PI;        // starting from N pole
        } else {
            tc1 = 0;            // starting from S pole
        }
    }

    // For starting points other than the poles:

    double tmp3 = sin(d)*cos(start.y());
    double tmp4 = sin(dest.y())-sin(start.y())*cos(d);
    double tmp5 = acos(tmp4/tmp3);
    if ( sin( dest.x() - start.x() ) < 0 ) {
         tc1 = tmp5;
    } else {
         tc1 = SGD_2PI - tmp5;
    }

    *course = tc1;
    *dist = d * SG_RAD_TO_NM * SG_NM_TO_METER;
}
#endif // 0
