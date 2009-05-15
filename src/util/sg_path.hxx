/**
 * \file sg_path.hxx
 * Routines to abstract out path separator differences between MacOS
 * and the rest of the world.
 */

// Written by Curtis L. Olson, started April 1999.
//
// Copyright (C) 1999  Curtis L. Olson - http://www.flightgear.org/~curt
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
// $Id: sg_path.hxx,v 1.1 2007/02/14 02:45:39 curt Exp $


#ifndef _SG_PATH_HXX
#define _SG_PATH_HXX

#include <sys/types.h>

#include <string>
#include <vector>

using std::string;
using std::vector;

#ifdef _MSC_VER
  typedef int mode_t;
#endif

typedef vector<string> string_list;


/**
 * A class to hide path separator difference across platforms and assist
 * in managing file system path names.
 *
 * Paths can be input in any platform format and will be converted
 * automatically to the proper format.
 */

class SGPath {

private:

    string path;

public:

    /** Default constructor */
    SGPath();

    /**
     * Construct a path based on the starting path provided.
     * @param p initial path
     */
    SGPath( const string& p );

    /** Destructor */
    ~SGPath();

    /**
     * Set path to a new value
     * @param p new path
     */
    void set( const string& p );
    SGPath& operator= ( const char* p ) { this->set(p); return *this; }

    /**
     * Append another piece to the existing path.  Inserts a path
     * separator between the existing component and the new component.
     * @param p additional path component */
    void append( const string& p );

    /**
     * Append a new piece to the existing path.  Inserts a search path
     * separator to the existing path and the new patch component.
     * @param p additional path component */
    void add( const string& p );

    /**
     * Concatenate a string to the end of the path without inserting a
     * path separator.
     * @param p addtional path suffix
     */
    void concat( const string& p );

    /**
     * Get the file part of the path (everything after the last path sep)
     * @return file string
     */
    string file() const;
  
    /**
     * Get the directory part of the path.
     * @return directory string
     */
    string dir() const;
  
    /**
     * Get the base part of the path (everything but the extension.)
     * @return the base string
     */
    string base() const;

    /**
     * Get the extention part of the path (everything after the final ".")
     * @return the extention string
     */
    string extension() const;

    /**
     * Get the path string
     * @return path string
     */
    string str() const { return path; }

    /**
     * Get the path string
     * @return path in "C" string (ptr to char array) form.
     */
    const char* c_str() { return path.c_str(); }

    /**
     * Determine if file exists by attempting to fopen it.
     * @return true if file exists, otherwise returns false.
     */
    bool exists() const;

    /**
     * Create the designated directory.
     */
    void create_dir(mode_t mode);

private:

    void fix();

};


/**
 * Split a directory string into a list of it's parent directories.
 */
string_list sgPathBranchSplit( const string &path );

/**
 * Split a directory search path into a vector of individual paths
 */
string_list sgPathSplit( const string &search_path );


#endif // _SG_PATH_HXX


