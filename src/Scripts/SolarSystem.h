#pragma once

#include "Camera.h"
#include "Shader.h"
#include "Model.h"
#include "../../vendor/glfw/include/GLFW/glfw3.h"
#include "../../vendor/glm/glm.hpp"

class SolarSystem
{
public:
	SolarSystem();
	~SolarSystem();
	void Simulate();
private:
	/// @brief Used to get callbacks from GLFW which expects static functions.
	class GLFWCallbackWrapper
	{
	public:
		GLFWCallbackWrapper() = delete;
		~GLFWCallbackWrapper() = delete;

		static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
		static void MouseMoveCallback(GLFWwindow* window, double xpos, double ypos);
		static void MouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
		static void WindowResizeCallback(GLFWwindow* window, int width, int height);
		static void SetApplication(SolarSystem* application);
	private:
		static SolarSystem* s_application;
	};
private:
	bool Init();
	void RenderLoop();
	void Cleanup();

	static unsigned int LoadTexture(char const* path, bool sRGB = false);
	static unsigned int LoadHDRTexture(char const* path);

	void WindowResizeCallback(GLFWwindow* window, int width, int height);
	void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	void MouseMoveCallback(GLFWwindow* window, double xpos, double ypos);
	void MouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

	void ProcessInput(GLFWwindow* window);

	void UpdateAllFramebuffersSize(int bufferWidth, int bufferHeight);

	void RenderQuad();
	void RenderCube();
	void SetupPBR(unsigned int hdrTexture);

	void SetCustomImGuiStyle();
private:
	///<summary>Screen Width in Screen Coordinates.</summary>
	unsigned const int SCR_WIDTH = 1280;
	///<summary>Screen Height in Screen Coordinates.</summary>
	unsigned const int SCR_HEIGHT = 720;

	///<summary>Number of Samples For Multisampling.</summary>
	unsigned const int SAMPLES = 4;

	///<summary>Camera Near Plane Distance.</summary>
	float m_NearPlane = 0.01f; // 1 km
	///<summary>Camera Far Plane Distance.</summary>
	float m_FarPlane = 1000.0f; // 1 million km

	///<summary>Buffer width of the window incase the Screen Width is not in Screen Coordinates.</summary>
	int m_BufferWidth = 0;
	///<summary>Buffer height of the window incase the Screen Height is not in Screen Coordinates.</summary>
	int m_BufferHeight = 0;

	///<summary>Tells Us If We Are in Full Screen Mode Or Not.</summary>
	bool m_FullScreen = false;

	///<summary>Fly Camera</summary>
	Camera m_Camera;

	/// <summary> GLFW Window </summary>
	GLFWwindow* m_Window = nullptr;

	///<summary>Last Mouse X Position.</summary>
	float m_LastX = SCR_WIDTH / 2.0f;
	///<summary>Last Mouse Y Position.</summary>
	float m_LastY = SCR_HEIGHT / 2.0f;
	///<summary>Is This Our First Mouse Input?</summary>
	bool m_FirstMouse = true;
	/// <summary>True if Camera Zoom Changed This Frame.</summary>
	bool m_CamZoomDirty = false;
	///<summary>The Time Elapsed Since Last Frame Was Rendered.</summary>
	float m_DeltaTime = 0.0f;
	///<summary>The Time At Which Last Frame Was Rendered.</summary>
	float m_LastFrame = 0.0f;

	//RenderCube() VAO & VBO.
	unsigned int m_CubeVAO = 0;
	unsigned int m_CubeVBO = 0;

	//RenderQuad() VAO & VBO.
	unsigned int m_QuadVAO = 0;
	unsigned int m_QuadVBO = 0;

	// Shaders
	Shader m_ModelShader, m_LightShader, m_PostProcessingShader, m_SkyboxShader, m_BloomShader;

	// Models
	Model m_Sun, m_Mercury, m_Venus, m_Earth, m_Mars, m_Jupiter, m_Saturn, m_Uranus, m_Neptune, m_Pluto;

	// Skybox Texture
	unsigned int m_SpaceHDRTexture;

	glm::mat4 m_ModelMatrix, m_ProjectionMatrix;

	unsigned int m_MatricesUBO;

	//Bloom Ping-Pong Buffers
	unsigned int m_BloomFBO[2];
	unsigned int m_BloomTexture[2];

	//HDR Render Buffer
	unsigned int m_RenderFBO = 0;
	unsigned int m_FinalColorBufferTexture[2];
	unsigned int m_FinalRBO = 0;

	//Geometry Buffer
	unsigned int m_GBuffer = 0;
	unsigned int m_GPosition = 0, m_GNormal = 0, m_GAlbedo = 0, m_GEmission = 0, m_GMetallicRoughness = 0;
	unsigned int m_GDepth = 0;

	//PBR Image Based Lighting
	bool m_PbrInitialized = false;	//True if PBR has been Initialized atleast once.
	unsigned int m_EnvCubemap = 0;		//Enivornment Cubemap Generated From Equirectangular Map(HDR Map).
	unsigned int m_IrradianceMap = 0;		//Irradiance Cubemap
	unsigned int m_PrefilterMap = 0;		//Prefilter Cubemap
	unsigned int m_BrdfLUTTexture = 0;	//2D LUT Generated from the BRDF equations.
	unsigned int m_CaptureFBO = 0;		//PBR FBO
	unsigned int m_CaptureRBO = 0;		//PBR RBO
};