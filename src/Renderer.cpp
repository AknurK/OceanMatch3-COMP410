#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_GLCOREARB
#include "Renderer.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Fallback colors (used only when texture fails to load)
static const float TILE_COLORS[4][3] = {
    {0.20f, 0.60f, 1.00f}, // BLUE
    {0.20f, 0.85f, 0.30f}, // GREEN
    {1.00f, 0.55f, 0.10f}, // ORANGE
    {1.00f, 0.30f, 0.70f}, // PINK
};
static float clamp01(float v) {
  return std::max(0.0f, std::min(1.0f, v));
}

// ─────────────────────────────────────────────────────────────────────────────
// Texture loading (shared between 2D and 3D pipelines)
// ─────────────────────────────────────────────────────────────────────────────

unsigned int Renderer::loadTexture(const char *path) {
  unsigned int textureID;
  glGenTextures(1, &textureID);

  int width, height, nrComponents;
  stbi_set_flip_vertically_on_load(true);

  // First pass: detect original channel count
  {
    int w, h, n;
    unsigned char *probe = stbi_load(path, &w, &h, &n, 0);
    nrComponents = n;
    stbi_image_free(probe);
  }

  // Always load as RGBA
  unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 4);
  if (data) {
    // Only do CPU background removal for RGB images (no original alpha).
    // For RGBA images the user already provided correct transparent alpha —
    // trust it.
    if (nrComponents == 3) {
      for (int i = 0; i < width * height; i++) {
        int r = data[i * 4 + 0], g = data[i * 4 + 1], b = data[i * 4 + 2];
        int mx = std::max({r, g, b}), mn = std::min({r, g, b});
        int sat = mx - mn;
        bool isWhiteBg = (mx > 230 && sat < 20);
        bool isBlackBg = (mx < 15);
        data[i * 4 + 3] = (isWhiteBg || isBlackBg) ? 0 : 255;
      }
    }
    // For RGBA (nrComponents == 4): original alpha is intact — no modification
    // needed

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
  } else {
    std::cerr << "Failed to load texture: " << path << std::endl;
    stbi_image_free(data);
    return 0;
  }
  return textureID;
}

// ─────────────────────────────────────────────────────────────────────────────
// 2D orthographic projection (for obstacles & empty cells)
// ─────────────────────────────────────────────────────────────────────────────

void Renderer::buildOrtho(float w, float h) {
  memset(proj, 0, sizeof(proj));
  proj[0] = 2.0f / w;
  proj[5] = -2.0f / h;
  proj[10] = -1.0f;
  proj[12] = -1.0f;
  proj[13] = 1.0f;
  proj[15] = 1.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// 3D perspective projection
// ─────────────────────────────────────────────────────────────────────────────

void Renderer::buildPerspective(float fovDeg, float aspect, float near,
                                float far) {
  memset(projMatrix3D, 0, sizeof(projMatrix3D));
  float fovRad = fovDeg * 3.14159265f / 180.0f;
  float tanHalf = tanf(fovRad / 2.0f);
  projMatrix3D[0] = 1.0f / (aspect * tanHalf);
  projMatrix3D[5] = 1.0f / tanHalf;
  projMatrix3D[10] = -(far + near) / (far - near);
  projMatrix3D[11] = -1.0f;
  projMatrix3D[14] = -(2.0f * far * near) / (far - near);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3D view matrix — look-at implementation
// ─────────────────────────────────────────────────────────────────────────────

static void crossVec3(const float a[3], const float b[3], float out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

static void normalizeVec3(float v[3]) {
  float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (len > 0.0001f) {
    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
  }
}

void Renderer::buildViewMatrix() {
  // Board spans cols 0..7 and rows 0..7 in 3D space (x = col, z = row)
  // Center of the board
  float centerX = 3.5f;
  float centerY = 0.0f;
  float centerZ = 3.5f;

  // Camera above and slightly in front, looking down at the board
  // Slight tilt (~15°) for 3D perception while keeping match-3 readability
  float eyeX = centerX;
  float eyeY = 17.5f;
  float eyeZ = centerZ + 6.3f;

  float fwd[3] = {centerX - eyeX, centerY - eyeY, centerZ - eyeZ};
  normalizeVec3(fwd);

  float worldUp[3] = {0.0f, 1.0f, 0.0f};
  float right[3];
  crossVec3(fwd, worldUp, right);
  normalizeVec3(right);

  float up[3];
  crossVec3(right, fwd, up);
  normalizeVec3(up);

  // Column-major layout for OpenGL
  memset(viewMatrix, 0, sizeof(viewMatrix));
  viewMatrix[0] = right[0];
  viewMatrix[1] = up[0];
  viewMatrix[2] = -fwd[0];

  viewMatrix[4] = right[1];
  viewMatrix[5] = up[1];
  viewMatrix[6] = -fwd[1];

  viewMatrix[8] = right[2];
  viewMatrix[9] = up[2];
  viewMatrix[10] = -fwd[2];

  viewMatrix[12] = -(right[0] * eyeX + right[1] * eyeY + right[2] * eyeZ);
  viewMatrix[13] = -(up[0] * eyeX + up[1] * eyeY + up[2] * eyeZ);
  viewMatrix[14] = (fwd[0] * eyeX + fwd[1] * eyeY + fwd[2] * eyeZ);
  viewMatrix[15] = 1.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// 3D cube mesh — single reusable cube centered at origin, half-size 0.35
// ─────────────────────────────────────────────────────────────────────────────

void Renderer::initCubeMesh() {
  const float H =
      0.35f; // half-size — diagonal ~0.495, fits within 0.5 cell half-spacing

  // 24 vertices: 4 per face × 6 faces
  // Each vertex: pos(3) + uv(2) + normal(3) = 8 floats
  float verts[] = {
      // Front face (z = +H, normal = 0,0,1)
      -H,
      -H,
      H,
      0.0f,
      0.0f,
      0.0f,
      0.0f,
      1.0f,
      H,
      -H,
      H,
      1.0f,
      0.0f,
      0.0f,
      0.0f,
      1.0f,
      H,
      H,
      H,
      1.0f,
      1.0f,
      0.0f,
      0.0f,
      1.0f,
      -H,
      H,
      H,
      0.0f,
      1.0f,
      0.0f,
      0.0f,
      1.0f,

      // Back face (z = -H, normal = 0,0,-1)
      H,
      -H,
      -H,
      0.0f,
      0.0f,
      0.0f,
      0.0f,
      -1.0f,
      -H,
      -H,
      -H,
      1.0f,
      0.0f,
      0.0f,
      0.0f,
      -1.0f,
      -H,
      H,
      -H,
      1.0f,
      1.0f,
      0.0f,
      0.0f,
      -1.0f,
      H,
      H,
      -H,
      0.0f,
      1.0f,
      0.0f,
      0.0f,
      -1.0f,

      // Top face (y = +H, normal = 0,1,0)
      -H,
      H,
      H,
      0.0f,
      0.0f,
      0.0f,
      1.0f,
      0.0f,
      H,
      H,
      H,
      1.0f,
      0.0f,
      0.0f,
      1.0f,
      0.0f,
      H,
      H,
      -H,
      1.0f,
      1.0f,
      0.0f,
      1.0f,
      0.0f,
      -H,
      H,
      -H,
      0.0f,
      1.0f,
      0.0f,
      1.0f,
      0.0f,

      // Bottom face (y = -H, normal = 0,-1,0)
      -H,
      -H,
      -H,
      0.0f,
      0.0f,
      0.0f,
      -1.0f,
      0.0f,
      H,
      -H,
      -H,
      1.0f,
      0.0f,
      0.0f,
      -1.0f,
      0.0f,
      H,
      -H,
      H,
      1.0f,
      1.0f,
      0.0f,
      -1.0f,
      0.0f,
      -H,
      -H,
      H,
      0.0f,
      1.0f,
      0.0f,
      -1.0f,
      0.0f,

      // Right face (x = +H, normal = 1,0,0)
      H,
      -H,
      H,
      0.0f,
      0.0f,
      1.0f,
      0.0f,
      0.0f,
      H,
      -H,
      -H,
      1.0f,
      0.0f,
      1.0f,
      0.0f,
      0.0f,
      H,
      H,
      -H,
      1.0f,
      1.0f,
      1.0f,
      0.0f,
      0.0f,
      H,
      H,
      H,
      0.0f,
      1.0f,
      1.0f,
      0.0f,
      0.0f,

      // Left face (x = -H, normal = -1,0,0)
      -H,
      -H,
      -H,
      0.0f,
      0.0f,
      -1.0f,
      0.0f,
      0.0f,
      -H,
      -H,
      H,
      1.0f,
      0.0f,
      -1.0f,
      0.0f,
      0.0f,
      -H,
      H,
      H,
      1.0f,
      1.0f,
      -1.0f,
      0.0f,
      0.0f,
      -H,
      H,
      -H,
      0.0f,
      1.0f,
      -1.0f,
      0.0f,
      0.0f,
  };

  unsigned int inds[] = {
      0,  1,  2,  0,  2,  3,  // front
      4,  5,  6,  4,  6,  7,  // back
      8,  9,  10, 8,  10, 11, // top
      12, 13, 14, 12, 14, 15, // bottom
      16, 17, 18, 16, 18, 19, // right
      20, 21, 22, 20, 22, 23, // left
  };

  glGenVertexArrays(1, &cubeVAO);
  glGenBuffers(1, &cubeVBO);
  glGenBuffers(1, &cubeEBO);

  glBindVertexArray(cubeVAO);
  glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(inds), inds, GL_STATIC_DRAW);

  // position (location 0)
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
  // texcoord (location 1)
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  // normal (location 2)
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(5 * sizeof(float)));

  glBindVertexArray(0);
}
// pufferfish mesh
void Renderer::initPufferMesh() {
  const int latSegments = 6;
  const int lonSegments = 8;
  const float radius = 0.36f;

  std::vector<float> verts;
  std::vector<unsigned int> inds;

  // Vertex format: pos(3) + uv(2 dummy) + normal(3)
  for (int lat = 0; lat <= latSegments; lat++) {
    float v = (float)lat / (float)latSegments;
    float theta = v * 3.14159265f; // 0..pi

    for (int lon = 0; lon <= lonSegments; lon++) {
      float u = (float)lon / (float)lonSegments;
      float phi = u * 2.0f * 3.14159265f; // 0..2pi

      float x = sinf(theta) * cosf(phi);
      float y = cosf(theta);
      float z = sinf(theta) * sinf(phi);

      // Slightly squash vertically and puff a bit in depth so it reads less
      // like a perfect ball
      float px = radius * x;
      float py = radius * 0.92f * y;
      float pz = radius * 1.05f * z;

      // Normal from unsquashed sphere direction is good enough visually
      float nx = x;
      float ny = y;
      float nz = z;

      verts.push_back(px);
      verts.push_back(py);
      verts.push_back(pz);

      // dummy UVs (not used)
      verts.push_back(u);
      verts.push_back(v);

      verts.push_back(nx);
      verts.push_back(ny);
      verts.push_back(nz);
    }
  }

  for (int lat = 0; lat < latSegments; lat++) {
    for (int lon = 0; lon < lonSegments; lon++) {
      int row1 = lat * (lonSegments + 1);
      int row2 = (lat + 1) * (lonSegments + 1);

      unsigned int a = row1 + lon;
      unsigned int b = row1 + lon + 1;
      unsigned int c = row2 + lon;
      unsigned int d = row2 + lon + 1;

      inds.push_back(a);
      inds.push_back(c);
      inds.push_back(b);

      inds.push_back(b);
      inds.push_back(c);
      inds.push_back(d);
    }
  }

  pufferIndexCount = (int)inds.size();

  glGenVertexArrays(1, &pufferVAO);
  glGenBuffers(1, &pufferVBO);
  glGenBuffers(1, &pufferEBO);

  glBindVertexArray(pufferVAO);

  glBindBuffer(GL_ARRAY_BUFFER, pufferVBO);
  glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(),
               GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pufferEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int),
               inds.data(), GL_STATIC_DRAW);

  // position
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);

  // uv
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(3 * sizeof(float)));

  // normal
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(5 * sizeof(float)));

  glBindVertexArray(0);
}
// pufferfish spike mesh
void Renderer::initSpikeMesh() {
  const int segments = 6;
  const float baseRadius = 0.055f;
  const float height = 0.18f;

  std::vector<float> verts;
  std::vector<unsigned int> inds;

  // Vertex 0 = tip, pointing +Y
  verts.insert(verts.end(), {0.0f, height, 0.0f, 0.5f, 1.0f, 0.0f, 1.0f, 0.0f});

  // Base ring
  for (int i = 0; i < segments; i++) {
    float a = (2.0f * 3.14159265f * i) / (float)segments;
    float x = cosf(a) * baseRadius;
    float z = sinf(a) * baseRadius;

    // rough normal
    float nx = cosf(a);
    float ny = 0.4f;
    float nz = sinf(a);
    float len = sqrtf(nx * nx + ny * ny + nz * nz);
    nx /= len;
    ny /= len;
    nz /= len;

    verts.push_back(x);
    verts.push_back(0.0f);
    verts.push_back(z);
    verts.push_back(0.0f);
    verts.push_back(0.0f);
    verts.push_back(nx);
    verts.push_back(ny);
    verts.push_back(nz);
  }

  // Base center
  int baseCenterIndex = 1 + segments;
  verts.insert(verts.end(), {0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f, -1.0f, 0.0f});

  // Side triangles
  for (int i = 0; i < segments; i++) {
    int next = (i + 1) % segments;
    inds.push_back(0);
    inds.push_back(1 + i);
    inds.push_back(1 + next);
  }

  // Base triangles
  for (int i = 0; i < segments; i++) {
    int next = (i + 1) % segments;
    inds.push_back(baseCenterIndex);
    inds.push_back(1 + next);
    inds.push_back(1 + i);
  }

  spikeIndexCount = (int)inds.size();

  glGenVertexArrays(1, &spikeVAO);
  glGenBuffers(1, &spikeVBO);
  glGenBuffers(1, &spikeEBO);

  glBindVertexArray(spikeVAO);

  glBindBuffer(GL_ARRAY_BUFFER, spikeVBO);
  glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(),
               GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, spikeEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int),
               inds.data(), GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);

  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(3 * sizeof(float)));

  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(5 * sizeof(float)));

  glBindVertexArray(0);
}

void Renderer::initPufferFinMesh() {
  // Double-sided diamond fin in local XY plane. The model matrix orients it.
  float verts[] = {
      // pos                 // uv       // normal
       0.00f,  0.30f, 0.00f, 0.5f, 1.0f,  0.0f, 0.0f,  1.0f,
       0.34f,  0.00f, 0.00f, 1.0f, 0.5f,  0.0f, 0.0f,  1.0f,
       0.00f, -0.30f, 0.00f, 0.5f, 0.0f,  0.0f, 0.0f,  1.0f,
      -0.34f,  0.00f, 0.00f, 0.0f, 0.5f,  0.0f, 0.0f,  1.0f,

       0.00f,  0.30f, 0.00f, 0.5f, 1.0f,  0.0f, 0.0f, -1.0f,
      -0.34f,  0.00f, 0.00f, 0.0f, 0.5f,  0.0f, 0.0f, -1.0f,
       0.00f, -0.30f, 0.00f, 0.5f, 0.0f,  0.0f, 0.0f, -1.0f,
       0.34f,  0.00f, 0.00f, 1.0f, 0.5f,  0.0f, 0.0f, -1.0f,
  };

  unsigned int inds[] = {
      0, 1, 2, 0, 2, 3,
      4, 5, 6, 4, 6, 7,
  };

  pufferFinIndexCount = 12;

  glGenVertexArrays(1, &pufferFinVAO);
  glGenBuffers(1, &pufferFinVBO);
  glGenBuffers(1, &pufferFinEBO);

  glBindVertexArray(pufferFinVAO);
  glBindBuffer(GL_ARRAY_BUFFER, pufferFinVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pufferFinEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(inds), inds, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);

  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(3 * sizeof(float)));

  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(5 * sizeof(float)));

  glBindVertexArray(0);
}

void Renderer::initPiranhaMesh() {
  std::vector<float> verts;
  std::vector<unsigned int> inds;

  auto addVertex = [&](float x, float y, float z, float u, float v, float nx,
                       float ny, float nz) {
    verts.push_back(x);
    verts.push_back(y);
    verts.push_back(z);
    verts.push_back(u);
    verts.push_back(v);
    verts.push_back(nx);
    verts.push_back(ny);
    verts.push_back(nz);
  };

  auto addTri = [&](unsigned int a, unsigned int b, unsigned int c) {
    inds.push_back(a);
    inds.push_back(b);
    inds.push_back(c);
  };

  struct PiranhaPoint {
    float x;
    float y;
    float z;
  };

  auto appendTri = [&](PiranhaPoint a, PiranhaPoint b, PiranhaPoint c) {
    float ux = b.x - a.x;
    float uy = b.y - a.y;
    float uz = b.z - a.z;
    float vx = c.x - a.x;
    float vy = c.y - a.y;
    float vz = c.z - a.z;

    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    float len = sqrtf(nx * nx + ny * ny + nz * nz);
    if (len < 1e-5f) {
      nx = 0.0f;
      ny = 1.0f;
      nz = 0.0f;
    } else {
      nx /= len;
      ny /= len;
      nz /= len;
    }

    unsigned int base = (unsigned int)(verts.size() / 8);
    addVertex(a.x, a.y, a.z, 0.0f, 0.0f, nx, ny, nz);
    addVertex(b.x, b.y, b.z, 1.0f, 0.0f, nx, ny, nz);
    addVertex(c.x, c.y, c.z, 0.5f, 1.0f, nx, ny, nz);
    addTri(base, base + 1, base + 2);
  };

  auto appendQuad = [&](PiranhaPoint a, PiranhaPoint b, PiranhaPoint c,
                        PiranhaPoint d) {
    appendTri(a, b, c);
    appendTri(a, c, d);
  };

  auto appendDoubleSidedTri = [&](PiranhaPoint a, PiranhaPoint b,
                                  PiranhaPoint c) {
    appendTri(a, b, c);
    appendTri(c, b, a);
  };

  const float xHalf = 0.16f;
  const float yHalf = 0.22f;
  const float zBack = -0.30f;
  const float zFront = 0.14f;
  const PiranhaPoint bodyFront[4] = {{-xHalf, -yHalf, zFront},
                                     {xHalf, -yHalf, zFront},
                                     {xHalf, yHalf, zFront},
                                     {-xHalf, yHalf, zFront}};
  const PiranhaPoint bodyBack[4] = {{-xHalf, -yHalf, zBack},
                                    {xHalf, -yHalf, zBack},
                                    {xHalf, yHalf, zBack},
                                    {-xHalf, yHalf, zBack}};

  // Longer rectangular prism body.
  appendQuad(bodyFront[0], bodyFront[1], bodyFront[2], bodyFront[3]);
  appendQuad(bodyBack[1], bodyBack[0], bodyBack[3], bodyBack[2]);
  appendQuad(bodyFront[3], bodyFront[2], bodyBack[2], bodyBack[3]);
  appendQuad(bodyFront[0], bodyBack[0], bodyBack[1], bodyFront[1]);
  appendQuad(bodyFront[1], bodyBack[1], bodyBack[2], bodyFront[2]);
  appendQuad(bodyFront[0], bodyFront[3], bodyBack[3], bodyBack[0]);

  // Four-sided pyramid head attached to the front of the body.
  const PiranhaPoint headTip = {0.0f, 0.0f, 0.36f};
  appendTri(bodyFront[3], bodyFront[2], headTip); // top
  appendTri(bodyFront[1], bodyFront[0], headTip); // bottom
  appendTri(bodyFront[2], bodyFront[1], headTip); // right
  appendTri(bodyFront[0], bodyFront[3], headTip); // left

  // Vertical triangular wings that stick outward from the body sides.
  appendDoubleSidedTri({xHalf, 0.14f, -0.02f}, {xHalf, -0.14f, -0.02f},
                       {0.38f, 0.0f, -0.02f});
  appendDoubleSidedTri({-xHalf, -0.14f, -0.02f}, {-xHalf, 0.14f, -0.02f},
                       {-0.38f, 0.0f, -0.02f});

  // Single rear triangle tail.
  appendDoubleSidedTri({0.0f, 0.21f, zBack}, {0.0f, -0.21f, zBack},
                       {0.0f, 0.0f, zBack - 0.22f});

  piranhaIndexCount = (int)inds.size();

  glGenVertexArrays(1, &piranhaVAO);
  glGenBuffers(1, &piranhaVBO);
  glGenBuffers(1, &piranhaEBO);

  glBindVertexArray(piranhaVAO);

  glBindBuffer(GL_ARRAY_BUFFER, piranhaVBO);
  glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(),
               GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, piranhaEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int),
               inds.data(), GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);

  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(3 * sizeof(float)));

  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(5 * sizeof(float)));

  glBindVertexArray(0);
}

void Renderer::initEffectQuad() {
  float verts[] = {
      // positions            // uv
      -0.5f, 0.0f, -0.5f,     0.0f, 0.0f,
       0.5f, 0.0f, -0.5f,     1.0f, 0.0f,
       0.5f, 0.0f,  0.5f,     1.0f, 1.0f,
      -0.5f, 0.0f,  0.5f,     0.0f, 1.0f
  };

  unsigned int inds[] = {
      0, 1, 2,
      2, 3, 0
  };

  glGenVertexArrays(1, &effectVAO);
  glGenBuffers(1, &effectVBO);
  glGenBuffers(1, &effectEBO);

  glBindVertexArray(effectVAO);

  glBindBuffer(GL_ARRAY_BUFFER, effectVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, effectEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(inds), inds, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        (void *)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Init — sets up both 2D and 3D pipelines
// ─────────────────────────────────────────────────────────────────────────────

void Renderer::init(int windowW, int windowH, Shader *shader2D,
                    Shader *shader3D) {
  sh = shader2D;
  cubeSh = shader3D;
  winW = windowW;
  winH = windowH;
  buildOrtho((float)winW, (float)winH);

  // ── 2D quad mesh ──────────────────────────────────────────────────────
  // Unit quad (0,0)..(1,1), UV flipped vertically to match stbi_set_flip
  float verts[] = {
      0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
  };
  unsigned int inds[] = {0, 1, 2, 0, 2, 3};

  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(inds), inds, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(2 * sizeof(float)));
  glBindVertexArray(0);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // ── Load textures ─────────────────────────────────────────────────────
  tileTextures[0] = loadTexture("assets/textures/blue.png");
  tileTextures[1] = loadTexture("assets/textures/green.png");
  tileTextures[2] = loadTexture("assets/textures/orange.png");
  tileTextures[3] = loadTexture("assets/textures/pink.png");

  // Load obstacle textures (new single-file textures)
  iceObstacleTex = loadTexture("assets/textures/ice_obstacle.png");
  woodFullTex = loadTexture("assets/textures/wood_full.png");
  woodDamagedTex = loadTexture("assets/textures/wood_damaged.png");

  // Load special tile textures
  piranhaTex = loadTexture("assets/textures/piranha.png");
  pufferfishTex = loadTexture("assets/textures/puffer_fish.png");

  // Bind sampler to texture unit 0 for 2D shader
  sh->use();
  sh->setInt("tex0", 0);

  // ── 3D cube mesh & camera ─────────────────────────────────────────────
  effectShader = Shader("shaders/effect.vert", "shaders/effect.frag");

  initCubeMesh();
  initPufferMesh();
  initSpikeMesh();
  initPufferFinMesh();
  initPiranhaMesh();
  initEffectQuad();

  float aspect = (float)winW / (float)winH;
  buildPerspective(30.0f, aspect, 0.1f, 100.0f);
  buildViewMatrix();

  // Bind sampler to texture unit 0 for 3D shader
  cubeSh->use();
  cubeSh->setInt("tex0", 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2D drawing helpers (unchanged from original)
// ─────────────────────────────────────────────────────────────────────────────

void Renderer::drawQuad(float x, float y, float size, float r, float g, float b,
                        float a) {
  float model[16] = {size, 0.0f, 0.0f, 0.0f, 0.0f, size, 0.0f, 0.0f,
                     0.0f, 0.0f, 1.0f, 0.0f, x,    y,    0.0f, 1.0f};
  sh->use();
  sh->setMat4("projection", proj);
  sh->setMat4("model", model);
  sh->setVec4("tileColor", r, g, b, a);
  sh->setFloat("useTexture", 0.0f);
  glBindVertexArray(vao);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void Renderer::drawTexturedQuad(unsigned int tex, float x, float y, float size,
                                float r, float g, float b, float a) {
  float model[16] = {size, 0.0f, 0.0f, 0.0f, 0.0f, size, 0.0f, 0.0f,
                     0.0f, 0.0f, 1.0f, 0.0f, x,    y,    0.0f, 1.0f};
  sh->use();
  sh->setMat4("projection", proj);
  sh->setMat4("model", model);
  sh->setVec4("tileColor", r, g, b, a);
  sh->setFloat("useTexture", 1.0f);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);

  glBindVertexArray(vao);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3D cube drawing
// ─────────────────────────────────────────────────────────────────────────────

void Renderer::setRotationAngle(float angle) { rotAngle = angle; }

void Renderer::drawCube(unsigned int tex, float worldX, float worldZ, float r,
                        float g, float b, float a) {
  // Build model matrix: translate to (worldX, 0, worldZ) then rotate around Y
  float cosA = cosf(rotAngle);
  float sinA = sinf(rotAngle);

  // Column-major: M = T * Ry
  // Ry = | cosA  0  sinA |    T = translate(worldX, 0, worldZ)
  //      |  0    1   0   |
  //      |-sinA  0  cosA |
  float model[16] = {cosA, 0.0f, -sinA, 0.0f, 0.0f,   1.0f, 0.0f,   0.0f,
                     sinA, 0.0f, cosA,  0.0f, worldX, 0.0f, worldZ, 1.0f};

  cubeSh->use();
  cubeSh->setMat4("projection", projMatrix3D);
  cubeSh->setMat4("view", viewMatrix);
  cubeSh->setMat4("model", model);
  cubeSh->setVec4("tileColor", r, g, b, a);

  if (tex != 0) {
    cubeSh->setFloat("useTexture", 1.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
  } else {
    cubeSh->setFloat("useTexture", 0.0f);
  }

  glBindVertexArray(cubeVAO);
  glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}
// pufferfish drawing
void Renderer::drawPufferfish(float worldX, float worldZ, float brightness) {
  float cosA = 1.0f;
  float sinA = 0.0f;

  auto setPufferYaw = [&](float yaw) {
    cosA = cosf(yaw);
    sinA = sinf(yaw);
  };

  // Rotate the pufferfish with the same tile spin used by the other pieces.
  setPufferYaw(rotAngle);

  auto rotateOffset = [&](float lx, float lz, float &rx, float &rz) {
    rx = cosA * lx + sinA * lz;
    rz = -sinA * lx + cosA * lz;
  };

  auto rotateDir = [&](float lx, float ly, float lz, float out[3]) {
    out[0] = cosA * lx + sinA * lz;
    out[1] = ly;
    out[2] = -sinA * lx + cosA * lz;
    normalizeVec3(out);
  };

  auto drawSpherePart = [&](float lx, float ly, float lz, float sx, float sy,
                            float sz, float r, float g, float b, float a) {
    // local offset rotated by the tile's Y rotation
    float rx, rz;
    rotateOffset(lx, lz, rx, rz);

    float model[16] = {cosA * sx,   0.0f, -sinA * sz,  0.0f, 0.0f,      sy,
                       0.0f,        0.0f, sinA * sx,   0.0f, cosA * sz, 0.0f,
                       worldX + rx, ly,   worldZ + rz, 1.0f};

    cubeSh->use();
    cubeSh->setMat4("projection", projMatrix3D);
    cubeSh->setMat4("view", viewMatrix);
    cubeSh->setMat4("model", model);
    cubeSh->setVec4("tileColor", r, g, b, a);
    cubeSh->setFloat("useTexture", 0.0f);

    glBindVertexArray(pufferVAO);
    glDrawElements(GL_TRIANGLES, pufferIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
  };

  auto drawCameraFacingSpherePart = [&](float lx, float ly, float lz, float sx,
                                        float sy, float sz, float r, float g,
                                        float b, float a) {
    float rx, rz;
    rotateOffset(lx, lz, rx, rz);

    float model[16] = {sx,   0.0f, 0.0f, 0.0f, 0.0f, sy,   0.0f, 0.0f,
                       0.0f, 0.0f, sz,   0.0f, worldX + rx, ly,
                       worldZ + rz, 1.0f};

    cubeSh->use();
    cubeSh->setMat4("projection", projMatrix3D);
    cubeSh->setMat4("view", viewMatrix);
    cubeSh->setMat4("model", model);
    cubeSh->setVec4("tileColor", r, g, b, a);
    cubeSh->setFloat("useTexture", 0.0f);

    glBindVertexArray(pufferVAO);
    glDrawElements(GL_TRIANGLES, pufferIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
  };

  auto drawOrientedSpherePart = [&](const float center[3], const float xAxis[3],
                                    const float yAxis[3], const float zAxis[3],
                                    float sx, float sy, float sz, float r,
                                    float g, float b, float a) {
    float model[16] = {xAxis[0] * sx,
                       xAxis[1] * sx,
                       xAxis[2] * sx,
                       0.0f,
                       yAxis[0] * sy,
                       yAxis[1] * sy,
                       yAxis[2] * sy,
                       0.0f,
                       zAxis[0] * sz,
                       zAxis[1] * sz,
                       zAxis[2] * sz,
                       0.0f,
                       worldX + center[0],
                       center[1],
                       worldZ + center[2],
                       1.0f};

    cubeSh->use();
    cubeSh->setMat4("projection", projMatrix3D);
    cubeSh->setMat4("view", viewMatrix);
    cubeSh->setMat4("model", model);
    cubeSh->setVec4("tileColor", r, g, b, a);
    cubeSh->setFloat("useTexture", 0.0f);

    glBindVertexArray(pufferVAO);
    glDrawElements(GL_TRIANGLES, pufferIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
  };

  auto drawFin = [&](float lx, float ly, float lz,
                     const float localX[3], const float localY[3],
                     float sx, float sy, float sz, float r, float g, float b,
                     float a) {
    float rx, rz;
    rotateOffset(lx, lz, rx, rz);

    float xAxis[3], yAxis[3];
    rotateDir(localX[0], localX[1], localX[2], xAxis);
    rotateDir(localY[0], localY[1], localY[2], yAxis);

    float zAxis[3];
    crossVec3(xAxis, yAxis, zAxis);
    normalizeVec3(zAxis);

    float model[16] = {xAxis[0] * sx,
                       xAxis[1] * sx,
                       xAxis[2] * sx,
                       0.0f,
                       yAxis[0] * sy,
                       yAxis[1] * sy,
                       yAxis[2] * sy,
                       0.0f,
                       zAxis[0] * sz,
                       zAxis[1] * sz,
                       zAxis[2] * sz,
                       0.0f,
                       worldX + rx,
                       ly,
                       worldZ + rz,
                       1.0f};

    cubeSh->use();
    cubeSh->setMat4("projection", projMatrix3D);
    cubeSh->setMat4("view", viewMatrix);
    cubeSh->setMat4("model", model);
    cubeSh->setVec4("tileColor", r, g, b, a);
    cubeSh->setFloat("useTexture", 0.0f);

    glBindVertexArray(pufferFinVAO);
    glDrawElements(GL_TRIANGLES, pufferFinIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
  };

  auto drawSpike = [&](float lx, float ly, float lz, float radialScale,
                       float heightScale) {
    // rotate spike position with the tile
    float px, pz;
    rotateOffset(lx, lz, px, pz);

    // outward direction from local center
    float dx = lx;
    float dy = ly;
    float dz = lz;
    float len = sqrtf(dx * dx + dy * dy + dz * dz);
    if (len < 1e-5f)
      return;
    dx /= len;
    dy /= len;
    dz /= len;

    // rotate direction with tile Y rotation
    float rdx = cosA * dx + sinA * dz;
    float rdz = -sinA * dx + cosA * dz;
    float rdy = dy;

    float dir[3] = {rdx, rdy, rdz};
    normalizeVec3(dir);

    float refUp[3] = {0.0f, 1.0f, 0.0f};
    if (fabsf(dir[1]) > 0.95f) {
      refUp[0] = 1.0f;
      refUp[1] = 0.0f;
      refUp[2] = 0.0f;
    }

    float right[3];
    crossVec3(refUp, dir, right);
    normalizeVec3(right);

    float forward[3];
    crossVec3(dir, right, forward);
    normalizeVec3(forward);

    float model[16] = {right[0] * radialScale,
                       right[1] * radialScale,
                       right[2] * radialScale,
                       0.0f,
                       dir[0] * heightScale,
                       dir[1] * heightScale,
                       dir[2] * heightScale,
                       0.0f,
                       forward[0] * radialScale,
                       forward[1] * radialScale,
                       forward[2] * radialScale,
                       0.0f,
                       worldX + px,
                       ly,
                       worldZ + pz,
                       1.0f};

    cubeSh->use();
    cubeSh->setMat4("projection", projMatrix3D);
    cubeSh->setMat4("view", viewMatrix);
    cubeSh->setMat4("model", model);
    cubeSh->setVec4("tileColor", 1.00f * brightness, 0.55f * brightness,
                    0.12f * brightness, 1.0f);
    cubeSh->setFloat("useTexture", 0.0f);

    glBindVertexArray(spikeVAO);
    glDrawElements(GL_TRIANGLES, spikeIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
  };

  const float rightAxis[3] = {1.0f, 0.0f, 0.0f};
  const float leftAxis[3] = {-1.0f, 0.0f, 0.0f};
  const float upAxis[3] = {0.0f, 1.0f, 0.0f};
  const float backAxis[3] = {0.0f, 0.0f, -1.0f};
  const float frontAxis[3] = {0.0f, 0.0f, 1.0f};

  // Fins first, so the inflated body visually anchors them.
  drawFin(0.38f, -0.05f, 0.03f, rightAxis, upAxis, 0.72f, 0.62f, 0.07f,
          1.00f * brightness, 0.64f * brightness, 0.13f * brightness, 0.98f);
  drawFin(-0.38f, -0.05f, 0.03f, leftAxis, upAxis, 0.72f, 0.62f, 0.07f,
          1.00f * brightness, 0.64f * brightness, 0.13f * brightness, 0.98f);
  drawFin(0.0f, -0.03f, -0.45f, rightAxis, upAxis, 0.68f, 0.86f, 0.07f,
          1.00f * brightness, 0.73f * brightness, 0.16f * brightness, 0.98f);
  drawFin(0.0f, 0.03f, -0.48f, rightAxis, frontAxis, 0.50f, 0.42f, 0.06f,
          0.95f * brightness, 0.50f * brightness, 0.10f * brightness, 0.95f);

  // Inflated body and large cream belly patch.
  drawSpherePart(0.0f, 0.0f, 0.0f, 1.18f, 0.95f, 1.02f,
                 1.00f * brightness, 0.58f * brightness, 0.10f * brightness,
                 1.0f);
  drawSpherePart(0.0f, -0.15f, 0.13f, 0.96f, 0.54f, 0.70f,
                 1.00f * brightness, 0.88f * brightness, 0.48f * brightness,
                 1.0f);

  // Raised face: attach the eye/mouth plane to the fish's own rotating front.
  float faceNormal[3], faceRight[3];
  rotateDir(0.0f, 0.0f, 1.0f, faceNormal);
  rotateDir(1.0f, 0.0f, 0.0f, faceRight);
  float faceUp[3] = {0.0f, 1.0f, 0.0f};

  auto facePoint = [&](float x, float y, float push, float out[3]) {
    out[0] = faceNormal[0] * push + faceRight[0] * x + faceUp[0] * y;
    out[1] = faceNormal[1] * push + faceRight[1] * x + faceUp[1] * y;
    out[2] = faceNormal[2] * push + faceRight[2] * x + faceUp[2] * y;
  };

  float leftEye[3], rightEye[3], leftPupil[3], rightPupil[3];
  float leftHighlight[3], rightHighlight[3], mouth[3], lip[3];
  facePoint(-0.16f, 0.05f, 0.37f, leftEye);
  facePoint(0.16f, 0.05f, 0.37f, rightEye);
  facePoint(-0.16f, 0.05f, 0.43f, leftPupil);
  facePoint(0.16f, 0.05f, 0.43f, rightPupil);
  facePoint(-0.205f, 0.105f, 0.455f, leftHighlight);
  facePoint(0.115f, 0.105f, 0.455f, rightHighlight);
  facePoint(0.0f, -0.15f, 0.43f, mouth);
  facePoint(0.0f, -0.125f, 0.455f, lip);

  drawOrientedSpherePart(leftEye, faceRight, faceUp, faceNormal, 0.27f, 0.28f,
                         0.13f, 1.00f * brightness, 0.86f * brightness,
                         0.47f * brightness, 1.0f);
  drawOrientedSpherePart(rightEye, faceRight, faceUp, faceNormal, 0.27f, 0.28f,
                         0.13f, 1.00f * brightness, 0.86f * brightness,
                         0.47f * brightness, 1.0f);
  drawOrientedSpherePart(leftPupil, faceRight, faceUp, faceNormal, 0.15f,
                         0.15f, 0.050f, 0.16f, 0.07f, 0.025f, 1.0f);
  drawOrientedSpherePart(rightPupil, faceRight, faceUp, faceNormal, 0.15f,
                         0.15f, 0.050f, 0.16f, 0.07f, 0.025f, 1.0f);
  drawOrientedSpherePart(leftHighlight, faceRight, faceUp, faceNormal, 0.045f,
                         0.045f, 0.018f, 1.0f, 1.0f, 0.92f, 1.0f);
  drawOrientedSpherePart(rightHighlight, faceRight, faceUp, faceNormal, 0.045f,
                         0.045f, 0.018f, 1.0f, 1.0f, 0.92f, 1.0f);
  drawOrientedSpherePart(mouth, faceRight, faceUp, faceNormal, 0.19f, 0.060f,
                         0.035f, 0.48f * brightness, 0.17f * brightness,
                         0.045f * brightness, 1.0f);
  drawOrientedSpherePart(lip, faceRight, faceUp, faceNormal, 0.12f, 0.030f,
                         0.018f, 0.85f * brightness, 0.35f * brightness,
                         0.08f * brightness, 1.0f);

  // Markings: yellow top spots and softer brown belly freckles.
  const float topSpotR = 1.00f * brightness;
  const float topSpotG = 0.78f * brightness;
  const float topSpotB = 0.20f * brightness;
  const float bellySpotR = 0.58f * brightness;
  const float bellySpotG = 0.36f * brightness;
  const float bellySpotB = 0.12f * brightness;
  drawSpherePart(-0.24f, 0.25f, 0.10f, 0.070f, 0.045f, 0.035f, topSpotR,
                 topSpotG, topSpotB, 1.0f);
  drawSpherePart(0.03f, 0.30f, 0.13f, 0.080f, 0.050f, 0.035f, topSpotR,
                 topSpotG, topSpotB, 1.0f);
  drawSpherePart(0.25f, 0.19f, 0.04f, 0.070f, 0.045f, 0.035f, topSpotR,
                 topSpotG, topSpotB, 1.0f);
  drawSpherePart(-0.31f, 0.05f, 0.18f, 0.060f, 0.040f, 0.030f, topSpotR,
                 topSpotG, topSpotB, 1.0f);
  drawSpherePart(0.27f, -0.02f, 0.18f, 0.060f, 0.040f, 0.030f, topSpotR,
                 topSpotG, topSpotB, 1.0f);
  drawSpherePart(-0.16f, -0.28f, 0.15f, 0.050f, 0.035f, 0.025f, bellySpotR,
                 bellySpotG, bellySpotB, 1.0f);
  drawSpherePart(0.10f, -0.31f, 0.12f, 0.050f, 0.035f, 0.025f, bellySpotR,
                 bellySpotG, bellySpotB, 1.0f);
  drawSpherePart(0.29f, -0.21f, 0.02f, 0.045f, 0.032f, 0.023f, bellySpotR,
                 bellySpotG, bellySpotB, 1.0f);

  // Spines mostly on the silhouette/top/back, leaving the face readable.
  drawSpike(0.00f, 0.38f, -0.06f, 0.86f, 1.05f);
  drawSpike(0.18f, 0.30f, 0.03f, 0.74f, 0.88f);
  drawSpike(-0.18f, 0.30f, 0.03f, 0.74f, 0.88f);
  drawSpike(0.31f, 0.18f, -0.05f, 0.68f, 0.82f);
  drawSpike(-0.31f, 0.18f, -0.05f, 0.68f, 0.82f);
  drawSpike(0.30f, 0.02f, -0.20f, 0.64f, 0.78f);
  drawSpike(-0.30f, 0.02f, -0.20f, 0.64f, 0.78f);
  drawSpike(0.16f, -0.16f, -0.25f, 0.60f, 0.72f);
  drawSpike(-0.16f, -0.16f, -0.25f, 0.60f, 0.72f);
  drawSpike(0.00f, 0.18f, -0.35f, 0.68f, 0.84f);
  drawSpike(0.00f, -0.28f, -0.10f, 0.60f, 0.76f);
  drawSpike(0.33f, -0.08f, 0.05f, 0.54f, 0.66f);
  drawSpike(-0.33f, -0.08f, 0.05f, 0.54f, 0.66f);
}
// draw piranha
void Renderer::drawPiranha(float worldX, float worldZ, bool horizontal,
                           float brightness) {
  (void)horizontal;
  float totalYaw = rotAngle;

  float cosT = cosf(totalYaw);
  float sinT = sinf(totalYaw);

  auto rotateOffset = [&](float lx, float lz, float &rx, float &rz) {
    rx = cosT * lx + sinT * lz;
    rz = -sinT * lx + cosT * lz;
  };

  auto rotateDir = [&](float lx, float ly, float lz, float out[3]) {
    out[0] = cosT * lx + sinT * lz;
    out[1] = ly;
    out[2] = -sinT * lx + cosT * lz;
    normalizeVec3(out);
  };

  auto drawSpherePart = [&](float lx, float ly, float lz, float sx, float sy,
                            float sz, float r, float g, float b, float a) {
    float rx, rz;
    rotateOffset(lx, lz, rx, rz);

    float model[16] = {cosT * sx,   0.0f, -sinT * sz,  0.0f, 0.0f,      sy,
                       0.0f,        0.0f, sinT * sx,   0.0f, cosT * sz, 0.0f,
                       worldX + rx, ly,   worldZ + rz, 1.0f};

    cubeSh->use();
    cubeSh->setMat4("projection", projMatrix3D);
    cubeSh->setMat4("view", viewMatrix);
    cubeSh->setMat4("model", model);
    cubeSh->setVec4("tileColor", r, g, b, a);
    cubeSh->setFloat("useTexture", 0.0f);

    glBindVertexArray(pufferVAO);
    glDrawElements(GL_TRIANGLES, pufferIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
  };

  auto drawFin = [&](float lx, float ly, float lz, const float localX[3],
                     const float localY[3], float sx, float sy, float sz,
                     float r, float g, float b, float a) {
    float rx, rz;
    rotateOffset(lx, lz, rx, rz);

    float xAxis[3], yAxis[3];
    rotateDir(localX[0], localX[1], localX[2], xAxis);
    rotateDir(localY[0], localY[1], localY[2], yAxis);

    float zAxis[3];
    crossVec3(xAxis, yAxis, zAxis);
    normalizeVec3(zAxis);

    float model[16] = {xAxis[0] * sx,
                       xAxis[1] * sx,
                       xAxis[2] * sx,
                       0.0f,
                       yAxis[0] * sy,
                       yAxis[1] * sy,
                       yAxis[2] * sy,
                       0.0f,
                       zAxis[0] * sz,
                       zAxis[1] * sz,
                       zAxis[2] * sz,
                       0.0f,
                       worldX + rx,
                       ly,
                       worldZ + rz,
                       1.0f};

    cubeSh->use();
    cubeSh->setMat4("projection", projMatrix3D);
    cubeSh->setMat4("view", viewMatrix);
    cubeSh->setMat4("model", model);
    cubeSh->setVec4("tileColor", r, g, b, a);
    cubeSh->setFloat("useTexture", 0.0f);

    glBindVertexArray(pufferFinVAO);
    glDrawElements(GL_TRIANGLES, pufferFinIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
  };

  float model[16] = {cosT, 0.0f, -sinT, 0.0f, 0.0f,   1.0f, 0.0f,   0.0f,
                     sinT, 0.0f, cosT,  0.0f, worldX, 0.0f, worldZ, 1.0f};

  const float rightAxis[3] = {1.0f, 0.0f, 0.0f};
  const float leftAxis[3] = {-1.0f, 0.0f, 0.0f};
  const float upAxis[3] = {0.0f, 1.0f, 0.0f};
  const float frontAxis[3] = {0.0f, 0.0f, 1.0f};
  const float backAxis[3] = {0.0f, 0.0f, -1.0f};

  // 1) Draw the red piranha body.
  cubeSh->use();
  cubeSh->setMat4("projection", projMatrix3D);
  cubeSh->setMat4("view", viewMatrix);
  cubeSh->setMat4("model", model);
  cubeSh->setVec4("tileColor", 0.88f * brightness, 0.04f * brightness,
                  0.035f * brightness, 1.0f);
  cubeSh->setFloat("useTexture", 0.0f);

  glBindVertexArray(piranhaVAO);
  glDrawElements(GL_TRIANGLES, piranhaIndexCount, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);

  auto drawTooth = [&](float lx, float ly, float lz) {
    float px, pz;
    rotateOffset(lx, lz, px, pz);

    float dir[3];
    rotateDir(lx, ly, lz, dir);

    float refUp[3] = {0.0f, 1.0f, 0.0f};
    if (fabsf(dir[1]) > 0.95f) {
      refUp[0] = 1.0f;
      refUp[1] = 0.0f;
      refUp[2] = 0.0f;
    }

    float right[3];
    crossVec3(refUp, dir, right);
    normalizeVec3(right);

    float forward[3];
    crossVec3(dir, right, forward);
    normalizeVec3(forward);

    const float radialScale = 0.36f;
    const float heightScale = 0.50f;
    float toothModel[16] = {right[0] * radialScale,
                            right[1] * radialScale,
                            right[2] * radialScale,
                            0.0f,
                            dir[0] * heightScale,
                            dir[1] * heightScale,
                            dir[2] * heightScale,
                            0.0f,
                            forward[0] * radialScale,
                            forward[1] * radialScale,
                            forward[2] * radialScale,
                            0.0f,
                            worldX + px,
                            ly,
                            worldZ + pz,
                            1.0f};

    cubeSh->use();
    cubeSh->setMat4("projection", projMatrix3D);
    cubeSh->setMat4("view", viewMatrix);
    cubeSh->setMat4("model", toothModel);
    cubeSh->setVec4("tileColor", 1.0f * brightness, 0.96f * brightness,
                    0.86f * brightness, 1.0f);
    cubeSh->setFloat("useTexture", 0.0f);

    glBindVertexArray(spikeVAO);
    glDrawElements(GL_TRIANGLES, spikeIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
  };

  // 2) Eyes on the right and left sides of the pyramid head.
  drawSpherePart(0.12f, 0.075f, 0.225f, 0.098f, 0.098f, 0.046f, 1.0f,
                 0.94f * brightness, 0.78f * brightness, 1.0f);
  drawSpherePart(-0.12f, 0.075f, 0.225f, 0.098f, 0.098f, 0.046f, 1.0f,
                 0.94f * brightness, 0.78f * brightness, 1.0f);
  drawSpherePart(0.150f, 0.073f, 0.264f, 0.044f, 0.044f, 0.017f, 0.03f,
                 0.012f, 0.006f, 1.0f);
  drawSpherePart(-0.150f, 0.073f, 0.264f, 0.044f, 0.044f, 0.017f, 0.03f,
                 0.012f, 0.006f, 1.0f);

  drawTooth(0.0f, 0.050f, 0.315f);
  drawTooth(0.050f, 0.010f, 0.305f);
  drawTooth(-0.050f, 0.010f, 0.305f);
  drawTooth(0.035f, -0.055f, 0.315f);
  drawTooth(-0.035f, -0.055f, 0.315f);
}

// ─────────────────────────────────────────────────────────────────────────────
// 4×4 matrix inverse (general, column-major)
// ─────────────────────────────────────────────────────────────────────────────

void Renderer::invertMatrix4(const float m[16], float inv[16]) {
  // sub-expressions shared between cofactors
  float a00 = m[0], a01 = m[1], a02 = m[2], a03 = m[3];
  float a10 = m[4], a11 = m[5], a12 = m[6], a13 = m[7];
  float a20 = m[8], a21 = m[9], a22 = m[10], a23 = m[11];
  float a30 = m[12], a31 = m[13], a32 = m[14], a33 = m[15];

  float b00 = a00 * a11 - a01 * a10;
  float b01 = a00 * a12 - a02 * a10;
  float b02 = a00 * a13 - a03 * a10;
  float b03 = a01 * a12 - a02 * a11;
  float b04 = a01 * a13 - a03 * a11;
  float b05 = a02 * a13 - a03 * a12;
  float b06 = a20 * a31 - a21 * a30;
  float b07 = a20 * a32 - a22 * a30;
  float b08 = a20 * a33 - a23 * a30;
  float b09 = a21 * a32 - a22 * a31;
  float b10 = a21 * a33 - a23 * a31;
  float b11 = a22 * a33 - a23 * a32;

  float det =
      b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
  if (fabsf(det) < 1e-12f) {
    memset(inv, 0, 16 * sizeof(float));
    return;
  }
  float id = 1.0f / det;

  inv[0] = (a11 * b11 - a12 * b10 + a13 * b09) * id;
  inv[1] = (-a01 * b11 + a02 * b10 - a03 * b09) * id;
  inv[2] = (a31 * b05 - a32 * b04 + a33 * b03) * id;
  inv[3] = (-a21 * b05 + a22 * b04 - a23 * b03) * id;
  inv[4] = (-a10 * b11 + a12 * b08 - a13 * b07) * id;
  inv[5] = (a00 * b11 - a02 * b08 + a03 * b07) * id;
  inv[6] = (-a30 * b05 + a32 * b02 - a33 * b01) * id;
  inv[7] = (a20 * b05 - a22 * b02 + a23 * b01) * id;
  inv[8] = (a10 * b10 - a11 * b08 + a13 * b06) * id;
  inv[9] = (-a00 * b10 + a01 * b08 - a03 * b06) * id;
  inv[10] = (a30 * b04 - a31 * b02 + a33 * b00) * id;
  inv[11] = (-a20 * b04 + a21 * b02 - a23 * b00) * id;
  inv[12] = (-a10 * b09 + a11 * b07 - a12 * b06) * id;
  inv[13] = (a00 * b09 - a01 * b07 + a02 * b06) * id;
  inv[14] = (-a30 * b03 + a31 * b01 - a32 * b00) * id;
  inv[15] = (a20 * b03 - a21 * b01 + a22 * b00) * id;
}

// ─────────────────────────────────────────────────────────────────────────────
// Ray construction from screen coordinates
// ─────────────────────────────────────────────────────────────────────────────

void Renderer::buildRay(double screenX, double screenY, float rayOrigin[3],
                        float rayDir[3]) const {
  // 1. Convert screen coords to NDC  (-1..+1)
  //    screenX, screenY are in the logical 900×900 coordinate space
  float ndcX = (2.0f * (float)screenX / (float)winW) - 1.0f;
  float ndcY = 1.0f - (2.0f * (float)screenY / (float)winH); // flip Y

  // 2. Compute inverse(projection * view)
  //    VP = projMatrix3D * viewMatrix  (column-major multiply)
  float vp[16];
  memset(vp, 0, sizeof(vp));
  for (int c = 0; c < 4; c++)
    for (int r = 0; r < 4; r++)
      for (int k = 0; k < 4; k++)
        vp[c * 4 + r] += projMatrix3D[k * 4 + r] * viewMatrix[c * 4 + k];

  float ivp[16];
  invertMatrix4(vp, ivp);

  // 3. Unproject near point (NDC z = -1) and far point (NDC z = +1)
  //    clip = (ndcX, ndcY, z, 1)  — since w=1 in NDC
  auto unproject = [&](float nx, float ny, float nz, float out[3]) {
    float x = ivp[0] * nx + ivp[4] * ny + ivp[8] * nz + ivp[12];
    float y = ivp[1] * nx + ivp[5] * ny + ivp[9] * nz + ivp[13];
    float z = ivp[2] * nx + ivp[6] * ny + ivp[10] * nz + ivp[14];
    float w = ivp[3] * nx + ivp[7] * ny + ivp[11] * nz + ivp[15];
    if (fabsf(w) > 1e-12f) {
      x /= w;
      y /= w;
      z /= w;
    }
    out[0] = x;
    out[1] = y;
    out[2] = z;
  };

  float nearPt[3], farPt[3];
  unproject(ndcX, ndcY, -1.0f, nearPt);
  unproject(ndcX, ndcY, 1.0f, farPt);

  rayOrigin[0] = nearPt[0];
  rayOrigin[1] = nearPt[1];
  rayOrigin[2] = nearPt[2];

  rayDir[0] = farPt[0] - nearPt[0];
  rayDir[1] = farPt[1] - nearPt[1];
  rayDir[2] = farPt[2] - nearPt[2];

  // Normalize direction
  float len = sqrtf(rayDir[0] * rayDir[0] + rayDir[1] * rayDir[1] +
                    rayDir[2] * rayDir[2]);
  if (len > 1e-8f) {
    rayDir[0] /= len;
    rayDir[1] /= len;
    rayDir[2] /= len;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Screen-to-board picking via ray–plane intersection
// ─────────────────────────────────────────────────────────────────────────────

std::pair<int, int> Renderer::screenToBoard(double screenX,
                                            double screenY) const {
  float rayO[3], rayD[3];
  buildRay(screenX, screenY, rayO, rayD);

  // Board plane: y = 0  (cubes are centered at y=0)
  // Ray: P = O + t*D,  solve O.y + t*D.y = 0  →  t = -O.y / D.y
  if (fabsf(rayD[1]) < 1e-8f)
    return {-1, -1}; // ray parallel to plane

  float t = -rayO[1] / rayD[1];
  if (t < 0.0f)
    return {-1, -1}; // intersection behind camera

  float hitX = rayO[0] + t * rayD[0]; // world X → column
  float hitZ = rayO[2] + t * rayD[2]; // world Z → row

  // Board origin: cubes are placed at (col, 0, row) where col,row ∈ [0..7]
  // Cube centers are at integer coordinates, so cell boundaries are at ±0.5
  // Convert to board-local coords: offset by +0.5 so that floor() gives the
  // right cell
  int col = (int)floorf(hitX + 0.5f);
  int row = (int)floorf(hitZ + 0.5f);

  // Clamp to valid board bounds
  if (row < 0 || row >= BOARD_ROWS || col < 0 || col >= BOARD_COLS)
    return {-1, -1};

  return {row, col};
}

// ─────────────────────────────────────────────────────────────────────────────
// 3D selection highlight — translucent wireframe-style cube in world space
// ─────────────────────────────────────────────────────────────────────────────

void Renderer::drawSelectionHighlight(float worldX, float worldZ) {
  // Draw a slightly larger, translucent white cube to indicate selection
  float scale = 0.42f; // slightly bigger than tile half-size (0.35)
  float cosA = cosf(rotAngle);
  float sinA = sinf(rotAngle);

  float model[16] = {
      cosA * scale, 0.0f, -sinA * scale, 0.0f, 0.0f,   scale, 0.0f,   0.0f,
      sinA * scale, 0.0f, cosA * scale,  0.0f, worldX, 0.0f,  worldZ, 1.0f};

  cubeSh->use();
  cubeSh->setMat4("projection", projMatrix3D);
  cubeSh->setMat4("view", viewMatrix);
  cubeSh->setMat4("model", model);
  cubeSh->setVec4("tileColor", 1.0f, 1.0f, 1.0f, 0.35f);
  cubeSh->setFloat("useTexture", 0.0f);

  glBindVertexArray(cubeVAO);
  glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}
void Renderer::drawEffectBox(float worldX, float worldZ, float scaleX,
                             float scaleY, float scaleZ, float r, float g,
                             float b, float a, float yaw) {
  if (a <= 0.01f)
    return;

  float cosA = cosf(yaw);
  float sinA = sinf(yaw);

  float model[16] = {cosA * scaleX,
                     0.0f,
                     -sinA * scaleZ,
                     0.0f,
                     0.0f,
                     scaleY,
                     0.0f,
                     0.0f,
                     sinA * scaleX,
                     0.0f,
                     cosA * scaleZ,
                     0.0f,
                     worldX,
                     0.10f,
                     worldZ,
                     1.0f};

  cubeSh->use();
  cubeSh->setMat4("projection", projMatrix3D);
  cubeSh->setMat4("view", viewMatrix);
  cubeSh->setMat4("model", model);
  cubeSh->setVec4("tileColor", r, g, b, a);
  cubeSh->setFloat("useTexture", 0.0f);

  glBindVertexArray(cubeVAO);
  glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void Renderer::drawEffectSprite(float worldX, float worldZ, float scaleX,
                                float scaleZ, float r, float g, float b,
                                float a, int effectMode, float progress,
                                float yaw) {
  if (a <= 0.01f)
    return;

  float cosA = cosf(yaw);
  float sinA = sinf(yaw);

  float model[16] = {
      cosA * scaleX,  0.0f, -sinA * scaleZ, 0.0f,
      0.0f,           1.0f,  0.0f,          0.0f,
      sinA * scaleX,  0.0f,  cosA * scaleZ, 0.0f,
      worldX,         0.14f, worldZ,        1.0f};

  effectShader.use();
  effectShader.setMat4("projection", projMatrix3D);
  effectShader.setMat4("view", viewMatrix);
  effectShader.setMat4("model", model);
  effectShader.setVec4("effectColor", r, g, b, a);
  effectShader.setInt("effectMode", effectMode);
  effectShader.setFloat("progress", progress);

  glBindVertexArray(effectVAO);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void Renderer::drawExplosionEffect(const ExplosionAnim &anim) {
  if (!anim.active || anim.affectedCells.empty())
    return;

  float p = clamp01(anim.progress);
  float fade = 1.0f - p;
  float pi = 3.14159265f;

  int originR = anim.originR;
  int originC = anim.originC;
  if (originR < 0 || originC < 0) {
    originR = anim.affectedCells.front().first;
    originC = anim.affectedCells.front().second;
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive glow
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

bool isDoublePufferfish =
    anim.type == SpecialType::COMBO_DOUBLE_PUFFER;

bool isComboPufferPiranha =
    anim.type == SpecialType::COMBO_PUFFER_PIRANHA;

  if (anim.type == SpecialType::PIRANHA_ROW ||
      anim.type == SpecialType::PIRANHA_COL) {
    bool horizontal = (anim.type == SpecialType::PIRANHA_ROW);

    int maxDist = 1;
    for (auto [r, c] : anim.affectedCells) {
      int dist = horizontal ? std::abs(c - originC) : std::abs(r - originR);
      maxDist = std::max(maxDist, dist);
    }

    // Soft beam trail over affected cells
    for (auto [r, c] : anim.affectedCells) {
      int dist = horizontal ? std::abs(c - originC) : std::abs(r - originR);
      float arrival = 0.55f * ((float)dist / (float)maxDist);

      if (p + 0.04f < arrival)
        continue;

      float local = clamp01((p - arrival) / 0.38f);
      float alpha = clamp01(0.60f * (1.0f - local));

      float sx = horizontal ? 1.55f : 0.58f;
      float sz = horizontal ? 0.58f : 1.55f;

      drawEffectSprite((float)c, (float)r, sx, sz, 1.00f, 0.45f, 0.08f,
                       alpha, 1, local, horizontal ? 0.0f : 1.5707963f);

      drawEffectSprite((float)c, (float)r, sx * 0.72f, sz * 0.72f, 1.00f,
                       0.90f, 0.30f, alpha * 0.70f, 1, local,
                       horizontal ? 0.0f : 1.5707963f);
    }

    // Rocket heads moving outward from the center
    for (int dir = -1; dir <= 1; dir += 2) {
      float headDist = p * ((float)maxDist + 0.7f);

      float headX =
          horizontal ? (float)originC + dir * headDist : (float)originC;
      float headZ =
          horizontal ? (float)originR : (float)originR + dir * headDist;

      bool insideBoard =
          headX >= -0.8f && headX <= (float)BOARD_COLS - 0.2f &&
          headZ >= -0.8f && headZ <= (float)BOARD_ROWS - 0.2f;

      if (!insideBoard)
        continue;

      float rocketAlpha = clamp01(0.95f - p * 0.25f);

      drawEffectSprite(headX, headZ, horizontal ? 1.20f : 0.70f,
                       horizontal ? 0.70f : 1.20f, 1.00f, 0.95f, 0.35f,
                       rocketAlpha, 0, p, 0.0f);

      drawEffectSprite(headX, headZ, horizontal ? 0.65f : 0.42f,
                       horizontal ? 0.42f : 0.65f, 1.00f, 0.35f, 0.10f,
                       rocketAlpha * 0.65f, 0, p, 0.0f);
    }

    // Origin flash
    drawEffectSprite((float)originC, (float)originR, 1.35f, 1.35f, 1.00f,
                     0.75f, 0.18f, 0.55f * fade, 0, p, 0.0f);

  } else if (isDoublePufferfish) {
  // Pufferfish + Pufferfish combo:
  // Bigger TNT + TNT style explosion over a 6x6 area.

  float blastGrow = clamp01(p / 0.65f);
  float blastAlpha = clamp01(0.95f * (1.0f - p * 0.25f));

  // Large central flash
  drawEffectSprite((float)originC, (float)originR, 2.2f + 2.4f * p,
                   2.2f + 2.4f * p, 1.00f, 0.92f, 0.25f,
                   0.85f * fade, 0, p, 0.0f);

  drawEffectSprite((float)originC, (float)originR, 2.8f + 3.0f * p,
                   2.8f + 3.0f * p, 0.25f, 0.95f, 1.00f,
                   0.75f * fade, 0, p, 0.0f);

  // Two shockwave rings, slightly staggered, to make the blast feel stronger.
  drawEffectSprite((float)originC, (float)originR, 2.6f + 5.4f * p,
                   2.6f + 5.4f * p, 0.35f, 0.95f, 1.00f,
                   0.80f * fade, 2, p, 0.0f);

  drawEffectSprite((float)originC, (float)originR, 1.6f + 4.1f * blastGrow,
                   1.6f + 4.1f * blastGrow, 1.00f, 0.82f, 0.22f,
                   0.45f * fade, 2, blastGrow, 0.0f);

  // Soft flashes across the 6x6 affected area.
  for (auto [r, c] : anim.affectedCells) {
    float dr = (float)(r - originR);
    float dc = (float)(c - originC);
    float dist = sqrtf(dr * dr + dc * dc);

    float arrival = 0.055f * dist;
    if (p + 0.03f < arrival)
      continue;

    float local = clamp01((p - arrival) / 0.60f);
    float alpha = clamp01(blastAlpha * 0.36f * (1.0f - local));

    drawEffectSprite((float)c, (float)r, 1.15f + 0.85f * local,
                     1.15f + 0.85f * local, 0.45f, 0.95f, 1.00f,
                     alpha, 0, local, 0.0f);

    drawEffectSprite((float)c, (float)r, 0.75f + 0.45f * local,
                     0.75f + 0.45f * local, 1.00f, 0.92f, 0.28f,
                     alpha * 0.65f, 0, local, 0.0f);
  }
  } else if (isComboPufferPiranha) {
    // Pufferfish + Piranha combo:
    // 3 full horizontal beams + 3 full vertical beams + central mega shockwave.

    float beamGrow = clamp01(p / 0.55f);
    float beamAlpha = clamp01(0.85f * (1.0f - p * 0.35f));

    // Central mega flash
    drawEffectSprite((float)originC, (float)originR, 1.8f + 1.6f * p,
                     1.8f + 1.6f * p, 1.00f, 0.90f, 0.25f, 0.80f * fade, 0,
                     p, 0.0f);

    drawEffectSprite((float)originC, (float)originR, 2.2f + 2.2f * p,
                     2.2f + 2.2f * p, 0.25f, 0.95f, 1.00f, 0.65f * fade, 0,
                     p, 0.0f);

    // Expanding circular shockwave
    drawEffectSprite((float)originC, (float)originR, 2.2f + 5.0f * p,
                     2.2f + 5.0f * p, 0.30f, 0.95f, 1.00f, 0.75f * fade, 2,
                     p, 0.0f);

    // 3 horizontal full-row beams
    for (int rr = originR - 1; rr <= originR + 1; rr++) {
      if (rr < 0 || rr >= BOARD_ROWS)
        continue;

      float rowOffset = (float)(rr - originR);
      float delay = 0.06f * std::abs(rr - originR);
      float local = clamp01((p - delay) / 0.65f);
      float alpha = beamAlpha * (1.0f - 0.18f * std::abs(rowOffset));

      drawEffectSprite(3.5f, (float)rr, 8.8f * beamGrow, 0.72f, 1.00f, 0.42f,
                       0.08f, alpha, 1, local, 0.0f);

      drawEffectSprite(3.5f, (float)rr, 8.2f * beamGrow, 0.42f, 1.00f, 0.92f,
                       0.30f, alpha * 0.55f, 1, local, 0.0f);
    }

    // 3 vertical full-column beams
    for (int cc = originC - 1; cc <= originC + 1; cc++) {
      if (cc < 0 || cc >= BOARD_COLS)
        continue;

      float colOffset = (float)(cc - originC);
      float delay = 0.06f * std::abs(cc - originC);
      float local = clamp01((p - delay) / 0.65f);
      float alpha = beamAlpha * (1.0f - 0.18f * std::abs(colOffset));

      drawEffectSprite((float)cc, 3.5f, 0.72f, 8.8f * beamGrow, 0.25f, 0.95f,
                       1.00f, alpha, 1, local, 1.5707963f);

      drawEffectSprite((float)cc, 3.5f, 0.42f, 8.2f * beamGrow, 1.00f, 0.92f,
                       0.30f, alpha * 0.50f, 1, local, 1.5707963f);
    }

    // Small flashes on affected cells, so the clear area is readable.
    for (auto [r, c] : anim.affectedCells) {
      float dr = (float)(r - originR);
      float dc = (float)(c - originC);
      float dist = sqrtf(dr * dr + dc * dc);
      float arrival = 0.035f * dist;

      if (p + 0.03f < arrival)
        continue;

      float local = clamp01((p - arrival) / 0.55f);
      float alpha = clamp01(0.22f * (1.0f - local));

      drawEffectSprite((float)c, (float)r, 0.90f + 0.35f * local,
                       0.90f + 0.35f * local, 0.45f, 0.95f, 1.00f, alpha, 0,
                       local, 0.0f);
    }

  } else if (anim.type == SpecialType::PUFFERFISH) {
    // Central flash
    drawEffectSprite((float)originC, (float)originR, 1.2f + 1.3f * p,
                     1.2f + 1.3f * p, 0.35f, 0.95f, 1.00f, 0.55f * fade, 0,
                     p, 0.0f);

    drawEffectSprite((float)originC, (float)originR, 0.85f + 0.9f * p,
                     0.85f + 0.9f * p, 1.00f, 0.95f, 0.35f, 0.38f * fade, 0,
                     p, 0.0f);

    // Expanding shockwave ring
    drawEffectSprite((float)originC, (float)originR, 1.3f + 3.2f * p,
                     1.3f + 3.2f * p, 0.35f, 0.95f, 1.00f, 0.60f * fade, 2,
                     p, 0.0f);

    // Cell flashes inside the 3x3 area
    for (auto [r, c] : anim.affectedCells) {
      float dr = (float)(r - originR);
      float dc = (float)(c - originC);
      float dist = sqrtf(dr * dr + dc * dc);

      float arrival = 0.18f * dist;
      if (p + 0.03f < arrival)
        continue;

      float local = clamp01((p - arrival) / 0.45f);
      float alpha = clamp01(0.45f * (1.0f - local));

      drawEffectSprite((float)c, (float)r, 0.9f + 0.65f * local,
                       0.9f + 0.65f * local, 0.45f, 0.95f, 1.00f, alpha, 0,
                       local, 0.0f);
    }
  }

  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// ─────────────────────────────────────────────────────────────────────────────
// Frame rendering
// ─────────────────────────────────────────────────────────────────────────────

void Renderer::beginFrame() {
  glClearColor(0.03f, 0.10f, 0.22f, 1.0f); // deep ocean blue
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::drawBoard(const Board &board, int selRow, int selCol,
                         const SwapAnim &swapAnim, const FallAnim &fallAnim,
                         const ExplosionAnim &explosionAnim)  {
  constexpr float tileSize = 80.0f; // 90 (cell) - 2*5 (padding)

  // ── Pass 1: 2D overlay — empty cells only ────────────────────────────
  // Disable depth test for 2D overlay so it draws cleanly
  glDisable(GL_DEPTH_TEST);

  for (int r = 0; r < board.rows; r++) {
    for (int c = 0; c < board.cols; c++) {
      const Cell &cell = board.grid[r][c];

      if (cell.type == TileType::NONE) {
        auto [px, py] = Board::gridToWorld(r, c);
        drawQuad(px, py, tileSize, 0.06f, 0.14f, 0.30f, 0.5f);
      }
    }
  }

  // ── Pass 2: 3D cubes — regular tiles & obstacles ─────────────────────
  glEnable(GL_DEPTH_TEST);

  for (int r = 0; r < board.rows; r++) {
    for (int c = 0; c < board.cols; c++) {
      const Cell &cell = board.grid[r][c];

      if (cell.type == TileType::NONE)
        continue;

      // Map grid (row, col) to 3D world: x = col, z = row
      float worldX = static_cast<float>(c);
      float worldZ = static_cast<float>(r);

      // Apply swap animation offset
      if (swapAnim.active) {
        if (r == swapAnim.r1 && c == swapAnim.c1) {
          worldX += (float)(swapAnim.c2 - swapAnim.c1) * swapAnim.progress;
          worldZ += (float)(swapAnim.r2 - swapAnim.r1) * swapAnim.progress;
        } else if (r == swapAnim.r2 && c == swapAnim.c2) {
          worldX += (float)(swapAnim.c1 - swapAnim.c2) * swapAnim.progress;
          worldZ += (float)(swapAnim.r1 - swapAnim.r2) * swapAnim.progress;
        }
      }

      // Apply gravity/fall animation offset
      if (fallAnim.active) {
        worldZ += fallAnim.offsets[r][c];
      }

      // ── OBSTACLES (3D cubes, same rotation as regular tiles) ────
      if (cell.isObstacle()) {
        unsigned int obsTex = 0;
        if (cell.type == TileType::OBSTACLE_ICE) {
          obsTex = iceObstacleTex;
        } else { // WOOD
          obsTex = (cell.hp >= 2) ? woodFullTex : woodDamagedTex;
        }

        if (obsTex != 0) {
          drawCube(obsTex, worldX, worldZ, 1.0f, 1.0f, 1.0f, 1.0f);
        } else {
          // Fallback flat-color cube
          if (cell.type == TileType::OBSTACLE_ICE)
            drawCube(0, worldX, worldZ, 0.65f, 0.88f, 1.00f, 0.80f);
          else {
            float s = (cell.hp >= 2) ? 1.0f : 0.6f;
            drawCube(0, worldX, worldZ, 0.58f * s, 0.37f * s, 0.12f * s, 1.0f);
          }
        }
        continue;
      }

      // ── REGULAR TILES + SPECIAL TILES ──────────────────────────
      int idx = static_cast<int>(cell.type);
      unsigned int tex = 0;

      bool isSel = (r == selRow && c == selCol);

      if (cell.special == SpecialType::PIRANHA_ROW) {
        float tint = isSel ? 1.3f : 1.0f;
        drawPiranha(worldX, worldZ, true, tint);
        continue;
      } else if (cell.special == SpecialType::PIRANHA_COL) {
        float tint = isSel ? 1.3f : 1.0f;
        drawPiranha(worldX, worldZ, false, tint);
        continue;
      } else if (cell.special == SpecialType::PUFFERFISH) {
        float tint = isSel ? 1.3f : 1.0f;
        drawPufferfish(worldX, worldZ, tint);
        continue;
      } else {
        tex = (idx >= 0 && idx < 4) ? tileTextures[idx] : 0;
      }

      if (tex != 0) {
        float tint = isSel ? 1.3f : 1.0f;
        drawCube(tex, worldX, worldZ, tint, tint, tint, 1.0f);
      } else {
        // Fallback flat-color cube
        float cr = TILE_COLORS[idx][0];
        float cg = TILE_COLORS[idx][1];
        float cb2 = TILE_COLORS[idx][2];
        float br = isSel ? 1.3f : 1.0f;
        drawCube(0, worldX, worldZ, cr * br, cg * br, cb2 * br, 1.0f);
      }
    }
  }

// ── Pass 3: special-tile explosion overlay ───────────────────────────
if (explosionAnim.active) {
  drawExplosionEffect(explosionAnim);
}

// ── Pass 4: 3D selection highlight ───────────────────────────────────
if (selRow >= 0 && selCol >= 0 && selRow < board.rows &&
    selCol < board.cols) {
  const Cell &cell = board.grid[selRow][selCol];
  if (!cell.isObstacle() && cell.type != TileType::NONE) {
    float wx = static_cast<float>(selCol);
    float wz = static_cast<float>(selRow);
    drawSelectionHighlight(wx, wz);
  }
}
}
