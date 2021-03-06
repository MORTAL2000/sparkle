#ifndef SPARKLE_OPENGL_H_
#define SPARKLE_OPENGL_H_

#include "glfw.h"

/* OpenGL debug macro */
#ifdef NDEBUG
# define CHECKGLERROR()
#else
# define CHECKGLERROR()    CheckGLError(__FILE__, __LINE__, "", true)
#endif

/* Automatically specified by CMAKE. Safeguard in case. */
#ifndef SHADERS_DIR
# define SHADERS_DIR     "../shaders"
#endif

/* Maximum size per shader file (with include). 64 Ko */
#define MAX_SHADER_BUFFERSIZE  (64u*1024u)

// ----------------------------------------------------------------------------

void InitGL();
GLuint CreateRenderProgram(char const* vsfile, const char *gsfile, char const* fsfile, char *src_buffer);
GLuint CreateRenderProgram(char const* vsfile, char const* fsfile, char *src_buffer);
GLuint CreateComputeProgram(char const* program_name, char *src_buffer);
void CheckShaderStatus(GLuint shader, char const* name);
bool CheckProgramStatus(GLuint program, char const* name);
void CheckGLError(char const* file, int const line, char const* errMsg, bool bExitOnFail);
bool IsBufferBound(GLenum pname, GLuint buffer);
GLuint GetAttribLocation(GLuint const pgm, char const *name);
GLuint GetUniformLocation(GLuint const pgm, char const *name);
GLuint GetUInteger(GLenum const pname);

// ----------------------------------------------------------------------------

#endif  // SPARKLE_OPENGL_H_
