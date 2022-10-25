/*
 *
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
 * DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. THL A29 Limited designates
 * this particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#ifndef SHARE_VM_CLASSPATH_ENTRY_TABLE_HPP
#define SHARE_VM_CLASSPATH_ENTRY_TABLE_HPP

/*
 * WildcardEntryInfo: store the entry information in original class path
 * In java.c, the directory with wildcard will be converted to jar files 
 * in the directory. In different machine, the order may be different.
 */
class WildcardEntryInfo : public ResourceObj {
 private:
  char*   _path_name;
  int     _num_of_jar;          // the number of jar files in the directory, the value is 1 by default
  bool    _is_wildcard;         // the directory is with wildcard
 public:
  WildcardEntryInfo(char* name, bool is_wildcard) : _path_name(name), _is_wildcard(is_wildcard), _num_of_jar(1) {}
  void    set_num_of_jar(int number)     { _num_of_jar = number; }
  int     num_of_jar()                   { return _num_of_jar; }
  bool    is_wildcard()                  { return _is_wildcard; }
  char*   path_name()                    { return _path_name; }
  
};

class WildcardIterator : public StackObj {
  GrowableArray<WildcardEntryInfo*>* _merged_cp_array;
  WildcardEntryInfo* _wildcard_entry;
  int _cur_wildcard_entry;
  int _num_of_jar;

public:
  WildcardIterator(GrowableArray<WildcardEntryInfo*>* merged_cp_array) {
    _merged_cp_array = merged_cp_array;
    _cur_wildcard_entry = -1;
    _num_of_jar = 0;
    _wildcard_entry = NULL;
    // if merged_cp_array is null, num_of_jar is -1
    if (_merged_cp_array == NULL) {
      _num_of_jar = -1;
    }
  }

  WildcardEntryInfo* get_next_entry();
};

/*
 * ClassPathEntryTable
 * -- ClassPathEntryTableHeader
 *    -- entry table size
 *    -- entry table count
 *    -- classpath size
 *    -- classpath offset in ClassPathEntryTable
 * -- Array of ClassPathEntry Struct
 *    -- jar full path: offset, pointing to follwing strings
 *    -- jar timestamp: can be 0 if the jar file is in the directory with wildcard 
 *    --jar file size: can't be 0, it must be checked
 *    ...
 *    {}
 * -- Array of jar paths (string)
 *    -- jar_1 path
 *    -- jar_2 path
 *    ....
 *    -- jar_n path
 * -- Arguments::get_appclasspath() content
 */
class ClassPathEntryTable : public CHeapObj<mtInternal> {
 public:
  ClassPathEntryTable(char* start);
  bool   setup_classpath_entry_table();
  bool   setup_merged_classpath_entry_table();
  bool   validate();
  static GrowableArray<const char*>* create_path_array(const char* paths, int max_size);
  static GrowableArray<WildcardEntryInfo*>* create_merged_path_array(const char* paths, GrowableArray<const char*>* jar_files,
                                                                     bool allow_dir = false);
  // uttilities
  size_t size();
  void   print_on(outputStream* out);
 
  struct ClassPathEntry {
    int32_t _name_offset;       // offset from ClassPathEntryTableHeader's end to jar_x path
    time_t  _timestamp;         // jar timestamp
    long    _filesize;          // jar file size
    bool    need_order_check()  { return _timestamp != 0; }
  };

  struct ClassPathEntryTableHeader {
    size_t _classpath_entry_table_size;    // from Array of ClassPathEntry to last jar path
    int    _classpath_entry_table_count;
    size_t _classpath_size;                // Arguments::get_appclasspath() size
    size_t _classpath_offset;              // Arguments::get_appclasspath() offset from ClassPathEntry array start
  };
  static bool compare_jar_array(GrowableArray<const char*>* csa_jarfiles, GrowableArray<const char*>* cp_jarfiles);

 private:
  char*  _start;                           // mapped or write memory start for ClassPathEntryTable
  char*  _cur_pos;                         // current writing position
  char*  _classpath_entry_table;           // pointing to ClassPathEntry array start in memory
  ClassPathEntryTableHeader* _header;      // pointing to ClassPathEntryTableHeader in memory
  bool validate_app_class_paths(int shared_app_paths_len);
  bool validate_app_class_paths_for_merge(int shared_app_paths_len);
  bool check_paths(int num_paths, GrowableArray<const char*>* rp_array);
  bool check_paths_with_wildcard(int num_paths, GrowableArray<const char*>* merge_app_path_array,
                                 GrowableArray<WildcardEntryInfo*>* merged_cp_array);
  bool validate_file(ClassPathEntry* ent, char* file_name, bool check_timestamp);
};

#endif // SHARE_VM_CLASSPATH_ENTRY_TABLE_HPP
