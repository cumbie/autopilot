/**
 * \file props_io.cxx
 * Started Fall 2000 by David Megginson, david@megginson.com
 * This code is released into the Public Domain.
 *
 * See props.html for documentation [replace with URL when available].
 *
 * $Id: props_io.cxx,v 1.1 2007/02/14 02:43:46 curt Exp $
 */

#include <stdlib.h>		// atof() atoi()

#include "util/sg_path.hxx"
#include "xml/easyxml.hxx"

#include "props.hxx"
#include "props_io.hxx"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

using std::istream;
using std::ifstream;
using std::ostream;
using std::ofstream;
using std::string;
using std::vector;
using std::map;
using std::endl;

#define DEFAULT_MODE (SGPropertyNode::READ|SGPropertyNode::WRITE)

// return the maximum of two values
template <class T>
inline T SG_MAX2(const T a, const T b) {
    return  a > b ? a : b;
}


////////////////////////////////////////////////////////////////////////
// Property list visitor, for XML parsing.
////////////////////////////////////////////////////////////////////////

class PropsVisitor : public XMLVisitor
{
public:

  PropsVisitor (SGPropertyNode * root, const string &base, int default_mode = 0)
    : _default_mode(default_mode), _root(root), _level(0), _base(base), _hasException(false) {}

  virtual ~PropsVisitor () {}

  void startXML ();
  void endXML ();
  void startElement (const char * name, const XMLAttributes &atts);
  void endElement (const char * name);
  void data (const char * s, int length);
  void warning (const char * message, int line, int column);

  bool hasException () const { return _hasException; }
  sg_io_exception &getException () { return _exception; }
  void setException (const sg_io_exception &exception) {
    _exception = exception;
    _hasException = true;
  }

private:

  struct State
  {
    State () : node(0), type(""), mode(DEFAULT_MODE) {}
    State (SGPropertyNode * _node, const char * _type, int _mode)
      : node(_node), type(_type), mode(_mode) {}
    SGPropertyNode * node;
    string type;
    int mode;
    map<string,int> counters;
  };

  State &state () { return _state_stack[_state_stack.size() - 1]; }

  void push_state (SGPropertyNode * node, const char * type, int mode) {
    if (type == 0)
      _state_stack.push_back(State(node, "unspecified", mode));
    else
      _state_stack.push_back(State(node, type, mode));
    _level++;
    _data = "";
  }

  void pop_state () {
    _state_stack.pop_back();
    _level--;
  }

  int _default_mode;
  string _data;
  SGPropertyNode * _root;
  int _level;
  vector<State> _state_stack;
  string _base;
  sg_io_exception _exception;
  bool _hasException;
};

void
PropsVisitor::startXML ()
{
  _level = 0;
  _state_stack.resize(0);
}

void
PropsVisitor::endXML ()
{
  _level = 0;
  _state_stack.resize(0);
}


/**
 * Check a yes/no flag, with default.
 */
static bool
checkFlag (const char * flag, bool defaultState = true)
{
  if (flag == 0)
    return defaultState;
  else if (!strcmp(flag, "y"))
    return true;
  else if (!strcmp(flag, "n"))
    return false;
  else {
    string message = "Unrecognized flag value '";
    message += flag;
    message += '\'';
				// FIXME: add location info
    throw sg_io_exception(message, "SimGear Property Reader");
  }
}

void
PropsVisitor::startElement (const char * name, const XMLAttributes &atts)
{
  const char * attval;

  if (_level == 0) {
    if (strcmp(name, "PropertyList")) {
      string message = "Root element name is ";
      message += name;
      message += "; expected PropertyList";
      throw sg_io_exception(message, "SimGear Property Reader");
    }

				// Check for an include.
    attval = atts.getValue("include");
    if (attval != 0) {
      SGPath path(SGPath(_base).dir());
      path.append(attval);
      try {
	readProperties(path.str(), _root);
      } catch (sg_io_exception &e) {
	setException(e);
      }
    }

    push_state(_root, "", DEFAULT_MODE);
  }

  else {
    State &st = state();
				// Get the index.
    attval = atts.getValue("n");
    int index = 0;
    if (attval != 0) {
      index = atoi(attval);
      st.counters[name] = SG_MAX2(st.counters[name], index+1);
    } else {
      index = st.counters[name];
      st.counters[name]++;
    }

				// Got the index, so grab the node.
    SGPropertyNode * node = st.node->getChild(name, index, true);

				// Get the access-mode attributes,
				// but don't set yet (in case they
				// prevent us from recording the value).
    int mode = _default_mode;

    attval = atts.getValue("read");
    if (checkFlag(attval, true))
      mode |= SGPropertyNode::READ;
    attval = atts.getValue("write");
    if (checkFlag(attval, true))
      mode |= SGPropertyNode::WRITE;
    attval = atts.getValue("archive");
    if (checkFlag(attval, false))
      mode |= SGPropertyNode::ARCHIVE;
    attval = atts.getValue("trace-read");
    if (checkFlag(attval, false))
      mode |= SGPropertyNode::TRACE_READ;
    attval = atts.getValue("trace-write");
    if (checkFlag(attval, false))
      mode |= SGPropertyNode::TRACE_WRITE;
    attval = atts.getValue("userarchive");
    if (checkFlag(attval, false))
      mode |= SGPropertyNode::USERARCHIVE;

				// Check for an alias.
    attval = atts.getValue("alias");
    if (attval != 0) {
      if (!node->alias(attval))
          printf("Failed to set alias to %s", attval);
    }

				// Check for an include.
    attval = atts.getValue("include");
    if (attval != 0) {
      SGPath path(SGPath(_base).dir());
      path.append(attval);
      try {
	readProperties(path.str(), node);
      } catch (sg_io_exception &e) {
	setException(e);
      }
    }

    const char *type = atts.getValue("type");
    if (type)
      node->clearValue();
    push_state(node, type, mode);
  }
}

void
PropsVisitor::endElement (const char * name)
{
  State &st = state();
  bool ret;

				// If there are no children and it's
				// not an alias, then it's a leaf value.
  if (st.node->nChildren() == 0 && !st.node->isAlias()) {
    if (st.type == "bool") {
      if (_data == "true" || atoi(_data.c_str()) != 0)
	ret = st.node->setBoolValue(true);
      else
	ret = st.node->setBoolValue(false);
    } else if (st.type == "int") {
      ret = st.node->setIntValue(atoi(_data.c_str()));
    } else if (st.type == "long") {
      ret = st.node->setLongValue(strtol(_data.c_str(), 0, 0));
    } else if (st.type == "float") {
      ret = st.node->setFloatValue(atof(_data.c_str()));
    } else if (st.type == "double") {
      ret = st.node->setDoubleValue(strtod(_data.c_str(), 0));
    } else if (st.type == "string") {
      ret = st.node->setStringValue(_data.c_str());
    } else if (st.type == "unspecified") {
      ret = st.node->setUnspecifiedValue(_data.c_str());
    } else {
      string message = "Unrecognized data type '";
      message += st.type;
      message += '\'';
				// FIXME: add location information
      throw sg_io_exception(message, "SimGear Property Reader");
    }
    if (!ret)
        printf("readProperties: Failed to set %s to value \"%s\" with type %s\n",
               st.node->getPath(), _data.c_str(), st.type.c_str());
  }

				// Set the access-mode attributes now,
				// once the value has already been 
				// assigned.
  st.node->setAttributes(st.mode);

  pop_state();
}

void
PropsVisitor::data (const char * s, int length)
{
  if (state().node->nChildren() == 0)
    _data.append(string(s, length));
}

void
PropsVisitor::warning (const char * message, int line, int column)
{
    printf("readProperties: warning: %s at line %d, column %d\n",
           message, line, column);
}



////////////////////////////////////////////////////////////////////////
// Property list reader.
////////////////////////////////////////////////////////////////////////


/**
 * Read properties from an input stream.
 *
 * @param input The input stream containing an XML property file.
 * @param start_node The root node for reading properties.
 * @param base A base path for resolving external include references.
 * @return true if the read succeeded, false otherwise.
 */
void
readProperties (istream &input, SGPropertyNode * start_node,
		const string &base, int default_mode)
{
  PropsVisitor visitor(start_node, base, default_mode);
  readXML(input, visitor, base);
  if (visitor.hasException())
    throw visitor.getException();
}


/**
 * Read properties from a file.
 *
 * @param file A string containing the file path.
 * @param start_node The root node for reading properties.
 * @return true if the read succeeded, false otherwise.
 */
void
readProperties (const string &file, SGPropertyNode * start_node,
                int default_mode)
{
  PropsVisitor visitor(start_node, file, default_mode);
  readXML(file, visitor);
  if (visitor.hasException())
    throw visitor.getException();
}


/**
 * Read properties from an in-memory buffer.
 *
 * @param buf A character buffer containing the xml data.
 * @param size The size/length of the buffer in bytes
 * @param start_node The root node for reading properties.
 * @return true if the read succeeded, false otherwise.
 */
void readProperties (const char *buf, const int size,
                     SGPropertyNode * start_node, int default_mode)
{
  PropsVisitor visitor(start_node, "", default_mode);
  readXML(buf, size, visitor);
  if (visitor.hasException())
    throw visitor.getException();
}


////////////////////////////////////////////////////////////////////////
// Property list writer.
////////////////////////////////////////////////////////////////////////

#define INDENT_STEP 2

/**
 * Return the type name.
 */
static const char *
getTypeName (SGPropertyNode::Type type)
{
  switch (type) {
  case SGPropertyNode::UNSPECIFIED:
    return "unspecified";
  case SGPropertyNode::BOOL:
    return "bool";
  case SGPropertyNode::INT:
    return "int";
  case SGPropertyNode::LONG:
    return "long";
  case SGPropertyNode::FLOAT:
    return "float";
  case SGPropertyNode::DOUBLE:
    return "double";
  case SGPropertyNode::STRING:
    return "string";
  case SGPropertyNode::ALIAS:
  case SGPropertyNode::NONE:
    return "unspecified";
  }

  // keep the compiler from squawking
  return "unspecified";
}


/**
 * Escape characters for output.
 */
static void
writeData (ostream &output, const string &data)
{
  for (int i = 0; i < (int)data.size(); i++) {
    switch (data[i]) {
    case '&':
      output << "&amp;";
      break;
    case '<':
      output << "&lt;";
      break;
    case '>':
      output << "&gt;";
      break;
    default:
      output << data[i];
      break;
    }
  }
}

static void
doIndent (ostream &output, int indent)
{
  while (indent-- > 0) {
    output << ' ';
  }
}


static void
writeAtts (ostream &output, const SGPropertyNode * node)
{
  int index = node->getIndex();

  if (index != 0)
    output << " n=\"" << index << '"';

#if 0
  if (!node->getAttribute(SGPropertyNode::READ))
    output << " read=\"n\"";

  if (!node->getAttribute(SGPropertyNode::WRITE))
    output << " write=\"n\"";

  if (node->getAttribute(SGPropertyNode::ARCHIVE))
    output << " archive=\"y\"";
#endif

}


/**
 * Test whether a node is archivable or has archivable descendants.
 */
static bool
isArchivable (const SGPropertyNode * node, SGPropertyNode::Attribute archive_flag)
{
  // FIXME: it's inefficient to do this all the time
  if (node->getAttribute(archive_flag))
    return true;
  else {
    int nChildren = node->nChildren();
    for (int i = 0; i < nChildren; i++)
      if (isArchivable(node->getChild(i), archive_flag))
	return true;
  }
  return false;
}


static bool
writeNode (ostream &output, const SGPropertyNode * node,
           bool write_all, int indent, SGPropertyNode::Attribute archive_flag)
{
				// Don't write the node or any of
				// its descendants unless it is
				// allowed to be archived.
  if (!write_all && !isArchivable(node, archive_flag))
    return true;		// Everything's OK, but we won't write.

  const string name = node->getName();
  int nChildren = node->nChildren();

				// If there is a literal value,
				// write it first.
  if (node->hasValue() && (write_all || node->getAttribute(archive_flag))) {
    doIndent(output, indent);
    output << '<' << name;
    writeAtts(output, node);
    if (node->isAlias() && node->getAliasTarget() != 0) {
      output << " alias=\"" << node->getAliasTarget()->getPath()
	     << "\"/>" << endl;
    } else {
      if (node->getType() != SGPropertyNode::UNSPECIFIED)
	output << " type=\"" << getTypeName(node->getType()) << '"';
      output << '>';
      writeData(output, node->getStringValue());
      output << "</" << name << '>' << endl;
    }
  }

				// If there are children, write them next.
  if (nChildren > 0) {
    doIndent(output, indent);
    output << '<' << name;
    writeAtts(output, node);
    output << '>' << endl;
    for (int i = 0; i < nChildren; i++)
      writeNode(output, node->getChild(i), write_all, indent + INDENT_STEP, archive_flag);
    doIndent(output, indent);
    output << "</" << name << '>' << endl;
  }

  return true;
}


void
writeProperties (ostream &output, const SGPropertyNode * start_node,
                 bool write_all, SGPropertyNode::Attribute archive_flag)
{
  int nChildren = start_node->nChildren();

  output << "<?xml version=\"1.0\"?>" << endl << endl;
  output << "<PropertyList>" << endl;

  for (int i = 0; i < nChildren; i++) {
    writeNode(output, start_node->getChild(i), write_all, INDENT_STEP, archive_flag);
  }

  output << "</PropertyList>" << endl;
}


void
writeProperties (const string &file, const SGPropertyNode * start_node,
                 bool write_all, SGPropertyNode::Attribute archive_flag)
{
  ofstream output(file.c_str());
  if (output.good()) {
    writeProperties(output, start_node, write_all, archive_flag);
  } else {
    throw sg_io_exception("Cannot open file", sg_location(file));
  }
}



////////////////////////////////////////////////////////////////////////
// Copy properties from one tree to another.
////////////////////////////////////////////////////////////////////////


/**
 * Copy one property tree to another.
 * 
 * @param in The source property tree.
 * @param out The destination property tree.
 * @return true if all properties were copied, false if some failed
 *  (for example, if the property's value is tied read-only).
 */
bool
copyProperties (const SGPropertyNode *in, SGPropertyNode *out)
{
  bool retval = true;

				// First, copy the actual value,
				// if any.
  if (in->hasValue()) {
    switch (in->getType()) {
    case SGPropertyNode::BOOL:
      if (!out->setBoolValue(in->getBoolValue()))
	retval = false;
      break;
    case SGPropertyNode::INT:
      if (!out->setIntValue(in->getIntValue()))
	retval = false;
      break;
    case SGPropertyNode::LONG:
      if (!out->setLongValue(in->getLongValue()))
	retval = false;
      break;
    case SGPropertyNode::FLOAT:
      if (!out->setFloatValue(in->getFloatValue()))
	retval = false;
      break;
    case SGPropertyNode::DOUBLE:
      if (!out->setDoubleValue(in->getDoubleValue()))
	retval = false;
      break;
    case SGPropertyNode::STRING:
      if (!out->setStringValue(in->getStringValue()))
	retval = false;
      break;
    case SGPropertyNode::UNSPECIFIED:
      if (!out->setUnspecifiedValue(in->getStringValue()))
	retval = false;
      break;
    default:
      if (in->isAlias())
	break;
      string message = "Unknown internal SGPropertyNode type";
      message += in->getType();
      throw sg_error(message, "SimGear Property Reader");
    }
  }

  				// copy the attributes.
  out->setAttributes( in->getAttributes() );

				// Next, copy the children.
  int nChildren = in->nChildren();
  for (int i = 0; i < nChildren; i++) {
    const SGPropertyNode * in_child = in->getChild(i);
    SGPropertyNode * out_child = out->getChild(in_child->getName(),
					       in_child->getIndex(),
					       true);
    if (!copyProperties(in_child, out_child))
      retval = false;
  }

  return retval;
}

// end of props_io.cxx
