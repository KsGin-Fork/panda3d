// Filename: dcPacker.cxx
// Created by:  drose (15Jun04)
//
////////////////////////////////////////////////////////////////////
//
// PANDA 3D SOFTWARE
// Copyright (c) 2001 - 2004, Disney Enterprises, Inc.  All rights reserved
//
// All use of this software is subject to the terms of the Panda 3d
// Software license.  You should have received a copy of this license
// along with this source code; you will also find a current copy of
// the license at http://etc.cmu.edu/panda3d/docs/license/ .
//
// To contact the maintainers of this program write to
// panda3d-general@lists.sourceforge.net .
//
////////////////////////////////////////////////////////////////////

#include "dcPacker.h"
#include "dcSwitch.h"
#include "dcParserDefs.h"
#include "dcLexerDefs.h"

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::Constructor
//       Access: Published
//  Description: 
////////////////////////////////////////////////////////////////////
DCPacker::
DCPacker() {
  _mode = M_idle;
  _unpack_data = NULL;
  _unpack_length = 0;
  _owns_unpack_data = false;
  _unpack_p = 0;
  _live_catalog = NULL;
  _pack_error = false;
  _range_error = false;
  
  clear();
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::Destructor
//       Access: Published
//  Description: 
////////////////////////////////////////////////////////////////////
DCPacker::
~DCPacker() {
  clear();
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::begin_pack
//       Access: Published
//  Description: Begins a packing session.  The parameter is the DC
//               object that describes the packing format; it may be a
//               DCParameter or DCField.
////////////////////////////////////////////////////////////////////
void DCPacker::
begin_pack(const DCPackerInterface *root) {
  nassertv(_mode == M_idle);
  
  _mode = M_pack;
  _pack_error = false;
  _range_error = false;
  _pack_data.clear();

  _root = root;
  _catalog = NULL;
  _live_catalog = NULL;

  _current_field = root;
  _current_parent = NULL;
  _current_field_index = 0;
  _num_nested_fields = 0;
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::end_pack
//       Access: Published, Virtual
//  Description: Finishes a packing session.
//
//               The return value is true on success, or false if
//               there has been some error during packing.
////////////////////////////////////////////////////////////////////
bool DCPacker::
end_pack() {
  nassertr(_mode == M_pack, false);
  
  _mode = M_idle;

  if (!_stack.empty() || _current_field != NULL || _current_parent != NULL) {
    _pack_error = true;
  }

  clear();

  return !_pack_error && !_range_error;
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::begin_unpack
//       Access: Published
//  Description: Begins an unpacking session.  Unlike the other
//               version of begin_unpack(), this version makes a copy
//               of the data string.
////////////////////////////////////////////////////////////////////
void DCPacker::
begin_unpack(const string &data, const DCPackerInterface *root) {
  _unpack_str = data;
  begin_unpack(_unpack_str.data(), _unpack_str.length(), root);
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::begin_unpack
//       Access: Public
//  Description: Begins an unpacking session.  The data pointer is
//               used directly; the data buffer is not copied.
//               Therefore, you must not delete or modify the data
//               pointer until you call end_unpack().
////////////////////////////////////////////////////////////////////
void DCPacker::
begin_unpack(const char *data, size_t length,
             const DCPackerInterface *root) {
  nassertv(_mode == M_idle);
  
  _mode = M_unpack;
  _pack_error = false;
  _range_error = false;
  set_unpack_data(data, length, false);
  _unpack_p = 0;

  _root = root;
  _catalog = NULL;
  _live_catalog = NULL;

  _current_field = root;
  _current_parent = NULL;
  _current_field_index = 0;
  _num_nested_fields = 0;
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::end_unpack
//       Access: Published
//  Description: Finishes the unpacking session.
//
//               The return value is true on success, or false if
//               there has been some error during unpacking (or if all
//               fields have not been unpacked).
////////////////////////////////////////////////////////////////////
bool DCPacker::
end_unpack() {
  nassertr(_mode == M_unpack, false);
  
  _mode = M_idle;

  if (!_stack.empty() || _current_field != NULL || _current_parent != NULL) {
    // This happens if we have not unpacked all of the fields.
    // However, this is not an error if we have called seek() during
    // the unpack session (in which case the _catalog will be
    // non-NULL).  On the other hand, if the catalog is still NULL,
    // then we have never called seek() and it is an error not to
    // unpack all values.
    if (_catalog == (DCPackerCatalog *)NULL) {
      _pack_error = true;
    }
  }

  clear();

  return !_pack_error && !_range_error;
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::begin_repack
//       Access: Published
//  Description: Begins an repacking session.  Unlike the other
//               version of begin_repack(), this version makes a copy
//               of the data string.
////////////////////////////////////////////////////////////////////
void DCPacker::
begin_repack(const string &data, const DCPackerInterface *root) {
  _unpack_str = data;
  begin_repack(_unpack_str.data(), _unpack_str.length(), root);
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::begin_repack
//       Access: Public
//  Description: Begins an repacking session.  The data pointer is
//               used directly; the data buffer is not copied.
//               Therefore, you must not delete or modify the data
//               pointer until you call end_repack().
//
//               When repacking, unlike in packing or unpacking modes,
//               you may not walk through the fields from beginning to
//               end, or even pack two consecutive fields at once.
//               Instead, you must call seek() for each field you wish
//               to modify and pack only that one field; then call
//               seek() again to modify another field.
////////////////////////////////////////////////////////////////////
void DCPacker::
begin_repack(const char *data, size_t length,
             const DCPackerInterface *root) {
  nassertv(_mode == M_idle);
  
  _mode = M_repack;
  _pack_error = false;
  _range_error = false;
  set_unpack_data(data, length, false);
  _unpack_p = 0;

  // In repack mode, we immediately get the catalog, since we know
  // we'll need it.
  _root = root;
  _catalog = _root->get_catalog();
  _live_catalog = _catalog->get_live_catalog(_unpack_data, _unpack_length);

  // We don't begin at the first field in repack mode.  Instead, you
  // must explicitly call seek().
  _current_field = NULL;
  _current_parent = NULL;
  _current_field_index = 0;
  _num_nested_fields = 0;
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::end_repack
//       Access: Published
//  Description: Finishes the repacking session.
//
//               The return value is true on success, or false if
//               there has been some error during repacking (or if all
//               fields have not been repacked).
////////////////////////////////////////////////////////////////////
bool DCPacker::
end_repack() {
  nassertr(_mode == M_repack, false);

  // Put the rest of the data onto the pack stream.
  _pack_data.append_data(_unpack_data + _unpack_p, _unpack_length - _unpack_p);
  
  _mode = M_idle;
  clear();

  return !_pack_error && !_range_error;
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::seek
//       Access: Published
//  Description: Sets the current unpack (or repack) position to the
//               named field.  In unpack mode, the next call to
//               unpack_*() or push() will begin to read the named
//               field.  In repack mode, the next call to pack_*() or
//               push() will modify the named field.
//
//               Returns true if successful, false if the field is not
//               known (or if the packer is in an invalid mode).
////////////////////////////////////////////////////////////////////
bool DCPacker::
seek(const string &field_name) {
  if (_mode == M_unpack) {
    if (_catalog == (DCPackerCatalog *)NULL) {
      _catalog = _root->get_catalog();
      _live_catalog = _catalog->get_live_catalog(_unpack_data, _unpack_length);
    }
    nassertr(_catalog != (DCPackerCatalog *)NULL, false);

    int entry_index = _live_catalog->find_entry_by_name(field_name);
    if (entry_index < 0) {
      // The field was not known.
      _pack_error = true;
      return false;
    }

    const DCPackerCatalog::Entry &entry = _live_catalog->get_entry(entry_index);

    // If we are seeking, we don't need to remember our current stack
    // position.
    _stack.clear();
    _current_field = entry._field;
    _current_parent = entry._parent;
    _current_field_index = entry._field_index;
    _num_nested_fields = _current_parent->get_num_nested_fields();
    _unpack_p = _live_catalog->get_begin(entry_index);

    // We don't really need _push_marker and _pop_marker now, except
    // that we should set _push_marker in case we have just seeked to
    // a switch parameter, and we should set _pop_marker to 0 just so
    // it won't get in the way.
    _push_marker = _unpack_p;
    _pop_marker = 0;

    return true;

  } else if (_mode == M_repack) {
    nassertr(_catalog != (DCPackerCatalog *)NULL, false);

    if (!_stack.empty() || _current_field != (DCPackerInterface *)NULL) {
      // It is an error to reseek while the stack is nonempty--that
      // means we haven't finished packing the current field.
      _pack_error = true;
      return false;
    }

    int entry_index = _live_catalog->find_entry_by_name(field_name);
    if (entry_index < 0) {
      // The field was not known.
      _pack_error = true;
      return false;
    }

    const DCPackerCatalog::Entry &entry = _live_catalog->get_entry(entry_index);

    if (((DCPackerInterface *)entry._parent)->as_switch() != (DCSwitch *)NULL) {
      // If the parent is a DCSwitch, that can only mean that the
      // seeked field is a switch parameter.  We can't support seeking
      // to a switch parameter and modifying it directly--what would
      // happen to all of the related fields?  Instead, you'll have to
      // seek to the switch itself and repack the whole entity.
      _pack_error = true;
      return false;
    }

    size_t begin = _live_catalog->get_begin(entry_index);
    if (begin < _unpack_p) {
      // Whoops, we are seeking fields out-of-order.  That means we
      // need to write the entire record and start again. 
      _pack_data.append_data(_unpack_data + _unpack_p, _unpack_length - _unpack_p);
      size_t length = _pack_data.get_length();
      char *buffer = _pack_data.take_data();
      set_unpack_data(buffer, length, true);
      _unpack_p = 0;

      _catalog->release_live_catalog(_live_catalog);
      _live_catalog = _catalog->get_live_catalog(_unpack_data, _unpack_length);

      begin = _live_catalog->get_begin(entry_index);
    }

    // Now copy the bytes from _unpack_p to begin from the
    // _unpack_data to the _pack_data.  These are the bytes we just
    // skipped over with the call to seek().
    _pack_data.append_data(_unpack_data + _unpack_p, begin - _unpack_p);

    // And set the packer up to pack the indicated field (but no
    // subsequent fields).
    _current_field = entry._field;
    _current_parent = entry._parent;
    _current_field_index = entry._field_index;
    _num_nested_fields = 1;
    _unpack_p = _live_catalog->get_end(entry_index);

    // Set up push_marker and pop_marker so we won't try to advance
    // beyond this field.
    _push_marker = begin;
    _pop_marker = _live_catalog->get_end(entry_index);

    return true;
  }

  // Invalid mode.
  _pack_error = true;
  return false;
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::push
//       Access: Published
//  Description: Marks the beginning of a nested series of fields.
//
//               This must be called before filling the elements of an
//               array or the individual fields in a structure field.
//               It must also be balanced by a matching pop().
//
//               It is necessary to use push() / pop() only if
//               has_nested_fields() returns true.
////////////////////////////////////////////////////////////////////
void DCPacker::
push() {
  if (!has_nested_fields()) {
    _pack_error = true;

  } else {
    StackElement element;
    element._current_parent = _current_parent;
    element._current_field_index = _current_field_index;
    element._push_marker = _push_marker;
    element._pop_marker = _pop_marker;
    _stack.push_back(element);
    _current_parent = _current_field;


    // Now deal with the length prefix that might or might not be
    // before a sequence of nested fields.
    int num_nested_fields = _current_parent->get_num_nested_fields();
    size_t length_bytes = _current_parent->get_num_length_bytes();
    
    if (_mode == M_pack || _mode == M_repack) {
      // Reserve length_bytes for when we figure out what the length
      // is.
      _push_marker = _pack_data.get_length();
      _pop_marker = 0;
      _pack_data.append_junk(length_bytes);

    } else if (_mode == M_unpack) {
      // Read length_bytes to determine the end of this nested
      // sequence.
      _push_marker = _unpack_p;
      _pop_marker = 0;

      if (length_bytes != 0) {
        if (_unpack_p + length_bytes > _unpack_length) {
          _pack_error = true;

        } else {
          size_t length;
          if (length_bytes == 4) {
            length = DCPackerInterface::do_unpack_uint32
              (_unpack_data + _unpack_p);
            _unpack_p += 4;
            _pop_marker = _unpack_p + length;
          } else {
            length = DCPackerInterface::do_unpack_uint16
              (_unpack_data + _unpack_p);
            _unpack_p += 2;
          }
          _pop_marker = _unpack_p + length;
        
          // The explicit length trumps the number of nested fields
          // reported by get_num_nested_fields().
          if (length == 0) {
            num_nested_fields = 0;
          } else {
            num_nested_fields = _current_parent->calc_num_nested_fields(length);
          }
        }
      }
    } else {
      _pack_error = true;
    }


    // Now point to the first field in the nested range.
    _num_nested_fields = num_nested_fields;
    _current_field_index = 0;

    if (_num_nested_fields >= 0 &&
        _current_field_index >= _num_nested_fields) {
      _current_field = NULL;
      
    } else {
      _current_field = _current_parent->get_nested_field(_current_field_index);
    }
  }
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::pop
//       Access: Published
//  Description: Marks the end of a nested series of fields.
//
//               This must be called to match a previous push() only
//               after all the expected number of nested fields have
//               been packed.  It is an error to call it too early, or
//               too late.
////////////////////////////////////////////////////////////////////
void DCPacker::
pop() {
  if (_current_field != NULL && _num_nested_fields >= 0) {
    // Oops, didn't pack or unpack enough values.
    _pack_error = true;

  } else if (_mode == M_unpack && _pop_marker != 0 && 
             _unpack_p != _pop_marker) {
    // Didn't unpack the right number of values.
    _pack_error = true;
  }

  if (_stack.empty()) {
    // Unbalanced pop().
    _pack_error = true;

  } else {
    if (!_current_parent->validate_num_nested_fields(_current_field_index)) {
      // Incorrect number of nested elements.
      _pack_error = true;
    }

    if (_mode == M_pack || _mode == M_repack) {
      size_t length_bytes = _current_parent->get_num_length_bytes();
      if (length_bytes != 0) {
        // Now go back and fill in the length of the array.
        size_t length = _pack_data.get_length() - _push_marker - length_bytes;
        if (length_bytes == 4) {
          DCPackerInterface::do_pack_uint32
            (_pack_data.get_rewrite_pointer(_push_marker, 4), length);
        } else {
          DCPackerInterface::validate_uint_limits(length, 16, _range_error);
          DCPackerInterface::do_pack_uint16
            (_pack_data.get_rewrite_pointer(_push_marker, 2), length);
        }
      }
    }

    _current_field = _current_parent;
    _current_parent = _stack.back()._current_parent;
    _current_field_index = _stack.back()._current_field_index;
    _push_marker = _stack.back()._push_marker;
    _pop_marker = _stack.back()._pop_marker;
    _num_nested_fields = (_current_parent == NULL) ? 0 : _current_parent->get_num_nested_fields();
    _stack.pop_back();
  }

  advance();
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::unpack_validate
//       Access: Published
//  Description: Internally unpacks the current numeric or string
//               value and validates it against the type range limits,
//               but does not return the value.  If the current field
//               contains nested fields, validates all of them.
////////////////////////////////////////////////////////////////////
void DCPacker::
unpack_validate() {
  nassertv(_mode == M_unpack);
  if (_current_field == NULL) {
    _pack_error = true;

  } else {
    if (_current_field->unpack_validate(_unpack_data, _unpack_length, _unpack_p,
                                        _pack_error, _range_error)) {
      advance();
    } else {
      // If the single field couldn't be validated, try validating
      // nested fields.
      push();
      while (more_nested_fields()) {
        unpack_validate();
      }
      pop();
    }
  }
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::unpack_skip
//       Access: Published
//  Description: Skips the current field without unpacking it and
//               advances to the next field.  If the current field
//               contains nested fields, skips all of them.
////////////////////////////////////////////////////////////////////
void DCPacker::
unpack_skip() {
  nassertv(_mode == M_unpack);
  if (_current_field == NULL) {
    _pack_error = true;

  } else if (_current_field->has_fixed_byte_size()) {
    _unpack_p += _current_field->get_fixed_byte_size();
    advance();

  } else {
    if (_current_field->unpack_skip(_unpack_data, _unpack_length, _unpack_p)) {
      advance();

    } else {
      // If the single field couldn't be skipped, try skipping nested fields.
      push();
      while (more_nested_fields()) {
        unpack_skip();
      }
      pop();
    }
  }
}

#ifdef HAVE_PYTHON
////////////////////////////////////////////////////////////////////
//     Function: DCPacker::pack_object
//       Access: Published
//  Description: Packs the Python object of whatever type into the
//               packer.  Each numeric object and string object maps
//               to the corresponding pack_value() call; a tuple or
//               sequence maps to a push() followed by all of the
//               tuple's contents followed by a pop().
////////////////////////////////////////////////////////////////////
void DCPacker::
pack_object(PyObject *object) {
  nassertv(_mode == M_pack || _mode == M_repack);
  PyObject *str = PyObject_Str(object);
  Py_DECREF(str);

  if (PyInt_Check(object)) {
    pack_int(PyInt_AS_LONG(object));

  } else if (PyFloat_Check(object)) {
    pack_double(PyFloat_AS_DOUBLE(object));

  } else if (PyLong_Check(object)) {
    pack_int64(PyLong_AsLongLong(object));

  } else if (PyString_Check(object) || PyUnicode_Check(object)) {
    char *buffer;
    int length;
    PyString_AsStringAndSize(object, &buffer, &length);
    if (buffer) {
      pack_string(string(buffer, length));
    }

  } else if (PySequence_Check(object)) {
    push();
    int size = PySequence_Size(object);
    for (int i = 0; i < size; i++) {
      PyObject *element = PySequence_GetItem(object, i);
      pack_object(element);
      Py_DECREF(element);
    }
    pop();
  }
}
#endif  // HAVE_PYTHON

#ifdef HAVE_PYTHON
////////////////////////////////////////////////////////////////////
//     Function: DCPacker::unpack_object
//       Access: Published
//  Description: Unpacks a Python object of the appropriate type from
//               the stream for the current field.  This may be an
//               integer or a string for a simple field object; if the
//               current field represents a list of fields it will be
//               a tuple.
////////////////////////////////////////////////////////////////////
PyObject *DCPacker::
unpack_object() {
  PyObject *object = NULL;

  DCPackType pack_type = get_pack_type();

  switch (pack_type) {
  case PT_invalid:
    object = Py_None;
    unpack_skip();
    break;

  case PT_double:
    {
      double value = unpack_double();
      object = PyFloat_FromDouble(value);
    }
    break;
      
  case PT_int:
    {
      int value = unpack_int();
      object = PyInt_FromLong(value);
    }
    break;
      
  case PT_uint:
    {
      unsigned int value = unpack_uint();
      if (value & 0x80000000) {
        object = PyLong_FromUnsignedLong(value);
      } else {
        object = PyInt_FromLong(value);
      }
    }
    break;
      
  case PT_int64:
    {
      PN_int64 value = unpack_int64();
      object = PyLong_FromLongLong(value);
    }
    break;
      
  case PT_uint64:
    {
      PN_uint64 value = unpack_uint64();
      object = PyLong_FromUnsignedLongLong(value);
    }
    break;

  case PT_string:
  case PT_blob:
    {
      string str;
      unpack_string(str);
      object = PyString_FromStringAndSize(str.data(), str.size());
    }
    break;

  default:
    {
      // First, build up a list from the nested objects.
      object = PyList_New(0);

      push();
      while (more_nested_fields()) {
        PyObject *element = unpack_object();
        PyList_Append(object, element);
        Py_DECREF(element);
      }
      pop();

      if (pack_type != PT_array) {
        // For these other kinds of objects, we'll convert the list
        // into a tuple.
        PyObject *tuple = PyList_AsTuple(object);
        Py_DECREF(object);
        object = tuple;
      }
    }
    break;
  }

  return object;
}
#endif  // HAVE_PYTHON


////////////////////////////////////////////////////////////////////
//     Function: DCPacker::parse_and_pack
//       Access: Published
//  Description: Parses an object's value according to the DC file
//               syntax (e.g. as a default value string) and packs it.
//               Returns true on success, false on a parse error.
////////////////////////////////////////////////////////////////////
bool DCPacker::
parse_and_pack(const string &formatted_object) {
  istringstream strm(formatted_object);
  return parse_and_pack(strm);
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::parse_and_pack
//       Access: Published
//  Description: Parses an object's value according to the DC file
//               syntax (e.g. as a default value string) and packs it.
//               Returns true on success, false on a parse error.
////////////////////////////////////////////////////////////////////
bool DCPacker::
parse_and_pack(istream &in) {
  dc_init_parser_parameter_value(in, "parse_and_pack", *this);
  dcyyparse();
  dc_cleanup_parser();

  return (dc_error_count() == 0);
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::unpack_and_format
//       Access: Published
//  Description: Unpacks an object and formats its value into a syntax
//               suitable for parsing in the dc file (e.g. as a
//               default value), or as an input to parse_object.
////////////////////////////////////////////////////////////////////
string DCPacker::
unpack_and_format() {
  ostringstream strm;
  unpack_and_format(strm);
  return strm.str();
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::unpack_and_format
//       Access: Published
//  Description: Unpacks an object and formats its value into a syntax
//               suitable for parsing in the dc file (e.g. as a
//               default value), or as an input to parse_object.
////////////////////////////////////////////////////////////////////
void DCPacker::
unpack_and_format(ostream &out) {
  DCPackType pack_type = get_pack_type();

  switch (pack_type) {
  case PT_invalid:
    out << "<invalid>";
    break;

  case PT_double:
    out << unpack_double();
    break;
      
  case PT_int:
    out << unpack_int();
    break;
      
  case PT_uint:
    out << unpack_uint();
    break;
      
  case PT_int64:
    out << unpack_int64();
    break;
      
  case PT_uint64:
    out << unpack_uint64();
    break;

  case PT_string:
    enquote_string(out, '"', unpack_string());
    break;

  case PT_blob:
    output_hex_string(out, unpack_literal_value());
    break;

  default:
    {
      switch (pack_type) {
      case PT_array:
        out << '[';
        break;

      case PT_field:
      case PT_switch:
        out << '(';
        break;

      case PT_class:
      default:
        out << '{';
        break;
      }

      push();
      while (more_nested_fields()) {
        unpack_and_format(out);

        if (more_nested_fields()) {
          out << ", ";
        }
      }
      pop();

      switch (pack_type) {
      case PT_array:
        out << ']';
        break;

      case PT_field:
      case PT_switch:
        out << ')';
        break;

      case PT_class:
      default:
        out << '}';
        break;
      }
    }
    break;
  }
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::enquote_string
//       Access: Public, Static
//  Description: Outputs the indicated string within quotation marks.
////////////////////////////////////////////////////////////////////
void DCPacker::
enquote_string(ostream &out, char quote_mark, const string &str) {
  out << quote_mark;
  for (string::const_iterator pi = str.begin();
       pi != str.end();
       ++pi) {
    if ((*pi) == quote_mark || (*pi) == '\\') {
      out << '\\' << (*pi);

    } else if (!isprint(*pi)) {
      char buffer[10];
      sprintf(buffer, "%02x", (unsigned int)(*pi));
      out << "\\x" << buffer;

    } else {
      out << (*pi);
    }
  }
  out << quote_mark;
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::output_hex_string
//       Access: Public, Static
//  Description: Outputs the indicated string as a hex constant.
////////////////////////////////////////////////////////////////////
void DCPacker::
output_hex_string(ostream &out, const string &str) {
  out << '<';
  for (string::const_iterator pi = str.begin();
       pi != str.end();
       ++pi) {
    char buffer[10];
    sprintf(buffer, "%02x", (unsigned int)(*pi));
    out << buffer;
  }
  out << '>';
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::handle_switch
//       Access: Private
//  Description: When we advance past the key field on a switch
//               record, we suddenly have more fields available--all
//               the appropriate alternate fields in the switch.
//
//               This function is called when we detect this
//               condition; it switches the _current_parent to the
//               appropriate case of the switch record.
////////////////////////////////////////////////////////////////////
void DCPacker::
handle_switch(const DCSwitch *dswitch) {
  // First, get the value from the key.  This is either found in the
  // unpack or the pack data, depending on what mode we're in.
  const DCPackerInterface *new_parent = NULL;

  if (_mode == M_pack || _mode == M_repack) {
    const char *data = _pack_data.get_data();
    new_parent = dswitch->apply_switch(data + _push_marker,
                                       _pack_data.get_length() - _push_marker);

  } else if (_mode == M_unpack) {
    new_parent = dswitch->apply_switch(_unpack_data + _push_marker,
                                       _unpack_p - _push_marker);

  }

  if (new_parent == (DCPackerInterface *)NULL) {
    // This means an invalid value was packed for the key.
    _pack_error = true;
    return;
  }

  _last_switch = dswitch;

  // Now substitute in the switch case for the previous parent (which
  // replaces the switch node itself).  This will suddenly make a slew
  // of new fields appear.
  _current_parent = new_parent;
  _num_nested_fields = _current_parent->get_num_nested_fields();

  if (_num_nested_fields < 0 ||
      _current_field_index < _num_nested_fields) {
    _current_field = _current_parent->get_nested_field(_current_field_index);
  }
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::clear
//       Access: Private
//  Description: Resets the data structures after a pack or unpack
//               sequence.
////////////////////////////////////////////////////////////////////
void DCPacker::
clear() {
  _stack.clear();
  _current_field = NULL;
  _current_parent = NULL;
  _current_field_index = 0;
  _num_nested_fields = 0;
  _push_marker = 0;
  _pop_marker = 0;
  _last_switch = NULL;

  if (_live_catalog != (DCPackerCatalog::LiveCatalog *)NULL) {
    _catalog->release_live_catalog(_live_catalog);
    _live_catalog = NULL;
  }
  _catalog = NULL;
  _root = NULL;

  set_unpack_data(NULL, 0, false);
}

////////////////////////////////////////////////////////////////////
//     Function: DCPacker::set_unpack_data
//       Access: Private
//  Description: Sets up the unpack_data pointer.
////////////////////////////////////////////////////////////////////
void DCPacker::
set_unpack_data(const char *unpack_data, size_t unpack_length, 
                bool owns_unpack_data) {
  if (_owns_unpack_data) {
    delete[] _unpack_data;
  }
  _unpack_data = unpack_data;
  _unpack_length = unpack_length;
  _owns_unpack_data = owns_unpack_data;
}
