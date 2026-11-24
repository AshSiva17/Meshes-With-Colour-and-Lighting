#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <string>
#include <vector>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const int windowWidth = 1024;
const int windowHeight = 768;

const char *vertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPosition, 1.0);
}
)glsl";

const char *fragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(1.0, 0.65, 0.25, 1.0);
}
)glsl";

unsigned int compileShader(GLenum type, const char *source)
{
  unsigned int shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);

  int success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success)
  {
    char log[512];
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    std::cerr << "Shader compilation failed: " << log << '\n';
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

unsigned int createShaderProgram()
{
  unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
  if (!vertexShader)
    return 0;

  unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
  if (!fragmentShader)
  {
    glDeleteShader(vertexShader);
    return 0;
  }

  unsigned int program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  int success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success)
  {
    char log[512];
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    std::cerr << "Shader linking failed: " << log << '\n';
    glDeleteProgram(program);
    return 0;
  }
  return program;
}

void framebufferSizeCallback(GLFWwindow *, int width, int height)
{
  glViewport(0, 0, width, height);
}

int main()
{
  if (!glfwInit())
  {
    std::cerr << "Failed to initialize GLFW\n";
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight, "Assignment 3", nullptr, nullptr);
  if (!window)
  {
    std::cerr << "Failed to create the GLFW window\n";
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
  {
    std::cerr << "Failed to initialize GLAD\n";
    glfwTerminate();
    return 1;
  }

  glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
  int framebufferWidth, framebufferHeight;
  glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
  glViewport(0, 0, framebufferWidth, framebufferHeight);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glClearColor(0.3f, 0.4f, 0.5f, 1.0f);

  unsigned int program = createShaderProgram();
  if (!program)
  {
    glfwTerminate();
    return 1;
  }

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warning;
  std::string error;

  const char *objPath = "asset/timmy.obj";
  const bool loaded = tinyobj::LoadObj(
      &attrib, &shapes, &materials, &warning, &error, objPath);

  if (!warning.empty())
    std::cerr << warning << '\n';

  if (!loaded)
  {
    std::cerr << "Could not load " << objPath << ": " << error << '\n';
    glDeleteProgram(program);
    glfwTerminate();
    return 1;
  }

  const int floatsPerVertex = 8;
  std::vector<float> vertices;

  for (const auto &shape : shapes)
  {
    for (const auto &index : shape.mesh.indices)
    {
      if (index.vertex_index < 0 || index.normal_index < 0 ||
          index.texcoord_index < 0)
      {
        std::cerr << "A face corner is missing a position, normal, or UV.\n";
        glDeleteProgram(program);
        glfwTerminate();
        return 1;
      }

      const int position = 3 * index.vertex_index;
      const int normal = 3 * index.normal_index;
      const int uv = 2 * index.texcoord_index;

      vertices.push_back(attrib.vertices[position]);
      vertices.push_back(attrib.vertices[position + 1]);
      vertices.push_back(attrib.vertices[position + 2]);

      vertices.push_back(attrib.normals[normal]);
      vertices.push_back(attrib.normals[normal + 1]);
      vertices.push_back(attrib.normals[normal + 2]);

      vertices.push_back(attrib.texcoords[uv]);
      vertices.push_back(attrib.texcoords[uv + 1]);
    }
  }

  const GLsizei vertexCount = static_cast<GLsizei>(vertices.size() / floatsPerVertex);
  const GLsizei stride = floatsPerVertex * sizeof(float);

  unsigned int vao, vbo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
               vertices.data(), GL_STATIC_DRAW);

  // Position starts at float 0.
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
  glEnableVertexAttribArray(0);

  // Normal starts at float 3.
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // UV starts at float 6.
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                        (void *)(6 * sizeof(float)));
  glEnableVertexAttribArray(2);

  const glm::mat4 model(1.0f);
  const glm::mat4 view = glm::lookAt(glm::vec3(50, 100, 200),
                                     glm::vec3(0, 80, 0), glm::vec3(0, 1, 0));
  const glm::mat4 projection = glm::perspective(glm::radians(60.0f),
                                                4.0f / 3.0f, 0.1f, 1000.0f);

  glUseProgram(program);
  glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1, GL_FALSE, glm::value_ptr(model));
  glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(view));
  glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

  while (!glfwWindowShouldClose(window))
  {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(window, true);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // TODO Task 2: Update the point-light uniforms before drawing.
    // TODO Task 3: Update the three spotlight directions before drawing.
    // https://learnopengl.com/Lighting/Light-casters
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  glDeleteProgram(program);
  glfwTerminate();
  return 0;
}
