// Solar System in OpenGL by Harsh Pandey.
// 1 unit stands for a million kilometers.
// Diameter of Solar System is 287.46 billion kilometers.
// Speed of Light is 1.079 billion Km/h.
// I have Scaled every model to 10 meters of Diameter.

#include "SolarSystem.h"

#include <iostream>
#include <random>

#include "../../vendor/glad/include/glad.h"
#include "../../vendor/glm/gtc/matrix_transform.hpp"
#include "../../vendor/glm/gtc/type_ptr.hpp"

#include "../../vendor/imgui/imgui.h"
#include "../../vendor/imgui/imgui_impl_glfw.h"
#include "../../vendor/imgui/imgui_impl_opengl3.h"

#include <stb_image.h>

using namespace glm;

SolarSystem* SolarSystem::GLFWCallbackWrapper::s_application = nullptr;

void SolarSystem::GLFWCallbackWrapper::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	s_application->KeyCallback(window, key, scancode, action, mods);
}

void SolarSystem::GLFWCallbackWrapper::MouseMoveCallback(GLFWwindow* window, double xpos, double ypos)
{
	s_application->MouseMoveCallback(window, xpos, ypos);
}

void SolarSystem::GLFWCallbackWrapper::MouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	s_application->MouseScrollCallback(window, xoffset, yoffset);
}

void SolarSystem::GLFWCallbackWrapper::WindowResizeCallback(GLFWwindow* window, int width, int height)
{
	s_application->WindowResizeCallback(window, width, height);
}

void SolarSystem::GLFWCallbackWrapper::SetApplication(SolarSystem* application)
{
	GLFWCallbackWrapper::s_application = application;
}

SolarSystem::SolarSystem() : m_Camera(vec3(0.0f, 0.0f, 1.0f)), m_FinalColorBufferTexture(), 
	m_ModelMatrix(mat4(1.0f)), m_ProjectionMatrix(mat4(1.0f))
{

}

SolarSystem::~SolarSystem() { }

void SolarSystem::Simulate()
{
	//If GLFW fails to create a window, then exit the Solar System.
	if (!Init()) return;

	// Main Render Loop.
	RenderLoop();

	// Free the memory allocations.
	Cleanup();
}

bool SolarSystem::Init()
{
	//Initialize GLFW
	glfwInit();

	//Set GLFW Parameters
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	//Create OpenGL Window
	m_Window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Solar System", NULL, NULL);

	// Window NullCheck
	if (!m_Window)
	{
		cout << "Failed To Create OpenGL Window!";
		glfwTerminate();
		return false;
	}

	//Make This Window The Current Context Of OpenGL
	glfwMakeContextCurrent(m_Window);

	// Set Mouse Button Callback
	GLFWCallbackWrapper::SetApplication(this);
	// Set Mouse Move Callback
	glfwSetCursorPosCallback(m_Window, GLFWCallbackWrapper::MouseMoveCallback);
	// Set Mouse Scroll Callback
	glfwSetScrollCallback(m_Window, GLFWCallbackWrapper::MouseScrollCallback);
	// Set Key Callback
	glfwSetKeyCallback(m_Window, GLFWCallbackWrapper::KeyCallback);
	// Set Window Resize Callback
	glfwSetWindowSizeCallback(m_Window, GLFWCallbackWrapper::WindowResizeCallback);

	//Initialize GLAD
	//Because We Can Call gl Functions Only After GLAD is Initialized!
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		// GLAD Failed Initialization.
		cout << "Failed To Initialize GLAD!";
		glfwTerminate();
		return false;
	}

	// Enable Depth Testing & Face Culling.
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDepthFunc(GL_LESS);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// Enable seamless cubemap sampling for lower mip levels in the pre-filter map.
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	//Set Clear Color For Background Color.
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	//Get The FrameBufferSize
	glfwGetFramebufferSize(m_Window, &m_BufferWidth, &m_BufferHeight);

	// Set Window Icon.
	GLFWimage icon[1];
	icon[0].pixels = stbi_load(PROJECT_DIR"/src/Assets/icon.png", &icon[0].width, &icon[0].height, 0, 4);
	glfwSetWindowIcon(m_Window, 1, icon);
	stbi_image_free(icon[0].pixels);

	//Call glViewport To Set Viewport Transform Or The General Area where OpenGL will Render!
	glViewport(0, 0, m_BufferWidth, m_BufferHeight);

	// Enable Vsync
	glfwSwapInterval(1);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	const char* glsl_version = "#version 420";
	ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	#pragma region Geometry Framebuffer

	glGenFramebuffers(1, &m_GBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, m_GBuffer);

	// position color buffer
	glGenTextures(1, &m_GPosition);
	glBindTexture(GL_TEXTURE_2D, m_GPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_BufferWidth, m_BufferHeight, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_GPosition, 0);

	// normal color buffer
	glGenTextures(1, &m_GNormal);
	glBindTexture(GL_TEXTURE_2D, m_GNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_BufferWidth, m_BufferHeight, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_GNormal, 0);

	// Albedo color buffer
	glGenTextures(1, &m_GAlbedo);
	glBindTexture(GL_TEXTURE_2D, m_GAlbedo);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_BufferWidth, m_BufferHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_GAlbedo, 0);

	// Emission color buffer
	glGenTextures(1, &m_GEmission);
	glBindTexture(GL_TEXTURE_2D, m_GEmission);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_BufferWidth, m_BufferHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, m_GEmission, 0);

	// Metallic Roughness color buffer
	glGenTextures(1, &m_GMetallicRoughness);
	glBindTexture(GL_TEXTURE_2D, m_GMetallicRoughness);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, m_BufferWidth, m_BufferHeight, 0, GL_RG, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, m_GMetallicRoughness, 0);

	unsigned int colorAttachments[5] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };
	glDrawBuffers(5, colorAttachments);

	glGenRenderbuffers(1, &m_GDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, m_GDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_BufferWidth, m_BufferHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_GDepth);
	// finally check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		cout << "Geometry buffer not complete!" << endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	#pragma endregion

	#pragma region Bloom Framebuffer

	//Generate Bloom FBO.
	glGenFramebuffers(2, m_BloomFBO);
	glGenTextures(2, m_BloomTexture);
	for (unsigned int i = 0; i < 2; ++i)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[i]);
		glBindTexture(GL_TEXTURE_2D, m_BloomTexture[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_BufferWidth, m_BufferHeight, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_BloomTexture[i], 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cout << "Bloom " + to_string(i) + " Framebuffer not complete!" << std::endl;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	#pragma endregion

	#pragma region Render HDR Framebuffer

	glGenFramebuffers(1, &m_RenderFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, m_RenderFBO);
	// generate texture		
	glGenTextures(2, m_FinalColorBufferTexture);
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindTexture(GL_TEXTURE_2D, m_FinalColorBufferTexture[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_BufferWidth, m_BufferHeight, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		// attach it to currently bound framebuffer object
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, m_FinalColorBufferTexture[i], 0);
	}
	// Tell openGL to render to both color attachments.
	unsigned int drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, drawBuffers);

	glGenRenderbuffers(1, &m_FinalRBO);
	glBindRenderbuffer(GL_RENDERBUFFER, m_FinalRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_BufferWidth, m_BufferHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_FinalRBO);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Render Framebuffer not complete!" << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	#pragma endregion

	#pragma region Resource Initialization

	m_ModelShader.Create(PROJECT_DIR"/src/Shaders/Model.vs", PROJECT_DIR"/src/Shaders/Model.fs");
	m_LightShader.Create(PROJECT_DIR"/src/Shaders/LightShader.vs", PROJECT_DIR"/src/Shaders/LightShader.fs");
	m_BloomShader.Create(PROJECT_DIR"/src/Shaders/blur.vs", PROJECT_DIR"/src/Shaders/blur.fs");
	m_PostProcessingShader.Create(PROJECT_DIR"/src/Shaders/postProcessing.vs", PROJECT_DIR"/src/Shaders/postProcessing.fs");
	m_SkyboxShader.Create(PROJECT_DIR"/src/Shaders/skybox.vs", PROJECT_DIR"/src/Shaders/skybox.fs");

	m_Sun.Create(PROJECT_DIR"/src/Assets/Sun/Sun.gltf");
	m_Mercury.Create(PROJECT_DIR"/src/Assets/Mercury/Mercury.gltf");
	m_Venus.Create(PROJECT_DIR"/src/Assets/Venus/Venus.gltf");
	m_Earth.Create(PROJECT_DIR"/src/Assets/Earth/Earth.gltf");
	m_Mars.Create(PROJECT_DIR"/src/Assets/Mars/Mars.gltf");
	m_Jupiter.Create(PROJECT_DIR"/src/Assets/Jupiter/Jupiter.gltf");
	m_Saturn.Create(PROJECT_DIR"/src/Assets/Saturn/Saturn.gltf");
	m_Uranus.Create(PROJECT_DIR"/src/Assets/Uranus/Uranus.gltf");
	m_Neptune.Create(PROJECT_DIR"/src/Assets/Neptune/Neptune.gltf");
	m_Pluto.Create(PROJECT_DIR"/src/Assets/Pluto/Pluto.gltf");

	//stbi_set_flip_vertically_on_load(true);
	m_SpaceHDRTexture = LoadHDRTexture(PROJECT_DIR"/src/Assets/Space.hdr");

	#pragma endregion

	#pragma region Shader Uniforms

	//Use Shader To Set Uniforms.
	m_LightShader.use();
	m_LightShader.setInt("gPosition", 0);
	m_LightShader.setInt("gNormal", 1);
	m_LightShader.setInt("gAlbedo", 2);
	m_LightShader.setInt("gEmission", 3);
	m_LightShader.setInt("gMetallicRoughness", 4);
	m_LightShader.setInt("irradianceMap", 5);
	m_LightShader.setInt("prefilterMap", 6);
	m_LightShader.setInt("brdfLUT", 7);
	m_LightShader.setFloat("specularStrength", 0.5f);
	
	m_BloomShader.use();
	m_BloomShader.setInt("brightnessTexture", 0);

	m_PostProcessingShader.use();
	m_PostProcessingShader.setInt("screenTexture", 0);
	m_PostProcessingShader.setInt("blurTexture", 1);

	//Material & Lighting Data.
	float ambientStrength  = 0.2f;
	float specularStrength = 0.3f;
	float emissionStrength = 2.0f;

	//Setup PBR Workflow Based on The Environment Map.
	SetupPBR(m_SpaceHDRTexture);

	//Perform Perspective Projection for our Projection Matrix.
	m_ProjectionMatrix = perspective(radians(m_Camera.Zoom), (float)m_BufferWidth / (float)m_BufferHeight, CAM_NEAR_DIST, CAM_FAR_DIST);

	glGenBuffers(1, &m_MatricesUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, m_MatricesUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(mat4), NULL, GL_STATIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, m_MatricesUBO, 0, sizeof(mat4));

	#pragma endregion

	return true;
}

void SolarSystem::RenderLoop()
{
	// For ImGui
	unsigned int toneMapping = 9;
	const char* toneMappings[] = { "Exposure", "Reinhard", "Reinhard 2", "Filmic", "ACES Filmic", "Lottes", "Uchimura", "Uncharted 2", "Unreal", "NONE" };
	static const char* current_toneMapping = "NONE";
	float exposure = 2.5f;

	while (!glfwWindowShouldClose(m_Window))
	{
		//Calculate Delta Time.
		float currentFrame = (float)glfwGetTime();
		m_DeltaTime = currentFrame - m_LastFrame;
		m_LastFrame = currentFrame;

		//To Make Sure Inputs Are Being Read.
		glfwPollEvents();

		//Process Input.
		ProcessInput(m_Window);

		#pragma region Deferred Rendering - Geometry Pass

		//Disable Blending.
		glDisable(GL_BLEND);

		// Bind gBuffer as Current Framebuffer & Draw all The Geomtry & Fill The Samplers.
		glBindFramebuffer(GL_FRAMEBUFFER, m_GBuffer);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		//Get Camera View Matrix.
		mat4 view = m_Camera.GetViewMatrix();

		if (m_CamZoomDirty)
		{
			//Perform Perspective Projection for our Projection Matrix With The New Field of View in Mind.
			m_ProjectionMatrix = perspective(radians(m_Camera.Zoom), (float)m_BufferWidth / (float)m_BufferHeight, CAM_NEAR_DIST, CAM_FAR_DIST);
			m_CamZoomDirty = false;
		}

		//This Matrix Stores The Combined Effort Of Clipping To Camera & Perspective Projection.
		mat4 viewProjection = m_ProjectionMatrix * view;

		//Set Data for UBO.
		glBindBuffer(GL_UNIFORM_BUFFER, m_MatricesUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(mat4), value_ptr(viewProjection));
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		
		glFrontFace(GL_CW);

		m_ModelShader.use();
		m_ModelShader.setFloat("material.emissionStrength", 0.4f);

		#pragma region Draw Sun
		
		// Sun is at the origin and its radius is 696,340 km, our models have a 5 meter radius.
		m_ModelMatrix = mat4(1.0f);
		m_ModelMatrix = scale(m_ModelMatrix, vec3(0.1f));
		m_Sun.Draw(m_ModelShader, m_ModelMatrix);

		#pragma endregion

		#pragma region Draw Mercury

		// Sun is at the origin and its radius is 696,340 km, our models have a 5 meter radius.
		m_ModelMatrix = mat4(1.0f);
		m_ModelMatrix = translate(m_ModelMatrix, vec3(0.0f, 0.0f, -1.0f));
		m_ModelMatrix = scale(m_ModelMatrix, vec3(0.01f));
		m_Mercury.Draw(m_ModelShader, m_ModelMatrix);

		#pragma endregion

		#pragma region Draw Venus

		// Sun is at the origin and its radius is 696,340 km, our models have a 5 meter radius.
		m_ModelMatrix = mat4(1.0f);
		m_ModelMatrix = translate(m_ModelMatrix, vec3(0.0f, 0.0f, 1.0f));
		m_ModelMatrix = scale(m_ModelMatrix, vec3(0.01f));
		m_Venus.Draw(m_ModelShader, m_ModelMatrix);

		#pragma endregion

		#pragma region Draw Earth

		// Sun is at the origin and its radius is 696,340 km, our models have a 5 meter radius.
		m_ModelMatrix = mat4(1.0f);
		m_ModelMatrix = translate(m_ModelMatrix, vec3(0.0f, 0.0f, -2.0f));
		m_ModelMatrix = scale(m_ModelMatrix, vec3(0.01f));
		m_Earth.Draw(m_ModelShader, m_ModelMatrix);

		#pragma endregion

		#pragma region Draw Mars

		// Sun is at the origin and its radius is 696,340 km, our models have a 5 meter radius.
		m_ModelMatrix = mat4(1.0f);
		m_ModelMatrix = translate(m_ModelMatrix, vec3(0.0f, 0.0f, 2.0f));
		m_ModelMatrix = scale(m_ModelMatrix, vec3(0.01f));
		m_Mars.Draw(m_ModelShader, m_ModelMatrix);

		#pragma endregion

		#pragma region Draw Jupiter

		// Sun is at the origin and its radius is 696,340 km, our models have a 5 meter radius.
		m_ModelMatrix = mat4(1.0f);
		m_ModelMatrix = translate(m_ModelMatrix, vec3(0.0f, 0.0f, -3.0f));
		m_ModelMatrix = scale(m_ModelMatrix, vec3(0.01f));
		m_Jupiter.Draw(m_ModelShader, m_ModelMatrix);

		#pragma endregion

		#pragma region Draw Saturn

		// Sun is at the origin and its radius is 696,340 km, our models have a 5 meter radius.
		m_ModelMatrix = mat4(1.0f);
		m_ModelMatrix = translate(m_ModelMatrix, vec3(0.0f, 0.0f, 3.0f));
		m_ModelMatrix = scale(m_ModelMatrix, vec3(0.01f));
		m_Saturn.Draw(m_ModelShader, m_ModelMatrix);

		#pragma endregion

		#pragma region Draw Uranus

		// Sun is at the origin and its radius is 696,340 km, our models have a 5 meter radius.
		m_ModelMatrix = mat4(1.0f);
		m_ModelMatrix = translate(m_ModelMatrix, vec3(0.0f, 0.0f, -4.0f));
		m_ModelMatrix = scale(m_ModelMatrix, vec3(0.01f));
		m_Uranus.Draw(m_ModelShader, m_ModelMatrix);

		#pragma endregion

		#pragma region Draw Neptune

		// Sun is at the origin and its radius is 696,340 km, our models have a 5 meter radius.
		m_ModelMatrix = mat4(1.0f);
		m_ModelMatrix = translate(m_ModelMatrix, vec3(0.0f, 0.0f, 4.0f));
		m_ModelMatrix = scale(m_ModelMatrix, vec3(0.01f));
		m_Neptune.Draw(m_ModelShader, m_ModelMatrix);

		#pragma endregion

		#pragma region Draw Pluto

		// Sun is at the origin and its radius is 696,340 km, our models have a 5 meter radius.
		m_ModelMatrix = mat4(1.0f);
		m_ModelMatrix = translate(m_ModelMatrix, vec3(0.0f, 0.0f, 5.0f));
		m_ModelMatrix = scale(m_ModelMatrix, vec3(0.01f));
		m_Pluto.Draw(m_ModelShader, m_ModelMatrix);

		#pragma endregion

		glFrontFace(GL_CCW);

		#pragma endregion
	
		#pragma region Deferred Rendering - Lighting Pass

		//Calculate Lighting Result Of gBuffer in HDR Render Buffer & Extract Fragment & Brightness Color.
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_RenderFBO);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		m_LightShader.use();

		#pragma region Set Lighting Uniforms

		m_LightShader.setVector3("pointLight.position", 0.0f, 2.0f, 0.0f);
		m_LightShader.setVector3("pointLight.color", 1.0f, 1.0f, 1.0f);
		m_LightShader.setFloat("pointLight.intensity", 50.0f);

		m_LightShader.setFloat("pointLight.linear", 0.000001f);
		m_LightShader.setFloat("pointLight.quadratic", 0.000001f);

		m_LightShader.setVector3("viewPos", m_Camera.Position);

		#pragma endregion

		#pragma region Bind Textures

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_GPosition);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m_GNormal);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, m_GAlbedo);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, m_GEmission);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, m_GMetallicRoughness);
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_IrradianceMap);
		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_PrefilterMap);
		glActiveTexture(GL_TEXTURE7);
		glBindTexture(GL_TEXTURE_2D, m_BrdfLUTTexture);

		#pragma endregion

		RenderQuad();

		#pragma endregion

		#pragma region Bloom Pass

		bool horizontal = true;

		//Copy The Brightness Texture From m_RenderFBO to m_BloomFBO.
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_RenderFBO);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_BloomFBO[0]);

		glReadBuffer(GL_COLOR_ATTACHMENT1);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glBlitFramebuffer(0, 0, m_BufferWidth, m_BufferHeight, 0, 0, m_BufferWidth, m_BufferHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		m_BloomShader.use();
		for (int i = 0; i < 2 * 2; ++i)
		{
			//Bind Bloom FBO for Further Blurring of Brightness Texture.
			glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[horizontal]);
			m_BloomShader.setInt("horizontal", horizontal);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, m_BloomTexture[!horizontal]);
			RenderQuad();
			horizontal = !horizontal;
		}

		#pragma endregion

		#pragma region HDR Render Pass

		//Copy The Depth Buffer From gBuffer To HDR Render Buffer.
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_GBuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_RenderFBO);

		//Copy Depth.
		glReadBuffer(GL_DEPTH_ATTACHMENT);
		glDrawBuffer(GL_DEPTH_ATTACHMENT);
		glBlitFramebuffer(0, 0, m_BufferWidth, m_BufferHeight, 0, 0, m_BufferWidth, m_BufferHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

		//Draw with RenderFBO.
		glBindFramebuffer(GL_FRAMEBUFFER, m_RenderFBO);

		#pragma region Draw Skybox

		glDepthFunc(GL_LEQUAL);
		mat4 skyViewProjection = m_ProjectionMatrix * mat4(mat3(view));
		m_SkyboxShader.use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_EnvCubemap);
		m_SkyboxShader.setMat4("viewProjection", skyViewProjection);
		RenderCube();
		glDepthFunc(GL_LESS);

		#pragma endregion

		#pragma endregion

		#pragma region Draw Screen Quad with Post Processing Shader

		// now bind back to default framebuffer and draw a quad plane with the attached framebuffer color texture
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_FinalColorBufferTexture[0]);	// use the color attachment texture as the texture of the quad plane
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m_BloomTexture[!horizontal]);
		m_PostProcessingShader.use();
		m_PostProcessingShader.setMat4("view", view);
		m_PostProcessingShader.setFloat("exposure", exposure);
		m_PostProcessingShader.setUInt("toneMapping", toneMapping);

		RenderQuad();

		#pragma endregion

		#pragma region Draw ImGui

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Settings");

		// FPS
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

		ImGui::NewLine();

		// Post Processing.
		ImGui::Text("Tone Mapping");
		ImGui::SameLine(210.0f, -1.0f);
		if (ImGui::BeginCombo("##Tone Mapping", current_toneMapping, ImGuiComboFlags_NoArrowButton)) // The second parameter is the label previewed before opening the combo.
		{
			for (int n = 0; n < 10; n++)
			{
				bool is_selected = (current_toneMapping == toneMappings[n]); // You can store your selection however you want, outside or inside your objects
				if (ImGui::Selectable(toneMappings[n], is_selected))
				{
					current_toneMapping = toneMappings[n];
					// Get The Current Tone Mapping.
					toneMapping = n;
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
			}
			ImGui::EndCombo();
		}

		if (toneMapping == 0)
			ImGui::SliderFloat("Exposure", &exposure, 0.0f, 10.0f);

		ImGui::NewLine();

		ImGui::End();

		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(m_Window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		#pragma endregion

		//Swap Buffers.
		glfwSwapBuffers(m_Window);

	}
}

void SolarSystem::Cleanup()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(m_Window);
	glfwTerminate();
}

unsigned int SolarSystem::LoadTexture(char const* path, bool sRGB)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	
	int width, height, nrComponents;
	unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
	if (data)
	{
		GLenum format = GL_RED;
		GLenum internalFormat = GL_RED;
		if (nrComponents == 1)
		{
			format = GL_RED;
			internalFormat = GL_RED;
		}
		else if (nrComponents == 3)
		{
			format = GL_RGB;
			internalFormat = sRGB ? GL_SRGB : GL_RGB;
		}
		else if (nrComponents == 4)
		{
			format = GL_RGBA;
			internalFormat = sRGB ? GL_SRGB_ALPHA : GL_RGBA;
		}
	
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
		stbi_image_free(data);
	}
	else
	{
		std::cout << "Texture failed to load at path: " << path << std::endl;
		stbi_image_free(data);
	}
	
	return textureID;
}

unsigned int SolarSystem::LoadHDRTexture(char const* path)
{
	int width, height, nrComponents;
	float* data = stbi_loadf(path, &width, &height, &nrComponents, 0);
	unsigned int hdrTexture = 0;
	if (data)
	{
		glGenTextures(1, &hdrTexture);
		glBindTexture(GL_TEXTURE_2D, hdrTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data); // note how we specify the texture's data value to be float

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}
	else
	{
		std::cout << "Failed to load HDR image." << std::endl;
	}

	return hdrTexture;
}

void SolarSystem::WindowResizeCallback(GLFWwindow* window, int width, int height)
{
	//Get The Frame Buffer Size.
	glfwGetFramebufferSize(window, &m_BufferWidth, &m_BufferHeight);

	UpdateAllFramebuffersSize(m_BufferWidth, m_BufferHeight);

	//Resize The Viewport.
	glViewport(0, 0, m_BufferWidth, m_BufferHeight);
}

void SolarSystem::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_F && action == GLFW_PRESS && mods != GLFW_MOD_CONTROL)
	{
		// Switch Fullscreen!
		m_FullScreen = !m_FullScreen;

		if (m_FullScreen)
		{
			// Set the window to be in Full screen mode.
			GLFWmonitor* monitor = glfwGetPrimaryMonitor();
			const GLFWvidmode* mode = glfwGetVideoMode(monitor);
			glfwSetWindowMonitor(m_Window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
		}
		else
		{
			// Reset window.
			// Non zero value for offsets to make sure that the title bar can render on windows.
			glfwSetWindowMonitor(m_Window, nullptr, 20, 40, SCR_WIDTH, SCR_HEIGHT, GLFW_DONT_CARE);
		}

		int display_w, display_h;
		glfwGetFramebufferSize(m_Window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		UpdateAllFramebuffersSize(display_w, display_h);
	}
}

void SolarSystem::MouseMoveCallback(GLFWwindow* window, double xpos, double ypos)
{
	if (m_FirstMouse)
	{
		m_LastX = (float)xpos;
		m_LastY = (float)ypos;
		m_FirstMouse = false;
	}

	float xoffset = (float)xpos - m_LastX;
	float yoffset = m_LastY - (float)ypos; // reversed since y-coordinates go from bottom to top

	m_LastX = (float)xpos;
	m_LastY = (float)ypos;

	m_Camera.ProcessMouseMovement(xoffset, yoffset);
}

void SolarSystem::MouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	//Handle Zooming.
	m_Camera.ProcessMouseScroll((float)yoffset);
	m_CamZoomDirty = true;
}

void SolarSystem::ProcessInput(GLFWwindow* window)
{
	//Tell GLFW to capture our mouse only if we held down the Right Mouse Button.
	if (glfwGetMouseButton(window, 1))
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		m_Camera.updateRotation = true;
	}
	else
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		m_FirstMouse = true;
		m_Camera.updateRotation = false;
	}

	//Close Window On Pressing Escape Key.
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	//Perform Camera Input.
	bool forwardPressed = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
	bool backwardPressed = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
	bool leftwardPressed = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
	bool rightwardPressed = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
	bool upwardPressed = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
	bool downwardPressed = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
	bool hasCameraMovementInput = forwardPressed || backwardPressed || leftwardPressed ||
		rightwardPressed || upwardPressed || downwardPressed;

	if (forwardPressed)
		m_Camera.ProcessKeyboard(FORWARD, m_DeltaTime);
	if (backwardPressed)
		m_Camera.ProcessKeyboard(BACKWARD, m_DeltaTime);
	if (leftwardPressed)
		m_Camera.ProcessKeyboard(LEFT, m_DeltaTime);
	if (rightwardPressed)
		m_Camera.ProcessKeyboard(RIGHT, m_DeltaTime);
	if (upwardPressed)
		m_Camera.ProcessKeyboard(UP, m_DeltaTime);
	if (downwardPressed)
		m_Camera.ProcessKeyboard(DOWN, m_DeltaTime);
	if (glfwGetKey(window, GLFW_KEY_KP_ADD))
		m_Camera.Zoom -= 50.0f * m_DeltaTime;
	else if (glfwGetKey(window, GLFW_KEY_KP_SUBTRACT))
		m_Camera.Zoom += 50.0f * m_DeltaTime;
	m_Camera.Zoom = std::clamp(m_Camera.Zoom, 1.0f, 45.0f);
}

void SolarSystem::UpdateAllFramebuffersSize(int bufferWidth, int bufferHeight)
{
	#pragma region Resize HDR Render Buffer

	glBindFramebuffer(GL_FRAMEBUFFER, m_RenderFBO);
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindTexture(GL_TEXTURE_2D, m_FinalColorBufferTexture[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bufferWidth, bufferHeight, 0, GL_RGBA, GL_FLOAT, NULL);
		// attach it to currently bound framebuffer object
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, m_FinalColorBufferTexture[i], 0);
	}
	glBindRenderbuffer(GL_RENDERBUFFER, m_FinalRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, bufferWidth, bufferHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_FinalRBO);
	// tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
	unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, attachments);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	#pragma endregion

	#pragma region Resize Geometry Buffer

	glBindFramebuffer(GL_FRAMEBUFFER, m_GBuffer);

	// position color buffer
	glBindTexture(GL_TEXTURE_2D, m_GPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, bufferWidth, bufferHeight, 0, GL_RGB, GL_FLOAT, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_GPosition, 0);

	// normal color buffer
	glBindTexture(GL_TEXTURE_2D, m_GNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, bufferWidth, bufferHeight, 0, GL_RGB, GL_FLOAT, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_GNormal, 0);

	// Albedo color buffer
	glBindTexture(GL_TEXTURE_2D, m_GAlbedo);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, bufferWidth, bufferHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_GAlbedo, 0);

	// Emission color buffer
	glBindTexture(GL_TEXTURE_2D, m_GEmission);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, bufferWidth, bufferHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, m_GEmission, 0);

	// Metallic Roughness color buffer
	glBindTexture(GL_TEXTURE_2D, m_GMetallicRoughness);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, bufferWidth, bufferHeight, 0, GL_RG, GL_UNSIGNED_BYTE, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, m_GMetallicRoughness, 0);

	unsigned int colorAttachments[5] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };
	glDrawBuffers(5, colorAttachments);

	// gDepth Buffer
	glBindRenderbuffer(GL_RENDERBUFFER, m_GDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, bufferWidth, bufferHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_GDepth);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	#pragma endregion

	#pragma region Resize Bloom Buffer

	for (unsigned int i = 0; i < 2; ++i)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[i]);
		glBindTexture(GL_TEXTURE_2D, m_BloomTexture[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bufferWidth, bufferHeight, 0, GL_RGBA, GL_FLOAT, NULL);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_BloomTexture[i], 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	#pragma endregion

}

void SolarSystem::RenderQuad()
{
	if (m_QuadVAO == 0)
	{
		float quadVertices[] = {
			// positions        // texture Coords
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};
		// setup plane VAO
		glGenVertexArrays(1, &m_QuadVAO);
		glGenBuffers(1, &m_QuadVBO);
		glBindVertexArray(m_QuadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}

	glBindVertexArray(m_QuadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

void SolarSystem::RenderCube()
{
	// initialize (if necessary)
	if (m_CubeVAO == 0)
	{
		float vertices[] = {
			// back face
			-1.0f, -1.0f, -1.0f, // bottom-left
			 1.0f,  1.0f, -1.0f, // top-right
			 1.0f, -1.0f, -1.0f, // bottom-right         
			 1.0f,  1.0f, -1.0f, // top-right
			-1.0f, -1.0f, -1.0f, // bottom-left
			-1.0f,  1.0f, -1.0f, // top-left
			// front face		 
			-1.0f, -1.0f,  1.0f, // bottom-left
			 1.0f, -1.0f,  1.0f, // bottom-right
			 1.0f,  1.0f,  1.0f, // top-right
			 1.0f,  1.0f,  1.0f, // top-right
			-1.0f,  1.0f,  1.0f, // top-left
			-1.0f, -1.0f,  1.0f, // bottom-left
			// left face		 
			-1.0f,  1.0f,  1.0f, // top-right
			-1.0f,  1.0f, -1.0f, // top-left
			-1.0f, -1.0f, -1.0f, // bottom-left
			-1.0f, -1.0f, -1.0f, // bottom-left
			-1.0f, -1.0f,  1.0f, // bottom-right
			-1.0f,  1.0f,  1.0f, // top-right
			// right face		 
			 1.0f,  1.0f,  1.0f, // top-left
			 1.0f, -1.0f, -1.0f, // bottom-right
			 1.0f,  1.0f, -1.0f, // top-right         
			 1.0f, -1.0f, -1.0f, // bottom-right
			 1.0f,  1.0f,  1.0f, // top-left
			 1.0f, -1.0f,  1.0f, // bottom-left     
			 // bottom face		 
			 -1.0f, -1.0f, -1.0f, // top-right
			  1.0f, -1.0f, -1.0f, // top-left
			  1.0f, -1.0f,  1.0f, // bottom-left
			  1.0f, -1.0f,  1.0f, // bottom-left
			 -1.0f, -1.0f,  1.0f, // bottom-right
			 -1.0f, -1.0f, -1.0f, // top-right
			 // top face			 
			 -1.0f,  1.0f, -1.0f, // top-left
			  1.0f,  1.0f , 1.0f, // bottom-right
			  1.0f,  1.0f, -1.0f, // top-right     
			  1.0f,  1.0f,  1.0f, // bottom-right
			 -1.0f,  1.0f, -1.0f, // top-left
			 -1.0f,  1.0f,  1.0f  // bottom-left        
		};
		glGenVertexArrays(1, &m_CubeVAO);
		glGenBuffers(1, &m_CubeVBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, m_CubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		// link vertex attributes
		glBindVertexArray(m_CubeVAO);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	// Render Cube
	glBindVertexArray(m_CubeVAO);
	glFrontFace(GL_CW);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glFrontFace(GL_CCW);
	glBindVertexArray(0);
}

void SolarSystem::SetupPBR(unsigned int hdrTexture)
{
	// Setup framebuffer
	// ----------------------
	if (!m_PbrInitialized)
	{
		glGenFramebuffers(1, &m_CaptureFBO);
		glGenRenderbuffers(1, &m_CaptureRBO);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, m_CaptureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 1024, 1024);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_CaptureRBO);

	// Setup cubemap to render to and attach to framebuffer
	// ---------------------------------------------------------
	if (!m_PbrInitialized)	glGenTextures(1, &m_EnvCubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_EnvCubemap);
	if (!m_PbrInitialized)
	{
		for (unsigned int i = 0; i < 6; ++i)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 1024, 1024, 0, GL_RGB, GL_FLOAT, nullptr);
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // enable pre-filter mipmap sampling (combatting visible dots artifact)
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	// pbr: set up projection and view matrices for capturing data onto the 6 cubemap face directions
	// ----------------------------------------------------------------------------------------------
	mat4 captureProjection = perspective(radians(90.0f), 1.0f, 0.1f, 10.0f);
	mat4 captureViews[] =
	{
		lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(1.0f,  0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f)),
		lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(-1.0f,  0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f)),
		lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f,  1.0f,  0.0f), vec3(0.0f,  0.0f,  1.0f)),
		lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, -1.0f,  0.0f), vec3(0.0f,  0.0f, -1.0f)),
		lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f,  0.0f,  1.0f), vec3(0.0f, -1.0f,  0.0f)),
		lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f,  0.0f, -1.0f), vec3(0.0f, -1.0f,  0.0f))
	};

	// pbr: convert HDR equirectangular environment map to cubemap equivalent
	// ----------------------------------------------------------------------
	Shader equirectangularToCubemapShader(PROJECT_DIR"/src/Shaders/cubemap.vs", PROJECT_DIR"/src/Shaders/equirectangular_to_cubemap.fs");
	equirectangularToCubemapShader.use();
	equirectangularToCubemapShader.setInt("equirectangularMap", 0);
	equirectangularToCubemapShader.setMat4("projection", captureProjection);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdrTexture);

	glViewport(0, 0, 1024, 1024); // don't forget to configure the viewport to the capture dimensions.
	glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
	for (unsigned int i = 0; i < 6; ++i)
	{
		equirectangularToCubemapShader.setMat4("view", captureViews[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_EnvCubemap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		RenderCube();
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Then let OpenGL generate mipmaps from first mip face (combatting visible dots artifact)
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_EnvCubemap);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	// Create an irradiance cubemap, and re-scale capture FBO to irradiance scale.
	// --------------------------------------------------------------------------------
	if (!m_PbrInitialized)	glGenTextures(1, &m_IrradianceMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_IrradianceMap);
	if (!m_PbrInitialized)
	{
		for (unsigned int i = 0; i < 6; ++i)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 64, 64, 0, GL_RGB, GL_FLOAT, nullptr);
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	}
	glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, m_CaptureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 64, 64);

	// pbr: solve diffuse integral by convolution to create an irradiance (cube)map.
	// -----------------------------------------------------------------------------
	Shader irradianceShader(PROJECT_DIR"/src/Shaders/cubemap.vs", PROJECT_DIR"/src/Shaders/irradiance_convolution.fs");
	irradianceShader.use();
	irradianceShader.setInt("environmentMap", 0);
	irradianceShader.setMat4("projection", captureProjection);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_EnvCubemap);

	glViewport(0, 0, 64, 64); // don't forget to configure the viewport to the capture dimensions.
	glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
	for (unsigned int i = 0; i < 6; ++i)
	{
		irradianceShader.setMat4("view", captureViews[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_IrradianceMap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		RenderCube();
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Create a pre-filter cubemap, and re-scale capture FBO to pre-filter scale.
	// --------------------------------------------------------------------------------
	if (!m_PbrInitialized)	glGenTextures(1, &m_PrefilterMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_PrefilterMap);
	if (!m_PbrInitialized)
	{
		for (unsigned int i = 0; i < 6; ++i)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 256, 256, 0, GL_RGB, GL_FLOAT, nullptr);
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // be sure to set minification filter to mip_linear 
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	// Generate mipmaps for the cubemap so OpenGL automatically allocates the required memory.
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	// Run a quasi monte-carlo simulation on the environment lighting to create a prefilter (cube)map.
	// ----------------------------------------------------------------------------------------------------
	Shader prefilterShader(PROJECT_DIR"/src/Shaders/cubemap.vs", PROJECT_DIR"/src/Shaders/prefilter.fs");
	prefilterShader.use();
	prefilterShader.setInt("environmentMap", 0);
	prefilterShader.setMat4("projection", captureProjection);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_EnvCubemap);

	glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
	unsigned int maxMipLevels = 5;
	for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
	{
		// reisze framebuffer according to mip-level size.
		unsigned int mipWidth = static_cast<unsigned int>(256 * std::pow(0.5, mip));
		unsigned int mipHeight = static_cast<unsigned int>(256 * std::pow(0.5, mip));
		glBindRenderbuffer(GL_RENDERBUFFER, m_CaptureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
		glViewport(0, 0, mipWidth, mipHeight);

		float roughness = (float)mip / (float)(maxMipLevels - 1);
		prefilterShader.setFloat("roughness", roughness);
		for (unsigned int i = 0; i < 6; ++i)
		{
			prefilterShader.setMat4("view", captureViews[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_PrefilterMap, mip);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			RenderCube();
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Generate a 2D LUT from the BRDF equations used.
	// ----------------------------------------------------
	if (!m_PbrInitialized)	glGenTextures(1, &m_BrdfLUTTexture);
	Shader brdfShader(PROJECT_DIR"/src/Shaders/brdf.vs", PROJECT_DIR"/src/Shaders/brdf.fs");

	// pre-allocate enough memory for the LUT texture.
	glBindTexture(GL_TEXTURE_2D, m_BrdfLUTTexture);
	if (!m_PbrInitialized)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 1024, 1024, 0, GL_RG, GL_FLOAT, 0);
		// be sure to set wrapping mode to GL_CLAMP_TO_EDGE
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	// then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
	glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, m_CaptureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 1024, 1024);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_BrdfLUTTexture, 0);

	glViewport(0, 0, 1024, 1024);
	brdfShader.use();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	RenderQuad();

	//Reset Viewport Size.
	glViewport(0, 0, m_BufferWidth, m_BufferHeight);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//Set PBR Initialized as True.
	m_PbrInitialized = true;
}

int main()
{
	SolarSystem* solarSystem = new SolarSystem();
	solarSystem->Simulate();
	delete solarSystem;

	return 0;
}