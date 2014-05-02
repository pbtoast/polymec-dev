// Copyright (c) 2012-2014, Jeffrey N. Johnson
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this 
// list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE 
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef POLYMEC_FILE_UTILS_H
#define POLYMEC_FILE_UTILS_H

#include <unistd.h> // for rmdir().

// Given a full pathname, parse it into directory and file portions.
// Memory must be allocated for dirname and for filename that is sufficient 
// to store any portion of path.
void parse_path(const char *path, char *dirname, char *filename);

// Given a path and a filename, join them using the OS-specific separator, 
// storing the result in path.
void join_paths(const char *dirname, const char* filename, char* path);

// Remove a directory and any of its contents. Returns 0 on success, -1 on 
// failure (as does the standard rmdir).
int remove_dir(const char* path);

// Create a temporary file using the given template. This function replaces 
// up to 6 X characters in the filename template with a set of characters that 
// renders it unique (in the spirit of mkstemp), storing the result in filename. 
// The template should have the form path/to/fileXXXXXX, with the X's all at 
// the end. This function returns a file descriptor that is open for writing 
// ("w") on success, or NULL on failure. All temporary files are deleted 
// when a polymec application exits. Note that the temporary file 
// is created within a unique polymec-specific temporary directory.
FILE* make_temp_file(const char* file_template, char* filename);

// Create a temporary directory using the given template. This function replaces 
// up to 6 X characters in the dirname template with a set of characters that 
// renders it unique (in the spirit of mkdtemp). The template should have the 
// form path/to/fileXXXXXX, with the X's all at the end. This function returns 
// true if the directory was created, false if not. All temporary files are 
// deleted when a polymec application exits. Note that the temporary directory 
// is created within a unique polymec-specific temporary directory.
bool make_temp_dir(const char* dir_template, char* dirname);

#endif
