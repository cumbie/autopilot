// xmlauto.cxx - a more flexible, generic way to build autopilots
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
// $Id: xmlauto.cxx,v 1.8 2008/05/09 00:34:28 curt Exp $

#include <math.h>

#include <props/props_io.hxx>
#include <util/exception.hxx>
#include <util/sg_path.hxx>

#include "xmlauto.hxx"


FGPIDController::FGPIDController( SGPropertyNode *node ):
    debug( false ),
    y_n( 0.0 ),
    r_n( 0.0 ),
    y_scale( 1.0 ),
    r_scale( 1.0 ),
    y_offset( 0.0 ),
    r_offset( 0.0 ),
    Kp( 0.0 ),
    alpha( 0.1 ),
    beta( 1.0 ),
    gamma( 0.0 ),
    Ti( 0.0 ),
    Td( 0.0 ),
    u_min( 0.0 ),
    u_max( 0.0 ),
    ep_n_1( 0.0 ),
    edf_n_1( 0.0 ),
    edf_n_2( 0.0 ),
    u_n_1( 0.0 ),
    desiredTs( 0.0 ),
    elapsedTime( 0.0 )
{
    int i;
    for ( i = 0; i < node->nChildren(); ++i ) {
        SGPropertyNode *child = node->getChild(i);
        string cname = child->getName();
        string cval = child->getStringValue();
        if ( cname == "name" ) {
            name = cval;
        } else if ( cname == "debug" ) {
            debug = child->getBoolValue();
        } else if ( cname == "enable" ) {
            // cout << "parsing enable" << endl;
            SGPropertyNode *prop = child->getChild( "prop" );
            if ( prop != NULL ) {
                // cout << "prop = " << prop->getStringValue() << endl;
                enable_prop = fgGetNode( prop->getStringValue(), true );
            } else {
                // cout << "no prop child" << endl;
            }
            SGPropertyNode *val = child->getChild( "value" );
            if ( val != NULL ) {
                enable_value = val->getStringValue();
            }
            SGPropertyNode *pass = child->getChild( "honor-passive" );
            if ( pass != NULL ) {
                honor_passive = pass->getBoolValue();
            }
        } else if ( cname == "input" ) {
            SGPropertyNode *prop = child->getChild( "prop" );
            if ( prop != NULL ) {
                input_prop = fgGetNode( prop->getStringValue(), true );
            }
            prop = child->getChild( "scale" );
            if ( prop != NULL ) {
                y_scale = prop->getDoubleValue();
            }
            prop = child->getChild( "offset" );
            if ( prop != NULL ) {
                y_offset = prop->getDoubleValue();
            }
        } else if ( cname == "reference" ) {
            SGPropertyNode *prop = child->getChild( "prop" );
            if ( prop != NULL ) {
                r_n_prop = fgGetNode( prop->getStringValue(), true );
            } else {
                prop = child->getChild( "value" );
                if ( prop != NULL ) {
                    r_n_value = prop->getDoubleValue();
                }
            }
            prop = child->getChild( "scale" );
            if ( prop != NULL ) {
                r_scale = prop->getDoubleValue();
            }
            prop = child->getChild( "offset" );
            if ( prop != NULL ) {
                r_offset = prop->getDoubleValue();
            }
        } else if ( cname == "output" ) {
            int i = 0;
            SGPropertyNode *prop;
            while ( (prop = child->getChild("prop", i)) != NULL ) {
                SGPropertyNode *tmp = fgGetNode( prop->getStringValue(), true );
                output_list.push_back( tmp );
                i++;
            }
        } else if ( cname == "config" ) {
            SGPropertyNode *prop;

            prop = child->getChild( "Ts" );
            if ( prop != NULL ) {
                desiredTs = prop->getDoubleValue();
            }
            
            prop = child->getChild( "Kp" );
            if ( prop != NULL ) {
                Kp = prop->getDoubleValue();
            }

            prop = child->getChild( "beta" );
            if ( prop != NULL ) {
                beta = prop->getDoubleValue();
            }

            prop = child->getChild( "alpha" );
            if ( prop != NULL ) {
                alpha = prop->getDoubleValue();
            }

            prop = child->getChild( "gamma" );
            if ( prop != NULL ) {
                gamma = prop->getDoubleValue();
            }

            prop = child->getChild( "Ti" );
            if ( prop != NULL ) {
                Ti = prop->getDoubleValue();
            }

            prop = child->getChild( "Td" );
            if ( prop != NULL ) {
                Td = prop->getDoubleValue();
            }

            prop = child->getChild( "u_min" );
            if ( prop != NULL ) {
                u_min = prop->getDoubleValue();
            }

            prop = child->getChild( "u_max" );
            if ( prop != NULL ) {
                u_max = prop->getDoubleValue();
            }
        } else {
            printf("Error in autopilot config logic, " );
            if ( name.length() ) {
                printf("Section = %s", name.c_str() );
            }
        }
    }   
}


/*
 * Roy Vegard Ovesen:
 *
 * Ok! Here is the PID controller algorithm that I would like to see
 * implemented:
 *
 *   delta_u_n = Kp * [ (ep_n - ep_n-1) + ((Ts/Ti)*e_n)
 *               + (Td/Ts)*(edf_n - 2*edf_n-1 + edf_n-2) ]
 *
 *   u_n = u_n-1 + delta_u_n
 *
 * where:
 *
 * delta_u : The incremental output
 * Kp      : Proportional gain
 * ep      : Proportional error with reference weighing
 *           ep = beta * r - y
 *           where:
 *           beta : Weighing factor
 *           r    : Reference (setpoint)
 *           y    : Process value, measured
 * e       : Error
 *           e = r - y
 * Ts      : Sampling interval
 * Ti      : Integrator time
 * Td      : Derivator time
 * edf     : Derivate error with reference weighing and filtering
 *           edf_n = edf_n-1 / ((Ts/Tf) + 1) + ed_n * (Ts/Tf) / ((Ts/Tf) + 1)
 *           where:
 *           Tf : Filter time
 *           Tf = alpha * Td , where alpha usually is set to 0.1
 *           ed : Unfiltered derivate error with reference weighing
 *             ed = gamma * r - y
 *             where:
 *             gamma : Weighing factor
 * 
 * u       : absolute output
 * 
 * Index n means the n'th value.
 * 
 * 
 * Inputs:
 * enabled ,
 * y_n , r_n , beta=1 , gamma=0 , alpha=0.1 ,
 * Kp , Ti , Td , Ts (is the sampling time available?)
 * u_min , u_max
 * 
 * Output:
 * u_n
 */

void FGPIDController::update( double dt ) {
    double ep_n;            // proportional error with reference weighing
    double e_n;             // error
    double ed_n;            // derivative error
    double edf_n;           // derivative error filter
    double Tf;              // filter time
    double delta_u_n = 0.0; // incremental output
    double u_n = 0.0;       // absolute output
    double Ts;              // sampling interval (sec)
    
    elapsedTime += dt;
    if ( elapsedTime <= desiredTs ) {
        // do nothing if time step is not positive (i.e. no time has
        // elapsed)
        return;
    }
    Ts = elapsedTime;
    elapsedTime = 0.0;

    //if (enable_prop != NULL && enable_prop->getStringValue() == enable_value){
    if ( !enabled ) {
      // first time being enabled, seed u_n with current
      // property tree value
      u_n = output_list[0]->getDoubleValue();
      // and clip
      if ( u_n < u_min ) { u_n = u_min; }
      if ( u_n > u_max ) { u_n = u_max; }
      u_n_1 = u_n;
    }
    enabled = true;
    // } else {
    // enabled = false;
    // }

    if ( enabled && Ts > 0.0) {
        if ( debug ) printf("Updating %s Ts = %.2f", name.c_str(), Ts );

        double y_n = 0.0;
        if ( input_prop != NULL ) {
            y_n = input_prop->getFloatValue() * y_scale + y_offset;
        }

        double r_n = 0.0;
        if ( r_n_prop != NULL ) {
            r_n = r_n_prop->getFloatValue() * r_scale + r_offset;
        } else {
            r_n = r_n_value;
        }
                      
        if ( debug ) printf("  input = %.3f ref = %.3f\n", y_n, r_n );

        // Calculates proportional error:
        ep_n = beta * r_n - y_n;
        if ( debug ) printf( "  ep_n = %.3f", ep_n);
        if ( debug ) printf( "  ep_n_1 = %.3f", ep_n_1);

        // Calculates error:
        e_n = r_n - y_n;
        if ( debug ) printf( " e_n = %.3f", e_n);

        // Calculates derivate error:
        ed_n = gamma * r_n - y_n;
        if ( debug ) printf(" ed_n = %.3f", ed_n);

        if ( Td > 0.0 ) {
            // Calculates filter time:
            Tf = alpha * Td;
            if ( debug ) printf(" Tf = %.3f", Tf);

            // Filters the derivate error:
            edf_n = edf_n_1 / (Ts/Tf + 1)
                + ed_n * (Ts/Tf) / (Ts/Tf + 1);
            if ( debug ) printf(" edf_n = %.3f", edf_n);
        } else {
            edf_n = ed_n;
        }

        // Calculates the incremental output:
        if ( Ti > 0.0 ) {
            delta_u_n = Kp * ( (ep_n - ep_n_1)
                               + ((Ts/Ti) * e_n)
                               + ((Td/Ts) * (edf_n - 2*edf_n_1 + edf_n_2)) );
        }

        if ( debug ) {
	    printf(" delta_u_n = %.3f\n", delta_u_n);
            printf("P: %.3f  I: %.3f  D:%.3f\n",
		   Kp * (ep_n - ep_n_1),
		   Kp * ((Ts/Ti) * e_n),
		   Kp * ((Td/Ts) * (edf_n - 2*edf_n_1 + edf_n_2)));
        }

        // Integrator anti-windup logic:
        if ( delta_u_n > (u_max - u_n_1) ) {
            delta_u_n = u_max - u_n_1;
            if ( debug ) printf(" max saturation\n");
        } else if ( delta_u_n < (u_min - u_n_1) ) {
            delta_u_n = u_min - u_n_1;
            if ( debug ) printf(" min saturation\n");
        }

        // Calculates absolute output:
        u_n = u_n_1 + delta_u_n;
        if ( debug ) printf("  output = %.3f\n", u_n);

        // Updates indexed values;
        u_n_1   = u_n;
        ep_n_1  = ep_n;
        edf_n_2 = edf_n_1;
        edf_n_1 = edf_n;

	unsigned int i;
	for ( i = 0; i < output_list.size(); ++i ) {
	  output_list[i]->setDoubleValue( u_n );
	}
    } else if ( !enabled ) {
        ep_n  = 0.0;
        edf_n = 0.0;
        // Updates indexed values;
        u_n_1   = u_n;
        ep_n_1  = ep_n;
        edf_n_2 = edf_n_1;
        edf_n_1 = edf_n;
    }
}


FGPISimpleController::FGPISimpleController( SGPropertyNode *node ):
    proportional( false ),
    Kp( 0.0 ),
    offset_prop( NULL ),
    offset_value( 0.0 ),
    integral( false ),
    Ki( 0.0 ),
    int_sum( 0.0 ),
    clamp( false ),
    debug( false ),
    y_n( 0.0 ),
    r_n( 0.0 ),
    y_scale( 1.0 ),
    r_scale ( 1.0 ),
    u_min( 0.0 ),
    u_max( 0.0 )
{
    int i;
    for ( i = 0; i < node->nChildren(); ++i ) {
        SGPropertyNode *child = node->getChild(i);
        string cname = child->getName();
        string cval = child->getStringValue();
        if ( cname == "name" ) {
            name = cval;
        } else if ( cname == "debug" ) {
            debug = child->getBoolValue();
        } else if ( cname == "enable" ) {
            // cout << "parsing enable" << endl;
            SGPropertyNode *prop = child->getChild( "prop" );
            if ( prop != NULL ) {
                // cout << "prop = " << prop->getStringValue() << endl;
                enable_prop = fgGetNode( prop->getStringValue(), true );
            } else {
                // cout << "no prop child" << endl;
            }
            SGPropertyNode *val = child->getChild( "value" );
            if ( val != NULL ) {
                enable_value = val->getStringValue();
            }
        } else if ( cname == "input" ) {
            SGPropertyNode *prop = child->getChild( "prop" );
            if ( prop != NULL ) {
                input_prop = fgGetNode( prop->getStringValue(), true );
            }
            prop = child->getChild( "scale" );
            if ( prop != NULL ) {
                y_scale = prop->getDoubleValue();
            }
        } else if ( cname == "reference" ) {
            SGPropertyNode *prop = child->getChild( "prop" );
            if ( prop != NULL ) {
                r_n_prop = fgGetNode( prop->getStringValue(), true );
            } else {
                prop = child->getChild( "value" );
                if ( prop != NULL ) {
                    r_n_value = prop->getDoubleValue();
                }
            }
            prop = child->getChild( "scale" );
            if ( prop != NULL ) {
                r_scale = prop->getDoubleValue();
            }
        } else if ( cname == "output" ) {
            int i = 0;
            SGPropertyNode *prop;
            while ( (prop = child->getChild("prop", i)) != NULL ) {
                SGPropertyNode *tmp = fgGetNode( prop->getStringValue(), true );
                output_list.push_back( tmp );
                i++;
            }
        } else if ( cname == "config" ) {
            SGPropertyNode *prop;

            prop = child->getChild( "Kp" );
            if ( prop != NULL ) {
                Kp = prop->getDoubleValue();
                proportional = true;
            }

            prop = child->getChild( "Ki" );
            if ( prop != NULL ) {
                Ki = prop->getDoubleValue();
                integral = true;
            }

            prop = child->getChild( "u_min" );
            if ( prop != NULL ) {
                u_min = prop->getDoubleValue();
                clamp = true;
            }

            prop = child->getChild( "u_max" );
            if ( prop != NULL ) {
                u_max = prop->getDoubleValue();
                clamp = true;
            }
        } else {
            printf("Error in autopilot config logic, " );
            if ( name.length() ) {
                printf("Section = %s\n", name.c_str() );
            }
        }
    }   
}


void FGPISimpleController::update( double dt ) {
    //if (enable_prop != NULL && enable_prop->getStringValue() == enable_value){
    if ( !enabled ) {
      // we have just been enabled, zero out int_sum
      int_sum = 0.0;
    }
    enabled = true;
    // } else {
    //     enabled = false;
    // }

    if ( enabled ) {
        if ( debug ) printf("Updating %s\n", name.c_str());
        double input = 0.0;
        if ( input_prop != NULL ) {
            input = input_prop->getFloatValue() * y_scale;
        }

        double r_n = 0.0;
        if ( r_n_prop != NULL ) {
            r_n = r_n_prop->getFloatValue() * r_scale;
        } else {
            r_n = r_n_value;
        }
                      
        double error = r_n - input;
        if ( debug ) printf("input = %.3f reference = %.3f error = %.3f\n",
			    input, r_n, error);

        double prop_comp = 0.0;
        double offset = 0.0;
        if ( offset_prop != NULL ) {
            offset = offset_prop->getFloatValue();
            if ( debug ) printf("offset = %.3f\n", offset);
        } else {
            offset = offset_value;
        }

        if ( proportional ) {
            prop_comp = error * Kp + offset;
        }

        if ( integral ) {
            int_sum += error * Ki * dt;
        } else {
            int_sum = 0.0;
        }

        if ( debug ) printf("prop_comp = %.3f int_sum = %.3f\n",
			    prop_comp, int_sum);

        double output = prop_comp + int_sum;

        if ( clamp ) {
            if ( output < u_min ) {
                output = u_min;
            }
            if ( output > u_max ) {
                output = u_max;
            }
        }
        if ( debug ) printf("output = %.3f\n", output);

        unsigned int i;
        for ( i = 0; i < output_list.size(); ++i ) {
            output_list[i]->setDoubleValue( output );
        }
    }
}


FGPredictor::FGPredictor ( SGPropertyNode *node ):
    last_value ( 999999999.9 ),
    average ( 0.0 ),
    seconds( 0.0 ),
    filter_gain( 0.0 ),
    debug( false ),
    ivalue( 0.0 )
{
    int i;
    for ( i = 0; i < node->nChildren(); ++i ) {
        SGPropertyNode *child = node->getChild(i);
        string cname = child->getName();
        string cval = child->getStringValue();
        if ( cname == "name" ) {
            name = cval;
        } else if ( cname == "debug" ) {
            debug = child->getBoolValue();
        } else if ( cname == "input" ) {
            input_prop = fgGetNode( child->getStringValue(), true );
        } else if ( cname == "seconds" ) {
            seconds = child->getDoubleValue();
        } else if ( cname == "filter-gain" ) {
            filter_gain = child->getDoubleValue();
        } else if ( cname == "output" ) {
            SGPropertyNode *tmp = fgGetNode( child->getStringValue(), true );
            output_list.push_back( tmp );
        }
    }   
}

void FGPredictor::update( double dt ) {
    /*
       Simple moving average filter converts input value to predicted value "seconds".

       Smoothing as described by Curt Olson:
         gain would be valid in the range of 0 - 1.0
         1.0 would mean no filtering.
         0.0 would mean no input.
         0.5 would mean (1 part past value + 1 part current value) / 2
         0.1 would mean (9 parts past value + 1 part current value) / 10
         0.25 would mean (3 parts past value + 1 part current value) / 4

    */

    if ( input_prop != NULL ) {
        ivalue = input_prop->getDoubleValue();
        // no sense if there isn't an input :-)
        enabled = true;
    } else {
        enabled = false;
    }

    if ( enabled ) {

        // first time initialize average
        if (last_value >= 999999999.0) {
           last_value = ivalue;
        }

        if ( dt > 0.0 ) {
            double current = (ivalue - last_value)/dt; // calculate current error change (per second)
            if ( dt < 1.0 ) {
                average = (1.0 - dt) * average + current * dt;
            } else {
                average = current;
            }

            // calculate output with filter gain adjustment
            double output = ivalue + (1.0 - filter_gain) * (average * seconds) + filter_gain * (current * seconds);

            unsigned int i;
            for ( i = 0; i < output_list.size(); ++i ) {
                output_list[i]->setDoubleValue( output );
            }
        }
        last_value = ivalue;
    }
}


FGDigitalFilter::FGDigitalFilter(SGPropertyNode *node)
{
    samples = 1;

    int i;
    for ( i = 0; i < node->nChildren(); ++i ) {
        SGPropertyNode *child = node->getChild(i);
        string cname = child->getName();
        string cval = child->getStringValue();
        if ( cname == "name" ) {
            name = cval;
        } else if ( cname == "debug" ) {
            debug = child->getBoolValue();
        } else if ( cname == "type" ) {
            if ( cval == "exponential" ) {
                filterType = exponential;
            } else if (cval == "double-exponential") {
                filterType = doubleExponential;
            } else if (cval == "moving-average") {
                filterType = movingAverage;
            } else if (cval == "noise-spike") {
                filterType = noiseSpike;
            }
        } else if ( cname == "input" ) {
            input_prop = fgGetNode( child->getStringValue(), true );
        } else if ( cname == "filter-time" ) {
            Tf = child->getDoubleValue();
        } else if ( cname == "samples" ) {
            samples = child->getIntValue();
        } else if ( cname == "max-rate-of-change" ) {
            rateOfChange = child->getDoubleValue();
        } else if ( cname == "output" ) {
            SGPropertyNode *tmp = fgGetNode( child->getStringValue(), true );
            output_list.push_back( tmp );
        }
    }

    output.resize(2, 0.0);
    input.resize(samples + 1, 0.0);
}

void FGDigitalFilter::update(double dt)
{
    if ( input_prop != NULL ) {
        input.push_front(input_prop->getDoubleValue());
        input.resize(samples + 1, 0.0);
        // no sense if there isn't an input :-)
        enabled = true;
    } else {
        enabled = false;
    }

    if ( enabled && dt > 0.0 ) {
        /*
         * Exponential filter
         *
         * Output[n] = alpha*Input[n] + (1-alpha)*Output[n-1]
         *
         */

        if (filterType == exponential)
        {
            double alpha = 1 / ((Tf/dt) + 1);
            output.push_front(alpha * input[0] + 
                              (1 - alpha) * output[0]);
            unsigned int i;
            for ( i = 0; i < output_list.size(); ++i ) {
                output_list[i]->setDoubleValue( output[0] );
            }
            output.resize(1);
        } 
        else if (filterType == doubleExponential)
        {
            double alpha = 1 / ((Tf/dt) + 1);
            output.push_front(alpha * alpha * input[0] + 
                              2 * (1 - alpha) * output[0] -
                              (1 - alpha) * (1 - alpha) * output[1]);
            unsigned int i;
            for ( i = 0; i < output_list.size(); ++i ) {
                output_list[i]->setDoubleValue( output[0] );
            }
            output.resize(2);
        }
        else if (filterType == movingAverage)
        {
            output.push_front(output[0] + 
                              (input[0] - input.back()) / samples);
            unsigned int i;
            for ( i = 0; i < output_list.size(); ++i ) {
                output_list[i]->setDoubleValue( output[0] );
            }
            output.resize(1);
        }
        else if (filterType == noiseSpike)
        {
            double maxChange = rateOfChange * dt;

            if ((output[0] - input[0]) > maxChange)
            {
                output.push_front(output[0] - maxChange);
            }
            else if ((output[0] - input[0]) < -maxChange)
            {
                output.push_front(output[0] + maxChange);
            }
            else if (fabs(input[0] - output[0]) <= maxChange)
            {
                output.push_front(input[0]);
            }

            unsigned int i;
            for ( i = 0; i < output_list.size(); ++i ) {
                output_list[i]->setDoubleValue( output[0] );
            }
            output.resize(1);
        }
        if (debug)
        {
            printf("input: %.3f\toutput: %.3f\n", input[0], output[0]);
        }
    }
}


FGXMLAutopilot::FGXMLAutopilot() {
}


FGXMLAutopilot::~FGXMLAutopilot() {
}

 
void FGXMLAutopilot::init() {
    config_props = fgGetNode( "/autopilot/new-config", true );

    SGPropertyNode *root_n = fgGetNode("/config/root-path");
    SGPropertyNode *path_n = fgGetNode("/config/autopilot/path");

    if ( path_n ) {
        SGPath config( root_n->getStringValue() );
        config.append( path_n->getStringValue() );

        printf("Reading autopilot configuration from %s\n", config.c_str() );
        try {
            readProperties( config.str(), config_props );

            if ( ! build() ) {
	        printf("Detected an internal inconsistency in the autopilot\n");
                printf(" configuration.  See earlier errors for\n" );
                printf(" details.");
                exit(-1);
            }        
        } catch (const sg_exception& exc) {
	  printf("Failed to load autopilot configuration: %s\n",
		 config.c_str());
        }

    } else {
      printf("No autopilot configuration specified in master.xml file!");
    }
}


void FGXMLAutopilot::reinit() {
    components.clear();
    init();
    build();
}


void FGXMLAutopilot::bind() {
}

void FGXMLAutopilot::unbind() {
}

bool FGXMLAutopilot::build() {
    SGPropertyNode *node;
    int i;

    int count = config_props->nChildren();
    for ( i = 0; i < count; ++i ) {
        node = config_props->getChild(i);
        string name = node->getName();
        // cout << name << endl;
        if ( name == "pid-controller" ) {
            FGXMLAutoComponent *c = new FGPIDController( node );
            components.push_back( c );
        } else if ( name == "pi-simple-controller" ) {
            FGXMLAutoComponent *c = new FGPISimpleController( node );
            components.push_back( c );
        } else if ( name == "predict-simple" ) {
            FGXMLAutoComponent *c = new FGPredictor( node );
            components.push_back( c );
        } else if ( name == "filter" ) {
            FGXMLAutoComponent *c = new FGDigitalFilter( node );
            components.push_back( c );
        } else {
	  printf("Unknown top level section: %s\n", name.c_str() );
            return false;
        }
    }

    return true;
}


// normalize a value to lie between min and max
template <class T>
inline void SG_NORMALIZE_RANGE( T &val, const T min, const T max ) {
    T step = max - min;
    while( val >= max )  val -= step;
    while( val < min ) val += step;
};


/*
 * Update helper values
 */
static void update_helper( double dt ) {
    // Estimate speed in 5,10 seconds
    static SGPropertyNode *vel = fgGetNode( "/velocities/airspeed-kt", true );
    static SGPropertyNode *lookahead5
        = fgGetNode( "/autopilot/internal/lookahead-5-sec-airspeed-kt", true );
    static SGPropertyNode *lookahead10
        = fgGetNode( "/autopilot/internal/lookahead-10-sec-airspeed-kt", true );

    static double average = 0.0; // average/filtered prediction
    static double v_last = 0.0;  // last velocity

    if ( dt > 0.0 ) {
        double v = vel->getDoubleValue();
        double a = (v - v_last) / dt;

        if ( dt < 1.0 ) {
            average = (1.0 - dt) * average + dt * a;
        } else {
            average = a;
        }

        lookahead5->setDoubleValue( v + average * 5.0 );
        lookahead10->setDoubleValue( v + average * 10.0 );
        v_last = v;
    }

    // Calculate true heading error normalized to +/- 180.0
    static SGPropertyNode *target_true
        = fgGetNode( "/autopilot/settings/true-heading-deg", true );
    static SGPropertyNode *true_hdg
        = fgGetNode( "/orientation/groundtrack-deg", true );
    static SGPropertyNode *true_error
        = fgGetNode( "/autopilot/internal/true-heading-error-deg", true );

    double diff = target_true->getDoubleValue() - true_hdg->getDoubleValue();
    if ( diff < -180.0 ) { diff += 360.0; }
    if ( diff > 180.0 ) { diff -= 360.0; }
    true_error->setDoubleValue( diff );

    /* static int c = 0;
    c++;
    if ( c > 25 ) {
        printf("  tgt = %.1f  current = %.1f  error = %.1f\n",
               target_true->getDoubleValue(), true_hdg->getDoubleValue(),
               diff);
        c = 0;
        } */
}


/*
 * Update the list of autopilot components
 */

void FGXMLAutopilot::update( double dt ) {
    update_helper( dt );

    unsigned int i;
    for ( i = 0; i < components.size(); ++i ) {
        components[i]->update( dt );
    }

    /* static SGPropertyNode *debug
        = fgGetNode("/autopilot/internal/target-roll-deg");
    static int c = 0;
    c++;
    if ( c > 25 ) {
        printf("target roll = %.1f\n", debug->getDoubleValue());
        c = 0;
        } */

    /* static SGPropertyNode *debug1
        = fgGetNode("/position/altitude-agl-ft", true);
    static SGPropertyNode *debug2
        = fgGetNode("/autopilot/settings/target-agl-ft", true);
    printf("target agl = %.0f  current agl = %.0f\n", 
    debug2->getFloatValue(), debug1->getFloatValue()); */
}

