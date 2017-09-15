#include "opengl.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ----------------------------------------------------------------------------

static
int checkExtensions(char const** extensions) {
  unsigned int i = 0u;
  int valid = 1;

  for (i=0u; extensions[i] != NULL; ++i) {
    if (!glfwExtensionSupported(extensions[i])) {
      fprintf(stderr, "warning : Extension \"%s\" is not supported.\n", extensions[i]);
      valid = 0;
    }
  }
  return valid;
}

#ifndef USE_GLEW

static
GLFWglproc getAddress(char const* name) {
  GLFWglproc ptr = glfwGetProcAddress(name);

  if (ptr == 0) {
    fprintf(stderr, "error: Extension function %s not found.\n", name);
  }

  return ptr;
}

/* Automatically generated pointers to extension's function */
#include "ext/_extensions.inl"

#endif  // USE_GLEW

// ----------------------------------------------------------------------------

static
int ReadFile(const char* filename, const unsigned int maxsize, char out[]) {
  FILE* fd = 0;
  size_t nelems = 0;
  size_t nreads = 0;

  if (!(fd = fopen(filename, "r"))) {
    fprintf(stderr, "warning: \"%s\" not found.\n", filename);
    return 0;
  }

  memset(out, 0, maxsize);

  fseek(fd, 0, SEEK_END);
  nelems = ftell(fd);
  nelems = (nelems > maxsize) ? maxsize : nelems;
  fseek(fd, 0, SEEK_SET);

  nreads = fread(out, sizeof(char), nelems, fd);

  fclose(fd);

  return nreads == nelems;
}

static
void ReadShaderFile(char const* filename, unsigned int const maxsize, char out[], int *level);

static
void ReadShaderFile(char const* filename, unsigned int const maxsize, char out[]) {
  /// Simple way to deal with include recursivity, without reading guards.
  /// Known limitations : do not handle loop well.

  int max_level = 8;
  ReadShaderFile(filename, maxsize, out, &max_level);
  if (max_level < 0) {
    fprintf(stderr, "Error : too many includes found.\n");
  }
}

static
unsigned int CountChar(char const* str, unsigned int n, char c) {
  unsigned int count = 0u;
  for (unsigned int i=0u; i<n && str[i] != '\0'; ++i) {
    count += (str[i] == c) ? 1u : 0u;
  }
  return count;
}

/* Read the shader and process the #include preprocessors. */
static
void ReadShaderFile(char const* filename, unsigned int const maxsize, char out[], int *level) {
  char const * substr = "#include \"";
  size_t const len = strlen(substr);
  char *first = nullptr;
  char *last = nullptr;
  char include_fn[64u] = {0};
  char include_path[256u] = {0};
  int include_len = 0u;

  /* Prevent long recursive includes */
  if (*level <= 0) {
    return;
  }
  --(*level);

  /* Read the shaders */
  ReadFile(filename, maxsize, out);

  /* Check for include file an retrieve its name */
  last = out;
  //unsigned int newline_count = 0u;
  while (NULL != (first = strstr(last, substr))) {
    //newline_count = CountChar(last, first-last, '\n');

    /* pass commented include directives */
    if ((first != out) && (*(first-1) != '\n')) {
      last = first + 1;
      continue;
    }

    first += len;

    last = strchr(first, '"');
    if (!last) return;

    /* Copy the include file name */
    include_len = (size_t)(last-first);
    strncpy(include_fn, first, include_len);
    include_fn[include_len] = '\0';

    /* Count number of line before the include line */
    unsigned int newline_count = 0u;
    for (char *c = out; c != first; ++c) {
      if (*c == '\n') ++newline_count;
    }

    newline_count += CountChar(last, last-first, '\n');

    /* Set include global path */
    sprintf(include_path, "%s/%s", SHADERS_DIR, include_fn);

    /* Prevent first level recursivity */
    if (strcmp(include_path, filename) == 0) {
      return;
    }

    /* Create memory to hold the include file */
    char *include_file = (char*)calloc(maxsize, sizeof(char));

    /* Retrieve the include file */
    ReadShaderFile(include_path, maxsize, include_file, level);

    /* Add the line directive to the included file */
    sprintf(include_file, "%s\n#line %u", include_file, newline_count); // [incorrect]

    /* Add the second part of the shader */
    last = strchr(last, '\n');
    sprintf(include_file, "%s\n%s", include_file, last);

    /* Copy it back to the shader buffer */
    sprintf(first-len, "%s", include_file);

    /* Free include file data */
    free(include_file);
  }
}

// ----------------------------------------------------------------------------

extern
void InitGL() {
  char const* s_extensions[] = {
    "GL_ARB_compute_shader",
    "GL_ARB_separate_shader_objects",
    "GL_ARB_shader_image_load_store",
    "GL_ARB_shader_storage_buffer_object",
    NULL
  };

  /* Check if specific extensions exists */
  checkExtensions(s_extensions);

#ifdef USE_GLEW
  /* Load GLEW */
  glewExperimental = GL_TRUE;
  GLenum result = glewInit();

  /* flush doubtful error */
  glGetError();

  if (GLEW_OK != result) {
    fprintf(stderr, "Error: %s\n", glewGetErrorString(result));
  }
#else
  /* Load function pointer */
  LoadExtensionFuncPtrs();
#endif
}

extern
GLuint CreateRenderProgram(char const* vsfile, char const* gsfile, char const* fsfile, char *src_buffer) {
  GLuint pgm = 0u;
  GLuint vshader = 0u;
  GLuint gshader = 0u;
  GLuint fshader = 0u;

  assert(vsfile && fsfile);
  assert(src_buffer);

  /* Vertex Shader */
  vshader = glCreateShader(GL_VERTEX_SHADER);
  ReadShaderFile(vsfile, MAX_SHADER_BUFFERSIZE, src_buffer);
  glShaderSource(vshader, 1, &src_buffer, NULL);
  glCompileShader(vshader);
  CheckShaderStatus(vshader, vsfile);

  /* Geometry Shader */
  if (gsfile) {
    gshader = glCreateShader(GL_GEOMETRY_SHADER);
    ReadShaderFile(gsfile, MAX_SHADER_BUFFERSIZE, src_buffer);
    glShaderSource(gshader, 1, &src_buffer, NULL);
    glCompileShader(gshader);
    CheckShaderStatus(gshader, gsfile);
  }

  /* Fragment Shader */
  fshader = glCreateShader(GL_FRAGMENT_SHADER);
  ReadShaderFile(fsfile, MAX_SHADER_BUFFERSIZE, src_buffer);
  glShaderSource(fshader, 1, &src_buffer, NULL);
  glCompileShader(fshader);
  CheckShaderStatus(fshader, fsfile);

  pgm = glCreateProgram();
  glAttachShader(pgm, vshader); glDeleteShader(vshader);
  glAttachShader(pgm, fshader); glDeleteShader(fshader);
  if (gsfile) { glAttachShader(pgm, gshader); glDeleteShader(gshader); }
  glLinkProgram(pgm);
  CheckProgramStatus(pgm, fsfile);

  return pgm;
}

extern
GLuint CreateRenderProgram(char const* vsfile, char const* fsfile, char *src_buffer) {
  return CreateRenderProgram(vsfile, nullptr, fsfile, src_buffer);
}

extern
GLuint CreateComputeProgram(char const* program_name, char *src_buffer) {
  GLuint pgm = 0u;

  ReadShaderFile(program_name, MAX_SHADER_BUFFERSIZE, src_buffer);

  pgm = glCreateShaderProgramv(GL_COMPUTE_SHADER, 1, &src_buffer);
  if (!CheckProgramStatus(pgm, program_name)) {
    exit(EXIT_FAILURE);
  }

  return pgm;
}

void CheckShaderStatus(GLuint shader, char const* name) {
  GLint status = 0;

  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    char buffer[1024];
    glGetShaderInfoLog(shader, 1024, 0, buffer);
    fprintf(stderr, "%s :\n%s\n", name, buffer);
  }
}

bool CheckProgramStatus(GLuint program, char const* name) {
  GLint status = 0;

  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    char buffer[1024];
    glGetProgramInfoLog(program, 1024, 0, buffer);
    fprintf(stderr, "%s\n", buffer);
  }

  glValidateProgram(program);
  glGetProgramiv(program, GL_VALIDATE_STATUS, &status);
  if (status != GL_TRUE) {
    fprintf(stderr, "Program \"%s\" failed to be validated.\n", name);
    return false;
  }

  return true;
}

static
const char* GetErrorString(GLenum err) {
#define STRINGIFY(x) #x
  switch (err)
  {
    // [GetError]
    case GL_NO_ERROR:
      return STRINGIFY(GL_NO_ERROR);
      
    case GL_INVALID_ENUM:
      return STRINGIFY(GL_INVALID_ENUM);
      
    case GL_INVALID_VALUE:
      return STRINGIFY(GL_INVALID_VALUE);
      
    case GL_INVALID_OPERATION:
      return STRINGIFY(GL_INVALID_OPERATION);
      
    case GL_STACK_OVERFLOW:
      return STRINGIFY(GL_STACK_OVERFLOW);
      
    case GL_STACK_UNDERFLOW:
      return STRINGIFY(GL_STACK_UNDERFLOW);
      
    case GL_OUT_OF_MEMORY:
      return STRINGIFY(GL_OUT_OF_MEMORY);
      
      
    default:
      return "GetErrorString : Unknown constant";
  }
#undef STRINGIFY
    
  return "";
}

void CheckGLError(const char* file, const int line, const char* errMsg, bool bExitOnFail) {
  GLenum err = glGetError();

  if (err != GL_NO_ERROR) {
    fprintf(stderr,
            "OpenGL error @ \"%s\" [%d] : %s [%s].\n",
            file, line, errMsg, GetErrorString(err));

    if (bExitOnFail) {
      exit(EXIT_FAILURE);
    }
  }
}

extern
bool IsBufferBound(GLenum pname, GLuint buffer) {
  GLint data;
  glGetIntegerv(pname, &data);
  return ((GLuint)data) == buffer;
}

extern
GLint GetUniformLocation(GLuint const pgm, char const *name) {
  GLint loc = glGetUniformLocation(pgm, name);
#ifndef NDEBUG
  if (loc == -1) {
    fprintf(stderr, "Warning : uniform %s was not found.\n", name);
  }
#endif
  return loc;
}

// ----------------------------------------------------------------------------
