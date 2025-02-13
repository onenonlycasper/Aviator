//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef SAMPLE_UTIL_SHADER_UTILS_H
#define SAMPLE_UTIL_SHADER_UTILS_H

#define GL_GLEXT_PROTOTYPES

#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <string>

#define SHADER_SOURCE(...) #__VA_ARGS__

GLuint CompileShader(GLenum type, const std::string &source);
GLuint CompileShaderFromFile(GLenum type, const std::string &sourcePath);

GLuint CompileProgram(const std::string &vsSource, const std::string &fsSource);
GLuint CompileProgramFromFiles(const std::string &vsPath, const std::string &fsPath);

#endif // SAMPLE_UTIL_SHADER_UTILS_H
