/*
 * Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "classfile/classLoader.hpp"
#include "classfile/classLoaderExt.hpp"
#include "classfile/sharedClassUtil.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionaryShared.hpp"
#include "classfile/altHashing.hpp"
#include "memory/filemap.hpp"
#include "memory/metadataFactory.hpp"
#include "memory/oopFactory.hpp"
#include "oops/objArrayOop.hpp"
#include "runtime/arguments.hpp"
#include "runtime/java.hpp"
#include "runtime/os.hpp"
#include "services/memTracker.hpp"
#include "utilities/defaultStream.hpp"

# include <sys/stat.h>
# include <errno.h>

#ifndef O_BINARY       // if defined (Win32) use binary files.
#define O_BINARY 0     // otherwise do nothing.
#endif

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC
extern address JVM_FunctionAtStart();
extern address JVM_FunctionAtEnd();

// Complain and stop. All error conditions occurring during the writing of
// an archive file should stop the process.  Unrecoverable errors during
// the reading of the archive file should stop the process.

static void fail(const char *msg, va_list ap) {
  // This occurs very early during initialization: tty is not initialized.
  jio_fprintf(defaultStream::error_stream(),
              "An error has occurred while processing the"
              " shared archive file.\n");
  jio_vfprintf(defaultStream::error_stream(), msg, ap);
  jio_fprintf(defaultStream::error_stream(), "\n");
  // Do not change the text of the below message because some tests check for it.
  vm_exit_during_initialization("Unable to use shared archive.", NULL);
}


void FileMapInfo::fail_stop(const char *msg, ...) {
        va_list ap;
  va_start(ap, msg);
  fail(msg, ap);        // Never returns.
  va_end(ap);           // for completeness.
}


// Complain and continue.  Recoverable errors during the reading of the
// archive file may continue (with sharing disabled).
//
// If we continue, then disable shared spaces and close the file.

void FileMapInfo::fail_continue(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  MetaspaceShared::set_archive_loading_failed();
  if (PrintSharedArchiveAndExit && _validating_classpath_entry_table) {
    // If we are doing PrintSharedArchiveAndExit and some of the classpath entries
    // do not validate, we can still continue "limping" to validate the remaining
    // entries. No need to quit.
    tty->print("[");
    tty->vprint(msg, ap);
    tty->print_cr("]");
  } else {
    if (RequireSharedSpaces) {
      fail(msg, ap);
    } else {
      if (PrintSharedSpaces || RelaxCheckForAppCDS) {
        tty->print_cr("UseSharedSpaces: %s", msg);
      }
    }
    UseSharedSpaces = false;
    assert(current_info() != NULL, "singleton must be registered");
    current_info()->close();
  }
  va_end(ap);
}

// Fill in the fileMapInfo structure with data about this VM instance.

// This method copies the vm version info into header_version.  If the version is too
// long then a truncated version, which has a hash code appended to it, is copied.
//
// Using a template enables this method to verify that header_version is an array of
// length JVM_IDENT_MAX.  This ensures that the code that writes to the CDS file and
// the code that reads the CDS file will both use the same size buffer.  Hence, will
// use identical truncation.  This is necessary for matching of truncated versions.
template <int N> static void get_header_version(char (&header_version) [N]) {
  assert(N == JVM_IDENT_MAX, "Bad header_version size");

  const char *vm_version = VM_Version::internal_vm_info_string();
  const int version_len = (int)strlen(vm_version);

  memset(header_version, 0, JVM_IDENT_MAX);

  if (version_len < (JVM_IDENT_MAX-1)) {
    strcpy(header_version, vm_version);

  } else {
    // Get the hash value.  Use a static seed because the hash needs to return the same
    // value over multiple jvm invocations.
    uint32_t hash = AltHashing::halfsiphash_32(8191, (const uint8_t*)vm_version, version_len);

    // Truncate the ident, saving room for the 8 hex character hash value.
    strncpy(header_version, vm_version, JVM_IDENT_MAX-9);

    // Append the hash code as eight hex digits.
    sprintf(&header_version[JVM_IDENT_MAX-9], "%08x", hash);
    header_version[JVM_IDENT_MAX-1] = 0;  // Null terminate.
  }
}

FileMapInfo::FileMapInfo() {
  assert(_current_info == NULL, "must be singleton"); // not thread safe
  _current_info = this;
  memset(this, 0, sizeof(FileMapInfo));
  _file_offset = 0;
  _file_open = false;
  _header = SharedClassUtil::allocate_file_map_header();
  _header->_version = _invalid_version;
  _header->_has_ext_or_app_classes = false;
  _header->_use_appcds = UseAppCDS;
}

FileMapInfo::~FileMapInfo() {
  assert(_current_info == this, "must be singleton"); // not thread safe
  _current_info = NULL;
}

void FileMapInfo::populate_header(size_t alignment) {
  _header->populate(this, alignment);
}

size_t FileMapInfo::FileMapHeader::data_size() {
  return SharedClassUtil::file_map_header_size() - sizeof(FileMapInfo::FileMapHeaderBase);
}

void FileMapInfo::FileMapHeader::populate(FileMapInfo* mapinfo, size_t alignment) {
  _magic = 0xf00baba2;
  _version = _current_version;
  _alignment = alignment;
  _obj_alignment = ObjectAlignmentInBytes;
  _classpath_entry_table_size = mapinfo->_classpath_entry_table_size;
  _classpath_entry_table = mapinfo->_classpath_entry_table;
  _classpath_entry_size = mapinfo->_classpath_entry_size;

  // The following fields are for sanity checks for whether this archive
  // will function correctly with this JVM and the bootclasspath it's
  // invoked with.

  // JVM version string ... changes on each build.
  get_header_version(_jvm_ident);

  _app_class_paths_start_index = ClassLoaderExt::app_class_paths_start_index();
  _has_ext_or_app_classes = ClassLoaderExt::has_ext_or_app_classes();
}

void SharedClassPathEntry::init(ClassPathEntry *cpe, const char* name) {
  assert(DumpSharedSpaces, "dump time only");
  _timestamp = 0;
  _filesize  = 0;
  _from_class_path_attr = false;

  if (cpe->is_jar_file()) {
    struct stat st;
    if (os::stat(name, &st) != 0) {
      // The file/dir must exist, or it would not have been added
      // into ClassLoader::classpath_entry().
      //
      // If we can't access a jar file in the boot path, then we can't
      // make assumptions about where classes get loaded from.
      FileMapInfo::fail_stop("Unable to open jar file %s.", name);
    }
    _type = jar_entry;
    _from_class_path_attr = cpe->from_class_path_attr();
    EXCEPTION_MARK; // The following call should never throw, but would exit VM on error.
    SharedClassUtil::update_shared_classpath(cpe, this, st.st_mtime, st.st_size, THREAD);
  } else {
    _filesize  = -1;
    if (!UseAppCDS && !os::dir_is_empty(name)) {
      ClassLoader::exit_with_path_failure("Cannot have non-empty directory in archived classpaths", name);
    }
    _type = dir_entry;
  }
}

bool SharedClassPathEntry::validate() {
  assert(UseSharedSpaces, "runtime only");

  struct stat st;
  const char* name;

  name = this->name();

  bool ok = true;
  if (TraceClassPaths) {
    tty->print_cr("checking shared classpath entry: %s", name);
  }
  if (os::stat(name, &st) != 0) {
    FileMapInfo::fail_continue("Required classpath entry does not exist: %s", name);
    ok = false;
  } else if (is_dir()) {
    ok = true;
  } else if ((has_timestamp() && _timestamp != st.st_mtime) ||
             _filesize != st.st_size) {
    ok = false;
    if (PrintSharedArchiveAndExit) {
      FileMapInfo::fail_continue(_timestamp != st.st_mtime ?
                                 "Timestamp mismatch" :
                                 "File size mismatch");
    } else {
      FileMapInfo::fail_continue("A jar file is not the one used while building"
                                 " the shared archive file: %s", name);
    }
  }
  return ok;
}

class ManifestStream: public ResourceObj {
  private:
  u1*   _buffer_start; // Buffer bottom
  u1*   _buffer_end;   // Buffer top (one past last element)
  u1*   _current;      // Current buffer position

 public:
  // Constructor
  ManifestStream(u1* buffer, int length) : _buffer_start(buffer),
                                           _current(buffer) {
    _buffer_end = buffer + length;
  }

  // The return value indicates if the JAR is signed or not
  bool check_is_signed() {
    u1* attr = _current;
    bool isSigned = false;
    while (_current < _buffer_end) {
      if (*_current == '\n') {
        *_current = '\0';
        u1* value = (u1*)strchr((char*)attr, ':');
        if (value != NULL) {
          assert(*(value+1) == ' ', "Unrecognized format" );
          if (strstr((char*)attr, "-Digest") != NULL) {
            isSigned = true;
            break;
          }
        }
        *_current = '\n'; // restore
        attr = _current + 1;
      }
      _current ++;
    }
    return isSigned;
  }
};

void FileMapInfo::allocate_classpath_entry_table() {
  int bytes = 0;
  int count = 0;
  int manifest_byte = 0;
  char* strptr = NULL;
  char* strptr_max = NULL;
  char* manifestptr = NULL;
  char* manifestptr_max = NULL;
  ManifestEntry* manifest_array = NULL;
  Thread* THREAD = Thread::current();

  ClassLoaderData* loader_data = ClassLoaderData::the_null_class_loader_data();
  size_t entry_size = SharedClassUtil::shared_class_path_entry_size();

  for (int pass=0; pass<2; pass++) {
    int cur_entry = 0;
    ClassPathEntry *cpe = ClassLoader::classpath_entry(0);

    for (cur_entry = 0; cpe != NULL; cpe = cpe->next(), cur_entry++) {
      const char *name = cpe->name();
      int name_bytes = (int)(strlen(name) + 1);

      if (pass == 0) {
        count ++;
        bytes += (int)entry_size;
        bytes += name_bytes;
        if (TraceClassPaths || (TraceClassLoading && Verbose)) {
          tty->print_cr("[Add main shared path (%s) %s]", (cpe->is_jar_file() ? "jar" : "dir"), name);
        }
      } else {
        SharedClassPathEntry* ent = shared_classpath(cur_entry);
        ent->init(cpe, name);
        ent->_name = strptr;
        if (strptr + name_bytes <= strptr_max) {
          strncpy(strptr, name, (size_t)name_bytes); // name_bytes includes trailing 0.
          strptr += name_bytes;
        } else {
          assert(0, "miscalculated buffer size");
        }
      }
    }

    if (UseAppCDS) {
      ClassPathEntry *acpe = ClassLoader::app_classpath_entries();
      if (pass == 0) {
        int num_app_classpath_entries = ClassLoader::num_app_classpath_entries();
        manifest_array = NEW_C_HEAP_ARRAY(ManifestEntry, num_app_classpath_entries, mtInternal);
      }
      for (int cur_app_entry = 0; acpe != NULL; acpe = acpe->next(), cur_entry++, cur_app_entry++) {
        const char *name = acpe->name();
        int name_bytes = (int)(strlen(name) + 1);

        if (pass == 0) {
          count ++;
          bytes += (int)entry_size;
          bytes += name_bytes;
          EXCEPTION_MARK;
          manifest_byte += populate_manifest_buffer(acpe, manifest_array, cur_app_entry, THREAD);
          if (TraceClassPaths || (TraceClassLoading && Verbose)) {
            tty->print_cr("[Add app shared path (%s) %s]", (acpe->is_jar_file() ? "jar" : "dir"), name);
          }
        } else {
          SharedClassPathEntry* ent = shared_classpath(cur_entry);
          ent->init(acpe, name);
          ent->_name = strptr;
          if (strptr + name_bytes <= strptr_max) {
            strncpy(strptr, name, (size_t)name_bytes); // name_bytes includes trailing 0.
            strptr += name_bytes;
          } else {
            assert(0, "miscalculated buffer size");
          }
          if (manifest_array[cur_app_entry]._has_manifest) {
            int mani_bytes = manifest_array[cur_app_entry]._size;
            ent->_manifest = manifestptr;
            if (manifestptr + mani_bytes <= manifestptr_max) {
              strncpy(manifestptr, manifest_array[cur_app_entry]._manifest, (size_t)mani_bytes); // mani_bytes includes trailing 0.
              free(manifest_array[cur_app_entry]._manifest);
              manifestptr += mani_bytes;
            } else {
              assert(0, "miscalculated buffer size");
            }
          } else {
            ent->_manifest = NULL;
            if (manifest_array[cur_app_entry]._is_signed) {
              ent->set_is_signed();
            }
          }
        }
      }
    }

    if (pass == 0) {
      int all_bytes = 0;
      all_bytes = bytes + manifest_byte;
      EXCEPTION_MARK; // The following call should never throw, but would exit VM on error.
      Array<u8>* arr = MetadataFactory::new_array<u8>(loader_data, (all_bytes + 7)/8, THREAD);
      strptr = (char*)(arr->data());
      strptr_max = strptr + bytes;
      SharedClassPathEntry* table = (SharedClassPathEntry*)strptr;
      strptr += entry_size * count;
      manifestptr = strptr_max;
      manifestptr_max = strptr_max + manifest_byte;

      _classpath_entry_table_size = count;
      _classpath_entry_table = table;
      _classpath_entry_size = entry_size;
    } else {
      FREE_C_HEAP_ARRAY(ManifestEntry, manifest_array, mtInternal); 
    }
  }
}

int FileMapInfo::populate_manifest_buffer(ClassPathEntry *cpe, ManifestEntry* manifests, int current, TRAPS) {
  assert(UseAppCDS, "must be");
  ClassLoaderData* loader_data = ClassLoaderData::the_null_class_loader_data();
  ResourceMark rm(THREAD);
  jint manifest_size = 0;
  manifests[current]._has_manifest = false;
  manifests[current]._is_signed = false;
  manifests[current]._manifest = NULL;
  manifests[current]._size = 0;

  if (cpe->is_jar_file()) {
    char* manifest = ClassLoaderExt::read_manifest(cpe, &manifest_size, CHECK_AND_CLEAR_0);
    if (manifest != NULL) {
      ManifestStream* stream = new ManifestStream((u1*)manifest,
                                                  manifest_size);
      if (stream->check_is_signed()) {
        manifests[current]._is_signed = true;
      } else {
        // Copy the manifest into the shared archive
        manifest = ClassLoaderExt::read_raw_manifest(cpe, &manifest_size, CHECK_AND_CLEAR_0);
        manifest_size += 1;
        manifests[current]._manifest = (char *)malloc(manifest_size);
        strncpy(manifests[current]._manifest, manifest, manifest_size);
        manifests[current]._size = manifest_size;
        manifests[current]._has_manifest = true;
      }
    }
  }
  return manifests[current]._size;
}

bool FileMapInfo::validate_classpath_entry_table() {
  _validating_classpath_entry_table = true;

  int count = _header->_classpath_entry_table_size;
  int start_app_index = _header->_app_class_paths_start_index;
  int shared_app_paths_len = 0;

  _classpath_entry_table = _header->_classpath_entry_table;
  _classpath_entry_size = _header->_classpath_entry_size;

  for (int i=0; i<count; i++) {
    SharedClassPathEntry* ent = shared_classpath(i);
    struct stat st;
    const char* org_name = ent->_name;
    bool ok = true;
    bool app_check = false;
    char name[JVM_MAXPATHLEN];

    if (UseAppCDS && i >= start_app_index) {
      app_check = true;
    } else if (!os::correct_cds_path(org_name, name, JVM_MAXPATHLEN)) {
      fail_continue("[Classpath is invalid", org_name);
      return false;
    }
    // For -Xbootclasspath:a, the org_name doesn't contain jre
    if (!app_check && name[0] == '\0') {
      strncpy(name, org_name, strlen(org_name) + 1);
    }
    if (TraceClassPaths || (TraceClassLoading && Verbose)) {
      tty->print_cr("[Checking shared classpath entry: %s]", UseAppCDS ? org_name : name);
    }

    if (app_check) {
      ok = ent->validate();
      if (!shared_classpath(i)->from_class_path_attr()) {
        shared_app_paths_len++;
      }
    } else if (os::stat(name, &st) != 0) {
      fail_continue("Required classpath entry does not exist: %s", name);
      ok = false;
    } else if (ent->is_dir()) {
      if (!os::dir_is_empty(name)) {
        fail_continue("directory is not empty: %s", name);
        ok = false;
      }
    } else {
      if (ent->_timestamp != st.st_mtime ||
          ent->_filesize != st.st_size) {
        ok = false;
        if (PrintSharedArchiveAndExit) {
          fail_continue(ent->_timestamp != st.st_mtime ?
                        "Timestamp mismatch" :
                        "File size mismatch");
        } else {
          fail_continue("A jar file is not the one used while building"
                        " the shared archive file: %s", name);
        }
      }
    }
    if (ok) {
      if (TraceClassPaths || (TraceClassLoading && Verbose)) {
        tty->print_cr("[ok]");
      }
    } else if (!PrintSharedArchiveAndExit) {
      _validating_classpath_entry_table = false;
      return false;
    }
  }
  if (shared_app_paths_len != 0 && !validate_app_class_paths(shared_app_paths_len)) {
    fail_continue("shared class paths mismatch (hint: enable -XX:+TraceClassPaths to diagnose the failure)");
    _validating_classpath_entry_table = false;
    return false;
  } 
  _classpath_entry_table_size = _header->_classpath_entry_table_size;
  _validating_classpath_entry_table = false;
  return true;
}


// Read the FileMapInfo information from the file.

bool FileMapInfo::init_from_file(int fd) {
  size_t sz = _header->data_size();
  char* addr = _header->data();
  size_t n = os::read(fd, addr, (unsigned int)sz);
  if (n != sz) {
    fail_continue("Unable to read the file header.");
    return false;
  }

  if (_header->_magic != (int)0xf00baba2) {
    FileMapInfo::fail_continue("The shared archive file has a bad magic number.");
    return false;
  }

  if (_header->_version != current_version()) {
    FileMapInfo::fail_continue("The shared archive file is the wrong version.");
    return false;
  }

  char header_version[JVM_IDENT_MAX];
  get_header_version(header_version);
  if (strncmp(_header->_jvm_ident, header_version, JVM_IDENT_MAX-1) != 0) {
    if (TraceClassPaths) {
      tty->print_cr("Expected: %s", header_version);
      tty->print_cr("Actual:   %s", _header->_jvm_ident);
    }
    FileMapInfo::fail_continue("The shared archive file was created by a different"
                  " version or build of HotSpot");
    return false;
  }

  if (VerifySharedSpaces && _header->compute_crc() != _header->_crc) {
    fail_continue("Header checksum verification failed.");
    return false;
  }

  size_t info_size = _header->_paths_misc_info_size;
  _paths_misc_info = NEW_C_HEAP_ARRAY_RETURN_NULL(char, info_size, mtClass);
  if (_paths_misc_info == NULL) {
    fail_continue("Unable to read the file header.");
    return false;
  }
  n = os::read(fd, _paths_misc_info, (unsigned int)info_size);
  if (n != info_size) {
    fail_continue("Unable to read the shared path info header.");
    FREE_C_HEAP_ARRAY(char, _paths_misc_info, mtClass);
    _paths_misc_info = NULL;
    return false;
  }

  size_t len = lseek(fd, 0, SEEK_END);
  struct FileMapInfo::FileMapHeader::space_info* si =
    &_header->_space[MetaspaceShared::mc];
  if (si->_file_offset >= len || len - si->_file_offset < si->_used) {
    fail_continue("The shared archive file has been truncated.");
    return false;
  }

  _file_offset += (long)n;
  return true;
}


// Read the FileMapInfo information from the file.
bool FileMapInfo::open_for_read() {
  _full_path = Arguments::GetSharedArchivePath();
  int fd = open(_full_path, O_RDONLY | O_BINARY, 0);
  if (fd < 0) {
    if (errno == ENOENT) {
      // Not locating the shared archive is ok.
      fail_continue("Specified shared archive not found.");
    } else {
      fail_continue("Failed to open shared archive file (%s).",
                    strerror(errno));
    }
    return false;
  }

  _fd = fd;
  _file_open = true;
  return true;
}


// Write the FileMapInfo information to the file.

void FileMapInfo::open_for_write() {
 _full_path = Arguments::GetSharedArchivePath();
  if (PrintSharedSpaces) {
    tty->print_cr("Dumping shared data to file: ");
    tty->print_cr("   %s", _full_path);
  }

#ifdef _WINDOWS  // On Windows, need WRITE permission to remove the file.
  chmod(_full_path, _S_IREAD | _S_IWRITE);
#endif

  // Use remove() to delete the existing file because, on Unix, this will
  // allow processes that have it open continued access to the file.
  remove(_full_path);
  int fd = open(_full_path, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0444);
  if (fd < 0) {
    fail_stop("Unable to create shared archive file %s.", _full_path);
  }
  _fd = fd;
  _file_offset = 0;
  _file_open = true;
}


// Write the header to the file, seek to the next allocation boundary.

void FileMapInfo::write_header() {
  int info_size = ClassLoader::get_shared_paths_misc_info_size();

  _header->_paths_misc_info_size = info_size;

  align_file_position();
  size_t sz = _header->data_size();
  char* addr = _header->data();
  write_bytes(addr, (int)sz); // skip the C++ vtable
  write_bytes(ClassLoader::get_shared_paths_misc_info(), info_size);
  align_file_position();
}


// Dump shared spaces to file.

void FileMapInfo::write_space(int i, Metaspace* space, bool read_only) {
  align_file_position();
  size_t used = space->used_bytes_slow(Metaspace::NonClassType);
  size_t capacity = space->capacity_bytes_slow(Metaspace::NonClassType);
  struct FileMapInfo::FileMapHeader::space_info* si = &_header->_space[i];
  write_region(i, (char*)space->bottom(), used, capacity, read_only, false);
}


// Dump region to file.

void FileMapInfo::write_region(int region, char* base, size_t size,
                               size_t capacity, bool read_only,
                               bool allow_exec) {
  struct FileMapInfo::FileMapHeader::space_info* si = &_header->_space[region];

  if (_file_open) {
    guarantee(si->_file_offset == _file_offset, "file offset mismatch.");
    if (PrintSharedSpaces) {
      tty->print_cr("Shared file region %d: 0x%6x bytes, addr " INTPTR_FORMAT
                    " file offset 0x%6x", region, size, base, _file_offset);
    }
  } else {
    si->_file_offset = _file_offset;
  }
  si->_base = base;
  si->_used = size;
  si->_capacity = capacity;
  si->_read_only = read_only;
  si->_allow_exec = allow_exec;
  si->_crc = ClassLoader::crc32(0, base, (jint)size);
  write_bytes_aligned(base, (int)size);
}


// Dump bytes to file -- at the current file position.

void FileMapInfo::write_bytes(const void* buffer, int nbytes) {
  if (_file_open) {
    int n = ::write(_fd, buffer, nbytes);
    if (n != nbytes) {
      // It is dangerous to leave the corrupted shared archive file around,
      // close and remove the file. See bug 6372906.
      close();
      remove(_full_path);
      fail_stop("Unable to write to shared archive file.", NULL);
    }
  }
  _file_offset += nbytes;
}


// Align file position to an allocation unit boundary.

void FileMapInfo::align_file_position() {
  size_t new_file_offset = align_size_up(_file_offset,
                                         os::vm_allocation_granularity());
  if (new_file_offset != _file_offset) {
    _file_offset = new_file_offset;
    if (_file_open) {
      // Seek one byte back from the target and write a byte to insure
      // that the written file is the correct length.
      _file_offset -= 1;
      if (lseek(_fd, (long)_file_offset, SEEK_SET) < 0) {
        fail_stop("Unable to seek.", NULL);
      }
      char zero = 0;
      write_bytes(&zero, 1);
    }
  }
}


// Dump bytes to file -- at the current file position.

void FileMapInfo::write_bytes_aligned(const void* buffer, int nbytes) {
  align_file_position();
  write_bytes(buffer, nbytes);
  align_file_position();
}


// Close the shared archive file.  This does NOT unmap mapped regions.

void FileMapInfo::close() {
  if (_file_open) {
    if (::close(_fd) < 0) {
      fail_stop("Unable to close the shared archive file.");
    }
    _file_open = false;
    _fd = -1;
  }
}


// JVM/TI RedefineClasses() support:
// Remap the shared readonly space to shared readwrite, private.
bool FileMapInfo::remap_shared_readonly_as_readwrite() {
  struct FileMapInfo::FileMapHeader::space_info* si = &_header->_space[0];
  if (!si->_read_only) {
    // the space is already readwrite so we are done
    return true;
  }
  size_t used = si->_used;
  size_t size = align_size_up(used, os::vm_allocation_granularity());
  if (!open_for_read()) {
    return false;
  }
  char *base = os::remap_memory(_fd, _full_path, si->_file_offset,
                                si->_base, size, false /* !read_only */,
                                si->_allow_exec);
  close();
  if (base == NULL) {
    fail_continue("Unable to remap shared readonly space (errno=%d).", errno);
    return false;
  }
  if (base != si->_base) {
    fail_continue("Unable to remap shared readonly space at required address.");
    return false;
  }
  si->_read_only = false;
  return true;
}

// Map the whole region at once, assumed to be allocated contiguously.
ReservedSpace FileMapInfo::reserve_shared_memory() {
  struct FileMapInfo::FileMapHeader::space_info* si = &_header->_space[0];
  char* requested_addr = si->_base;

  size_t size = FileMapInfo::shared_spaces_size();

  // Reserve the space first, then map otherwise map will go right over some
  // other reserved memory (like the code cache).
  ReservedSpace rs(size, os::vm_allocation_granularity(), false, requested_addr);
  if (!rs.is_reserved()) {
    fail_continue(err_msg("Unable to reserve shared space at required address " INTPTR_FORMAT, requested_addr));
    return rs;
  }
  // the reserved virtual memory is for mapping class data sharing archive
  MemTracker::record_virtual_memory_type((address)rs.base(), mtClassShared);

  return rs;
}

// Memory map a region in the address space.
static const char* shared_region_name[] = { "ReadOnly", "ReadWrite", "MiscData", "MiscCode"};

char* FileMapInfo::map_region(int i) {
  struct FileMapInfo::FileMapHeader::space_info* si = &_header->_space[i];
  size_t used = si->_used;
  size_t alignment = os::vm_allocation_granularity();
  size_t size = align_size_up(used, alignment);
  char *requested_addr = si->_base;

  bool read_only;

  // If a tool agent is in use (debugging enabled), we must map the address space RW
  if (JvmtiExport::can_modify_any_class() || JvmtiExport::can_walk_any_space()) {
    read_only = false;
  } else {
    read_only = si->_read_only;
  }

  // map the contents of the CDS archive in this memory
  char *base = os::map_memory(_fd, _full_path, si->_file_offset,
                              requested_addr, size, read_only,
                              si->_allow_exec);
  if (base == NULL || base != si->_base) {
    fail_continue(err_msg("Unable to map %s shared space at required address.", shared_region_name[i]));
    return NULL;
  }
#ifdef _WINDOWS
  // This call is Windows-only because the memory_type gets recorded for the other platforms
  // in method FileMapInfo::reserve_shared_memory(), which is not called on Windows.
  MemTracker::record_virtual_memory_type((address)base, mtClassShared);
#endif
  return base;
}

bool FileMapInfo::verify_region_checksum(int i) {
  if (!VerifySharedSpaces) {
    return true;
  }
  const char* buf = _header->_space[i]._base;
  size_t sz = _header->_space[i]._used;
  int crc = ClassLoader::crc32(0, buf, (jint)sz);
  if (crc != _header->_space[i]._crc) {
    fail_continue("Checksum verification failed.");
    return false;
  }
  return true;
}

// Unmap a memory region in the address space.

void FileMapInfo::unmap_region(int i) {
  struct FileMapInfo::FileMapHeader::space_info* si = &_header->_space[i];
  size_t used = si->_used;
  size_t size = align_size_up(used, os::vm_allocation_granularity());
  if (!os::unmap_memory(si->_base, size)) {
    fail_stop("Unable to unmap shared space.");
  }
}


void FileMapInfo::assert_mark(bool check) {
  if (!check) {
    fail_stop("Mark mismatch while restoring from shared file.", NULL);
  }
}


FileMapInfo* FileMapInfo::_current_info = NULL;
SharedClassPathEntry* FileMapInfo::_classpath_entry_table = NULL;
int FileMapInfo::_classpath_entry_table_size = 0;
size_t FileMapInfo::_classpath_entry_size = 0x1234baad;
bool FileMapInfo::_validating_classpath_entry_table = false;

// Open the shared archive file, read and validate the header
// information (version, boot classpath, etc.).  If initialization
// fails, shared spaces are disabled and the file is closed. [See
// fail_continue.]
//
// Validation of the archive is done in two steps:
//
// [1] validate_header() - done here. This checks the header, including _paths_misc_info.
// [2] validate_classpath_entry_table - this is done later, because the table is in the RW
//     region of the archive, which is not mapped yet.
bool FileMapInfo::initialize() {
  assert(UseSharedSpaces, "UseSharedSpaces expected.");

  if (!open_for_read()) {
    return false;
  }

  if (!init_from_file(_fd)) {
    return false;
  }

  if (!validate_header()) {
    return false;
  }

  SharedReadOnlySize =  _header->_space[0]._capacity;
  SharedReadWriteSize = _header->_space[1]._capacity;
  SharedMiscDataSize =  _header->_space[2]._capacity;
  SharedMiscCodeSize =  _header->_space[3]._capacity;
  return true;
}

int FileMapInfo::FileMapHeader::compute_crc() {
  char* header = data();
  // start computing from the field after _crc
  char* buf = (char*)&_crc + sizeof(int);
  size_t sz = data_size() - (buf - header);
  int crc = ClassLoader::crc32(0, buf, (jint)sz);
  return crc;
}

int FileMapInfo::compute_header_crc() {
  return _header->compute_crc();
}

bool FileMapInfo::FileMapHeader::validate() {
  if (_obj_alignment != ObjectAlignmentInBytes) {
    FileMapInfo::fail_continue("The shared archive file's ObjectAlignmentInBytes of %d"
                  " does not equal the current ObjectAlignmentInBytes of %d.",
                  _obj_alignment, ObjectAlignmentInBytes);
    return false;
  }

  
  if (UseAppCDS) {
    // if the archive file is generated without AppCDS, UseAppCDS should be false
    UseAppCDS = _use_appcds;
  }
  // This must be done after header validation because it might change the
  // header data
  const char* prop = Arguments::get_property("java.system.class.loader");
  if (prop != NULL) {
    warning("Archived non-system classes are disabled because the "
            "java.system.class.loader property is specified (value = \"%s\"). "
            "To use archived non-system classes, this property must be not be set", prop);
    _has_ext_or_app_classes = false;
  }
  return true;
}

bool FileMapInfo::validate_header() {
  bool status = _header->validate();

  if (status) {
    if (!ClassLoader::check_shared_paths_misc_info(_paths_misc_info, _header->_paths_misc_info_size)) {
      if (!PrintSharedArchiveAndExit) {
        fail_continue("shared class paths mismatch (hint: enable -XX:+TraceClassPaths to diagnose the failure)");
        status = false;
      }
    }
  }

  if (_paths_misc_info != NULL) {
    FREE_C_HEAP_ARRAY(char, _paths_misc_info, mtClass);
    _paths_misc_info = NULL;
  }
  return status;
}

// The following method is provided to see whether a given pointer
// falls in the mapped shared space.
// Param:
// p, The given pointer
// Return:
// True if the p is within the mapped shared space, otherwise, false.
bool FileMapInfo::is_in_shared_space(const void* p) {
  for (int i = 0; i < MetaspaceShared::n_regions; i++) {
    if (p >= _header->_space[i]._base &&
        p < _header->_space[i]._base + _header->_space[i]._used) {
      return true;
    }
  }

  return false;
}

void FileMapInfo::print_shared_spaces() {
  gclog_or_tty->print_cr("Shared Spaces:");
  for (int i = 0; i < MetaspaceShared::n_regions; i++) {
    struct FileMapInfo::FileMapHeader::space_info* si = &_header->_space[i];
    gclog_or_tty->print("  %s " INTPTR_FORMAT "-" INTPTR_FORMAT,
                        shared_region_name[i],
                        si->_base, si->_base + si->_used);
  }
}

// Unmap mapped regions of shared space.
void FileMapInfo::stop_sharing_and_unmap(const char* msg) {
  FileMapInfo *map_info = FileMapInfo::current_info();
  if (map_info) {
    map_info->fail_continue(msg);
    for (int i = 0; i < MetaspaceShared::n_regions; i++) {
      if (map_info->_header->_space[i]._base != NULL) {
        map_info->unmap_region(i);
        map_info->_header->_space[i]._base = NULL;
      }
    }
  } else if (DumpSharedSpaces) {
    fail_stop(msg, NULL);
  }
}

void FileMapInfo::check_nonempty_dir_in_shared_path_table() {
  assert(DumpSharedSpaces, "dump time only");

  if (!UseAppCDS || RelaxCheckForAppCDS) {
    return;
  }
  bool has_nonempty_dir = false;

  int last = _classpath_entry_table_size - 1;
  if (last > ClassLoaderExt::max_used_path_index()) {
     // no need to check any path beyond max_used_path_index
     last = ClassLoaderExt::max_used_path_index();
  }

  for (int i = 0; i <= last; i++) {
    SharedClassPathEntry *e = shared_classpath(i);
    if (e->is_dir()) {
      const char* path = e->name();
      if (!os::dir_is_empty(path)) {
        tty->print_cr("Error: non-empty directory '%s'", path);
        has_nonempty_dir = true;
      }
    }
  }

  if (has_nonempty_dir) {
    ClassLoader::exit_with_path_failure("Cannot have non-empty directory in paths", NULL);
  }
}

class ClasspathStream : public StackObj {
  const char* _class_path;
  int _len;
  int _start;
  int _end;

public:
  ClasspathStream(const char* class_path) {
    _class_path = class_path;
    _len = (int)strlen(class_path);
    _start = 0;
    _end = 0;
  }

  bool has_next() {
    return _start < _len;
  }

  const char* get_next();
};

const char* ClasspathStream::get_next() {
  while (_class_path[_end] != '\0' && _class_path[_end] != os::path_separator()[0]) {
    _end++;
  }
  int path_len = _end - _start;
  char* path = NEW_RESOURCE_ARRAY(char, path_len + 1);
  strncpy(path, &_class_path[_start], path_len);
  path[path_len] = '\0';

  while (_class_path[_end] == os::path_separator()[0]) {
    _end++;
  }
  _start = _end;
  return path;
}

bool FileMapInfo::same_files(const char* file1, const char* file2) {
  if (strcmp(file1, file2) == 0) {
    return true;
  }

  bool is_same = false;
  struct stat st1;
  struct stat st2;

  if (os::stat(file1, &st1) < 0) {
    return false;
  }

  if (os::stat(file2, &st2) < 0) {
    return false;
  }
#ifndef _WINDOWS
  if (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino) {
    // same files
    is_same = true;
  } else if (RelaxCheckForAppCDS && st1.st_size == st2.st_size) {
    is_same = true;
  }
#endif
  return is_same;
}

int FileMapInfo::num_paths(const char* path) {
  if (path == NULL) {
    return 0;
  }
  int npaths = 1;
  char* p = (char*)path;
  while (p != NULL) {
    char* prev = p;
    p = strstr((char*)p, os::path_separator());
    if (p != NULL) {
      p++;
      // don't count empty path
      if ((p - prev) > 1) {
       npaths++;
      }
    }
  }
  return npaths;
}

GrowableArray<const char*>* FileMapInfo::create_path_array(const char* paths, int* size) {
  GrowableArray<const char*>* path_array = new GrowableArray<const char*>(10);

  ClasspathStream cp_stream(paths);
  int n = 0;
  while (cp_stream.has_next()) {
    const char* path = cp_stream.get_next();
    struct stat st;
    if (os::stat(path, &st) == 0) {
      n++;
      if (!RelaxCheckForAppCDS || ((st.st_mode & S_IFREG) == S_IFREG)) {
        path_array->append(path);
      }
    }
  }
  *size = n;
  return path_array;
}

bool FileMapInfo::classpath_failure(const char* msg, const char* name) {
  ClassLoader::trace_class_path(tty, msg, name);
  if (PrintSharedArchiveAndExit) {
    MetaspaceShared::set_archive_loading_failed();
  }
  return false;
}

bool FileMapInfo::check_paths(int shared_path_start_idx, int num_paths, GrowableArray<const char*>* rp_array) {
  int i = 0;
  int j = shared_path_start_idx;
  bool mismatch = false;
  while (i < num_paths && !mismatch) {
    while (shared_classpath(j)->from_class_path_attr() && j < _header->_classpath_entry_table_size) {
      // shared_path(j) was expanded from the JAR file attribute "Class-Path:"
      // during dump time. It's not included in the -classpath VM argument.
      j++;
    }
    if (RelaxCheckForAppCDS) {
      while (shared_classpath(j)->is_dir() && j < _header->_classpath_entry_table_size) {
        j++;
      }
    }
    if (j >= _header->_classpath_entry_table_size) {
      break;
    }
    if (TraceClassPaths) {
      tty->print_cr("Compare files:");
      ClassLoader::trace_class_path(tty, "Expected: ", shared_classpath(j)->name());
      ClassLoader::trace_class_path(tty, "Actual: ", rp_array->at(i));
    }
    if (!same_files(shared_classpath(j)->name(), rp_array->at(i))) {
      mismatch = true;
    }
    i++;
    j++;
  }
  return mismatch;
}

void FileMapInfo::log_paths(const char* msg, int start_idx, int end_idx) {
  if (TraceClassPaths) {
    tty->print("%s", msg);
    const char* prefix = "";
    for (int i = start_idx; i < end_idx; i++) {
      tty->print("%s%s", prefix, shared_classpath(i)->name());
      prefix = os::path_separator();
    }
    tty->cr();
  }
}

bool FileMapInfo::validate_app_class_paths(int shared_app_paths_len) {
  assert(UseAppCDS, "must be");
  const char *appcp = Arguments::get_appclasspath();
  int rp_len = num_paths(appcp);
  bool mismatch = false;
  if (rp_len < shared_app_paths_len) {
    return classpath_failure("Run time APP classpath is shorter than the one at dump time: ", appcp);
  }
  if (TraceClassPaths) {
    tty->print_cr("Checking app classpath:");
    log_paths("Expected: ", _header->_app_class_paths_start_index, _header->_classpath_entry_table_size);
    ClassLoader::trace_class_path(tty, "Actual: ", appcp);
  }
  if (shared_app_paths_len != 0 && rp_len != 0) {
    // Prefix is OK: E.g., dump with -cp foo.jar, but run with -cp foo.jar:bar.jar.
    ResourceMark rm;
    int array_size = 0;
    GrowableArray<const char*>* rp_array = create_path_array(appcp, &array_size);
    if (array_size == 0) {
      // None of the jar file specified in the runtime -cp exists.
      return classpath_failure("None of the jar file specified in the runtime -cp exists: -Djava.class.path=", appcp);
    }
    if (array_size < shared_app_paths_len) {
      // create_path_array() ignores non-existing paths. Although the dump time and runtime app classpath lengths
      // are the same initially, after the call to create_path_array(), the runtime app classpath length could become
      // shorter. We consider app classpath mismatch in this case.
      return classpath_failure("[APP classpath mismatch, actual: -Djava.class.path=", appcp);
    }

    // Handling of non-existent entries in the classpath: we eliminate all the non-existent
    // entries from both the dump time classpath (ClassLoader::update_class_path_entry_list)
    // and the runtime classpath (FileMapInfo::create_path_array), and check the remaining
    // entries. E.g.:
    //
    // dump : -cp a.jar:NE1:NE2:b.jar  -> a.jar:b.jar -> recorded in archive.
    // run 1: -cp NE3:a.jar:NE4:b.jar  -> a.jar:b.jar -> matched
    // run 2: -cp x.jar:NE4:b.jar      -> x.jar:b.jar -> mismatched

    int j = _header->_app_class_paths_start_index;
    mismatch = check_paths(j, shared_app_paths_len, rp_array);
    if (mismatch) {
      return classpath_failure("[APP classpath mismatch, actual: -Djava.class.path=", appcp);
    }
  }
  return true;
}

