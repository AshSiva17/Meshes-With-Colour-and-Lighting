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
// 0 = Timmy, 1 = bucket, 2 = floor, 3 = bucket on floor.
const int scene = 0;

const char *vertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

out vec2 UV;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    UV = aUV; 
    gl_Position = projection * view * model * vec4(aPosition, 1.0);
}
)glsl";

const char *fragmentShaderSource = R"glsl(
#version 330 core
in vec2 UV;
out vec4 FragColor;

uniform sampler2D myTexture;

void main()
{
    FragColor = texture(myTexture, UV);
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

struct TexturedMesh
{
  unsigned int vao = 0;
  unsigned int vbo = 0;
  unsigned int texture = 0;
  GLsizei vertexCount = 0;
};

bool loadTexturedMesh(const char *objPath, const char *texturePath, TexturedMesh &mesh)
{
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warning;
  std::string error;

  const bool loaded = tinyobj::LoadObj(
      &attrib, &shapes, &materials, &warning, &error, objPath);

  if (!warning.empty())
    std::cerr << warning << '\n';

  if (!loaded)
  {
    std::cerr << "Could not load " << objPath << ": " << error << '\n';
    return false;
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
        return false;
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

  if (vertices.empty())
  {
    std::cerr << "No faces in " << objPath << '\n';
    return false;
  }
  mesh.vertexCount = static_cast<GLsizei>(vertices.size() / floatsPerVertex);
  const GLsizei stride = floatsPerVertex * sizeof(float);

  int width, height, channels;

  stbi_set_flip_vertically_on_load(true);
  unsigned char *pixels = stbi_load(texturePath, &width, &height, &channels, 4);

  if (!pixels)
  {
    std::cerr << "Could not load " << texturePath << '\n';
    return false;
  }

  glGenTextures(1, &mesh.texture);
  glBindTexture(GL_TEXTURE_2D, mesh.texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  stbi_image_free(pixels);

  glGenVertexArrays(1, &mesh.vao);
  glGenBuffers(1, &mesh.vbo);

  glBindVertexArray(mesh.vao);
  glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
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
  return true;
}

void drawMesh(const TexturedMesh &mesh)
{
  glBindTexture(GL_TEXTURE_2D, mesh.texture);
  glBindVertexArray(mesh.vao);
  glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
}

void drawScene(int scene, const TexturedMesh &timmy, const TexturedMesh &bucket,
               const TexturedMesh &floor)
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (scene == 0)
    drawMesh(timmy);
  if (scene == 2 || scene == 3)
    drawMesh(floor);
  if (scene == 1 || scene == 3)
    drawMesh(bucket);
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

  TexturedMesh timmy, bucket, floor;
  const bool loaded = loadTexturedMesh("asset/timmy.obj", "asset/timmy.png", timmy) &&
                      loadTexturedMesh("asset/bucket.obj", "asset/bucket.jpg", bucket) &&
                      loadTexturedMesh("asset/floor.obj", "asset/floor.jpeg", floor);
  int result = loaded ? 0 : 1;

  if (loaded)
  {
    const glm::mat4 model(1.0f);
    const glm::mat4 view = glm::lookAt(glm::vec3(50, 100, 200), glm::vec3(0, 80, 0), glm::vec3(0, 1, 0));
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), 4.0f / 3.0f, 0.1f, 1000.0f);

    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1i(glGetUniformLocation(program, "myTexture"), 0);
    glActiveTexture(GL_TEXTURE0);

    while (!glfwWindowShouldClose(window))
    {
      if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

      drawScene(scene, timmy, bucket, floor);
      glfwSwapBuffers(window);
      glfwPollEvents();
    }
  }

  for (const TexturedMesh &mesh : {timmy, bucket, floor})
  {
    glDeleteVertexArrays(1, &mesh.vao);
    glDeleteBuffers(1, &mesh.vbo);
    glDeleteTextures(1, &mesh.texture);
  }
  glDeleteProgram(program);
  glfwTerminate();
  return result;
}
