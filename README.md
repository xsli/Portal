# 非欧几里得空间传送门 (Portal) 渲染系统

一个基于 OpenGL 的 Portal 传送门渲染演示项目，实现了类似《Portal》游戏中的空间传送效果。

## ✅ 已实现功能

- **递归门户渲染**：通过模板缓冲实现最多4层递归的"门中看门"透视效果
- **无缝传送**：玩家穿过门户时自动传送到对应位置和朝向
- **虚拟相机计算**：正确计算穿过门户后的视角
- **双面门户支持**：从门户背面看时显示不透明背板
- **动态场景**：两个独立区域（蓝色房间和橙色房间）通过门户连接

## 📋 项目概述

本项目实现了一个完整的 Portal 传送系统，包括：

- **Portal 数学变换**：计算穿过门户时的位置和方向变换
- **Portal 渲染器**：使用 OpenGL 渲染门户框架和动画效果
- **Portal 传送器**：检测玩家穿越门户并执行传送
- **场景系统**：包含地板、墙壁、装饰物的测试场景

## 🛠️ 技术栈

- **图形 API**: OpenGL 3.3 Core Profile
- **窗口管理**: GLFW 3.3
- **OpenGL 扩展**: GLEW
- **数学库**: GLM (OpenGL Mathematics)
- **构建系统**: CMake 3.14+

## 📁 项目结构

```
d:\CC\portal\
├── CMakeLists.txt          # CMake 构建配置
├── README.md               # 项目文档
├── PortalMath.h            # 门户数学变换库
├── PortalRenderer.h        # 门户渲染器
├── PortalTeleporter.h      # 传送逻辑处理
└── main_example.cpp        # 主程序入口和场景定义
```

## 🔧 核心模块

### 1. PortalMath.h - 门户数学变换

提供门户传送所需的核心数学计算：

```cpp
namespace PortalMath {
    // 计算从入口门户到出口门户的变换矩阵
    glm::mat4 ComputePortalTransform(const glm::mat4& srcPortal, const glm::mat4& dstPortal);
    
    // 传送位置点
    glm::vec3 TeleportPoint(const glm::vec3& point, const glm::mat4& srcPortal, const glm::mat4& dstPortal);
    
    // 传送方向向量
    glm::vec3 TeleportDirection(const glm::vec3& direction, const glm::mat4& srcPortal, const glm::mat4& dstPortal);
    
    // 计算透过门户观察时的虚拟相机矩阵
    glm::mat4 ComputePortalViewMatrix(const glm::mat4& viewMatrix, const glm::mat4& srcPortal, const glm::mat4& dstPortal);
}
```

**关键算法**：
- 使用 180° Y轴旋转实现门户"穿过"效果
- 通过矩阵链 `dstPortal * rotation180 * inverse(srcPortal)` 计算完整变换

### 2. PortalRenderer.h - 门户渲染器

管理门户的可视化渲染：

```cpp
namespace PortalRenderer {
    struct Portal {
        glm::mat4 transform;        // 门户变换矩阵
        Portal* linkedPortal;       // 链接的目标门户
        float width, height;        // 门户尺寸
        bool isActive;              // 是否激活
        
        // OpenGL 资源
        GLuint VAO, VBO;            // 网格数据
        GLuint FBO, colorTexture, depthTexture;  // 渲染目标
        GLuint shaderProgram;       // 着色器
    };
    
    void CreatePortalMesh(Portal* portal);
    void CreatePortalRenderTarget(Portal* portal, int width, int height);
    GLuint CompilePortalShader();
}
```

### 3. PortalTeleporter.h - 传送逻辑

处理玩家穿越门户的检测和传送：

```cpp
namespace PortalTeleporter {
    struct TeleportableEntity {
        glm::vec3 position;
        glm::vec3 previousPosition;
        glm::vec3 velocity;
        glm::mat4 transform;
        float lastTeleportTime;
    };
    
    // 检测是否应该触发传送
    bool ShouldTeleport(TeleportableEntity& entity, PortalRenderer::Portal* portal,
                        float portalHalfWidth, float portalHalfHeight, float currentTime);
    
    // 执行传送
    void TeleportEntity(TeleportableEntity& entity, 
                        PortalRenderer::Portal* srcPortal, 
                        PortalRenderer::Portal* dstPortal);
}
```

**传送触发条件**：
1. 实体在门户平面的边界范围内
2. 上一帧在门户正面（signed distance > 0）
3. 当前帧在门户背面（signed distance ≤ 0）
4. 距离上次传送超过 0.5 秒（防止连续触发）

### 4. main_example.cpp - 主程序

实现完整的演示场景：

#### 场景几何体
- **地板**：100×100 单位的棋盘格图案（法线朝上）
- **墙壁**：双面渲染，确保从任意角度可见
- **装饰物**：彩色箱子和柱子，分布在两个房间

#### 门户配置
| 门户 | 位置 | 朝向 | 颜色 |
|------|------|------|------|
| Portal A | (-5, 1.5, 0) | +Z（朝向玩家） | 蓝色 |
| Portal B | (5, 1.5, -10) | -X（旋转90°） | 橙色 |

#### 门户视觉效果
- **框架**：3D 箱体构成的门框
- **正面**：带动画的半透明发光效果（漩涡 + 涟漪）
- **背面**：不透明深灰色遮挡板

## 🎮 操作控制

| 按键 | 功能 |
|------|------|
| W | 向前移动 |
| S | 向后移动 |
| A | 向左移动 |
| D | 向右移动 |
| 鼠标移动 | 调整视角 |
| ESC | 退出程序 |

## 🔨 编译构建

### 前置要求
- CMake 3.14 或更高版本
- 支持 C++17 的编译器（MSVC 2019+、GCC 8+、Clang 7+）
- Windows/Linux/macOS

### 构建步骤

```bash
# 创建构建目录
mkdir build
cd build

# 生成项目
cmake ..

# 编译 (Windows)
cmake --build . --config Release

# 运行
./Release/PortalDemo.exe   # Windows
./PortalDemo               # Linux/macOS
```

### 依赖管理

所有依赖通过 CMake FetchContent 自动下载：
- GLFW 3.3.8
- GLEW 2.2.0  
- GLM 0.9.9.8

## 📐 关键算法详解

### 1. 门户变换矩阵

穿过门户时，位置和方向需要通过以下变换：

```
Transform = DstPortal × Rotation180° × Inverse(SrcPortal)
```

这个变换将：
1. 把世界坐标转换到入口门户的局部空间
2. 绕 Y 轴旋转 180°（模拟"穿过"效果）
3. 从出口门户的局部空间转换回世界坐标

### 2. 相机朝向重建

传送后需要重新计算相机的 Yaw 和 Pitch：

```cpp
// 计算当前视线方向
glm::vec3 currentForward = GetCameraForward(yaw, pitch);

// 通过门户变换方向
glm::vec3 newForward = PortalMath::TeleportDirection(currentForward, src, dst);

// 从方向向量重建欧拉角
newYaw = atan2(newForward.z, newForward.x);
newPitch = asin(clamp(newForward.y, -1, 1));
```

### 3. 有符号距离检测

判断实体是否穿过门户平面：

```cpp
// 门户局部空间中的 Z 坐标即为有符号距离
// Z > 0：在门户正面
// Z ≤ 0：在门户背面
float signedDist = LocalPosition.z;

// 从正面穿到背面时触发传送
if (prevSignedDist > 0 && currSignedDist <= 0) {
    Teleport();
}
```

### 4. 顶点绕序与法线

地板使用逆时针顶点顺序确保法线朝上：

```cpp
// 从上往下看，逆时针顺序 → 法线朝 +Y
AddQuad(verts, p0, p3, p2, p1, color);

// 顺时针顺序 → 法线朝 -Y（会被背面剔除）
// AddQuad(verts, p0, p1, p2, p3, color);  // 错误
```

## 🎨 着色器

### 场景着色器
简单的顶点颜色渲染：

```glsl
// Vertex Shader
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uMVP;
out vec3 vColor;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vColor = aColor;
}

// Fragment Shader
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
```

### 门户表面着色器
带动画效果的半透明渲染：

```glsl
// Fragment Shader 核心逻辑
float dist = length(uv);
float angle = atan(uv.y, uv.x);

// 涟漪效果
float ripple = sin(dist * 8.0 - uTime * 3.0) * 0.5 + 0.5;

// 漩涡效果
float swirl = sin(angle * 3.0 + uTime * 2.0 + dist * 4.0) * 0.5 + 0.5;

// 组合颜色
vec3 color = uPortalColor * (0.5 + 0.3 * ripple + 0.2 * swirl);
```

## 🚀 扩展方向

1. ~~**递归门户渲染**~~：✅ 已实现 - 通过模板缓冲实现真正的"透视"效果
2. **斜裁剪投影**：优化门户内渲染，避免看到门户后面的物体
3. **物理系统**：添加速度传递和重力
4. **多门户支持**：支持超过两个门户的网络
5. **碰撞检测**：防止穿墙和与物体重叠
6. **音效系统**：传送时的音效反馈

## 📝 已知问题

- C4819 警告：源文件包含中文注释，在非 Unicode 代码页下可能显示警告（不影响编译）
- ~~当前门户表面是静态动画，未实现真正的递归渲染~~ ✅ 已修复

## 📄 许可证

本项目仅供学习和演示用途。

---

*最后更新: 2026-02-03*