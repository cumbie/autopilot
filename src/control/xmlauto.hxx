// xmlauto.hxx - a more flexible, generic way to build autopilots
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
// $Id: xmlauto.hxx,v 1.1 2007/03/20 20:39:49 curt Exp $


#ifndef _XMLAUTO_HXX
#define _XMLAUTO_HXX 1

#ifndef __cplusplus
# error This library requires C++
#endif

#include <string>
#include <vector>
#include <deque>

using std::string;
using std::vector;
using std::deque;

#include <props/props.hxx>
// #include </structure/subsystem_mgr.hxx>

// #include <Main/fg_props.hxx>


/**
 * Base class for other autopilot components
 */

class FGXMLAutoComponent {

protected:

    string name;

    SGPropertyNode_ptr enable_prop;
    SGPropertyNode_ptr passive_mode;
    string enable_value;
    bool honor_passive;
    bool enabled;

    SGPropertyNode_ptr input_prop;
    SGPropertyNode_ptr r_n_prop;
    double r_n_value;
    vector <SGPropertyNode_ptr> output_list;

public:

    FGXMLAutoComponent() :
      enable_prop( NULL ),
      passive_mode( fgGetNode("/autopilot/locks/passive-mode", true) ),
      enable_value( "" ),
      honor_passive( false ),
      enabled( false ),
      input_prop( NULL ),
      r_n_prop( NULL ),
      r_n_value( 0.0 )
    { }

    virtual ~FGXMLAutoComponent() {}

    virtual void update (double dt)=0;
    
    inline const string& get_name() { return name; }
};


/**
 * Roy Ovesen's PID controller
 */

class FGPIDController : public FGXMLAutoComponent {

private:

    // debug flag
    bool debug;

    // Input values
    double y_n;                 // measured process value
    double r_n;                 // reference (set point) value
    double y_scale;             // scale process input from property system
    double r_scale;             // scale reference input from property system
    double y_offset;
    double r_offset;

    // Configuration values
    double Kp;                  // proportional gain

    double alpha;               // low pass filter weighing factor (usually 0.1)
    double beta;                // process value weighing factor for
                                // calculating proportional error
                                // (usually 1.0)
    double gamma;               // process value weighing factor for
                                // calculating derivative error
                                // (usually 0.0)

    double Ti;                  // Integrator time (sec)
    double Td;                  // Derivator time (sec)

    double u_min;               // Minimum output clamp
    double u_max;               // Maximum output clamp

    // Previous state tracking values
    double ep_n_1;              // ep[n-1]  (prop error)
    double edf_n_1;             // edf[n-1] (derivative error)
    double edf_n_2;             // edf[n-2] (derivative error)
    double u_n_1;               // u[n-1]   (output)
    double desiredTs;            // desired sampling interval (sec)
    double elapsedTime;          // elapsed time (sec)
    
    
    
public:

    FGPIDController( SGPropertyNode *node );
    FGPIDController( SGPropertyNode *node, bool old );
    ~FGPIDController() {}

    void update_old( double dt );
    void update( double dt );
};


/**
 * A simplistic P [ + I ] PID controller
 */

class FGPISimpleController : public FGXMLAutoComponent {

private:

    // proportional component data
    bool proportional;
    double Kp;
    SGPropertyNode_ptr offset_prop;
    double offset_value;

    // integral component data
    bool integral;
    double Ki;
    double int_sum;

    // post functions for output
    bool clamp;

    // debug flag
    bool debug;

    // Input values
    double y_n;                 // measured process value
    double r_n;                 // reference (set point) value
    double y_scale;             // scale process input from property system
    double r_scale;             // scale reference input from property system

    double u_min;               // Minimum output clamp
    double u_max;               // Maximum output clamp

    
public:

    FGPISimpleController( SGPropertyNode *node );
    ~FGPISimpleController() {}

    void update( double dt );
};


/**
 * Predictor - calculates value in x seconds future.
 */

class FGPredictor : public FGXMLAutoComponent {

private:

    // proportional component data
    double last_value;
    double average;
    double seconds;
    double filter_gain;

    // debug flag
    bool debug;

    // Input values
    double ivalue;                 // input value
    
public:

    FGPredictor( SGPropertyNode *node );
    ~FGPredictor() {}

    void update( double dt );
};


/**
 * FGDigitalFilter - a selection of digital filters
 *
 * Exponential filter
 * Double exponential filter
 * Moving average filter
 * Noise spike filter
 *
 * All these filters are low-pass filters.
 *
 */

class FGDigitalFilter : public FGXMLAutoComponent
{
private:
    double Tf;            // Filter time [s]
    unsigned int samples; // Number of input samples to average
    double rateOfChange;  // The maximum allowable rate of change [1/s]
    deque <double> output;
    deque <double> input;
    enum filterTypes { exponential, doubleExponential, movingAverage, noiseSpike };
    filterTypes filterType;

    bool debug;

public:
    FGDigitalFilter(SGPropertyNode *node);
    ~FGDigitalFilter() {}

    void update(double dt);
};

/**
 * Model an autopilot system.
 * 
 */

class FGXMLAutopilot /* : public SGSubsystem */
{

public:

    FGXMLAutopilot();
    ~FGXMLAutopilot();

    void init();
    void reinit();
    void bind();
    void unbind();
    void update( double dt );

    bool build();

protected:

    typedef vector<FGXMLAutoComponent *> comp_list;

private:

    bool serviceable;
    SGPropertyNode_ptr config_props;
    comp_list components;
};


#endif // _XMLAUTO_HXX
