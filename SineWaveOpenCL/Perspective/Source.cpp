#include<windows.h>
#include<stdio.h>
#include<stdlib.h>

#include<gl/glew.h>
#include<gl/GL.h>
#include<CL/opencl.h>

#include "vmath.h"
#include "resource.h"

using namespace vmath;

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "OpenCL.lib")

#define WINWIDTH 800
#define WINHEIGHT 600
#define MY_ARRAY_SIZE meshWidth * meshHeight * 4;
enum
{
	RRB_ATTRIBUTE_POSITION = 0,
	RRB_ATTRIBUTE_COLOR, 
	RRB_ATTRIBUTE_NORMAL,
	RRB_ATTRIBUTE_TEXTURE0
};

//Callback
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

//Log File 
FILE* gpLogFile = NULL;
HWND ghwnd;
HDC ghdc = NULL;
HGLRC ghrc = NULL;
DWORD dwStyle;

WINDOWPLACEMENT wpPrev = { sizeof(WINDOWPLACEMENT) };

bool gbActiveWindow = false;
bool gbFullscreen = false;
bool gbEscapeKeyPressed = false;

GLuint gVertexShaderObject;
GLuint gFragmentShaderObject;
GLuint gShaderProgramObject;

GLuint gVao;
GLuint gVbo_Position;
GLuint gVbo_GPU;

GLuint gMVPMatrixUniform;

//matrix 
mat4 gPerspectiveProjectionMatrix;

//Sinewave variable
const unsigned int meshWidth = 1024;
const unsigned int meshHeight = 1024;

GLfloat position[meshWidth][meshHeight][4];
float animationTime = 0.0f;

//OpenCL Variables
cl_int clResult;
cl_mem clGraphicsResource;
cl_device_id oclDeviceID;
cl_context oclContext;
cl_command_queue oclCommandQueue;
cl_program oclProgram;
cl_kernel oclKernel;

char* oclKernelSource;
size_t oclKernelSize;
bool bOnGPU = false;

//main
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstace, LPSTR lpszCmdLine, int iCmdShow)
{
	//functions declarations
	void initialize(void);
	void uninitialize(void);
	void display();

	bool bDone = false;
	HWND hwnd;
	MSG msg;
	TCHAR szAppName[] = TEXT("PP_Perspective");
	WNDCLASSEX wndclass;

	if (fopen_s(&gpLogFile, "Application.log", "w"))
	{
		MessageBox(NULL, TEXT("Unable to create a log file, exiting..."), TEXT("Error"), MB_OK | MB_ICONERROR);
		exit(1);
	}
	else
	{
		fprintf(gpLogFile, "Log file created successfully, program execution starts...\n\n");
	}


	wndclass.cbSize = sizeof(wndclass);
	wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = hInstance;
	wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndclass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(RRB_ICON));
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(RRB_ICON));
	wndclass.lpfnWndProc = WndProc;
	wndclass.lpszClassName = szAppName;
	wndclass.lpszMenuName = NULL;

	//Registering class
	RegisterClassEx(&wndclass);

	///CreateWindow
	hwnd = CreateWindowEx(WS_EX_APPWINDOW,
		szAppName,
		TEXT("RRB: PP ORTHO"),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | CS_OWNDC,
		100,
		100,
		WINWIDTH,
		WINHEIGHT,
		NULL,
		NULL,
		hInstance,
		NULL);

	ghwnd = hwnd;

	ShowWindow(ghwnd, iCmdShow);
	SetForegroundWindow(ghwnd);
	SetFocus(ghwnd);

	initialize();

	while (!bDone)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				bDone = true;
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			if (gbActiveWindow)
			{
				display();
				if (gbEscapeKeyPressed)
				{
					bDone = true;
				}
			}
		}
	}

	uninitialize();
	return((int)msg.lParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	//functions
	void resize(int, int);
	void toggleFullscreen();
	void uninitialize();

	//code
	switch (iMsg)
	{
	case WM_SETFOCUS:
		gbActiveWindow = true;
		break;
	case WM_KILLFOCUS:
		gbActiveWindow = false;
		break;

	case WM_ACTIVATE:
		if (HIWORD(wParam) == 0)
			gbActiveWindow = true;
		else
			gbActiveWindow = false;
		break;

	case WM_ERASEBKGND:
		return (0);
	case WM_SIZE:
		resize(LOWORD(lParam), HIWORD(lParam));
		break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_ESCAPE:
			gbEscapeKeyPressed = true;
			break;
		case 0x46:
		case 0x66:
			toggleFullscreen();
			break;
		default:
			break;
		}
		break;
	case WM_CHAR:
		switch (wParam)
		{
		case 'c':
		case 'C':
			bOnGPU = false;
			break;
		case 'g':
		case 'G':
			bOnGPU = true;
			break;				 
		default:
			break;
		}
		break;
	case WM_LBUTTONDOWN:
		break;
	case WM_CLOSE:
		uninitialize();
		break;
	case WM_DESTROY:
		uninitialize();
		PostQuitMessage(0);
		break;
	default:
		break;
	}

	return (DefWindowProc(hwnd, iMsg, wParam, lParam));
}

void toggleFullscreen()
{
	//code
	MONITORINFO mi = { sizeof(MONITORINFO) };

	if (gbFullscreen == FALSE)
	{
		dwStyle = GetWindowLong(ghwnd, GWL_STYLE);
		if (dwStyle & WS_OVERLAPPEDWINDOW)
		{
			if (GetWindowPlacement(ghwnd, &wpPrev) &&
				GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &mi))
			{
				SetWindowLong(ghwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);

				SetWindowPos(ghwnd,
					HWND_TOP,
					mi.rcMonitor.left,
					mi.rcMonitor.top,
					mi.rcMonitor.right - mi.rcMonitor.left,
					mi.rcMonitor.bottom - mi.rcMonitor.top,
					SWP_NOZORDER | SWP_FRAMECHANGED);
			}
		}
		ShowCursor(FALSE);
		gbFullscreen = true;
	}
	else
	{
		SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(ghwnd, &wpPrev);
		SetWindowPos(ghwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOSIZE);
		ShowCursor(TRUE);
		gbFullscreen = false;
	}
}

void initialize()
{
	//functions
	void initialize(void);
	void resize(int, int);
	char* loadOclProgramSource(const char* fileName, const char* preamble, size_t * sizeFinalLength);

	PIXELFORMATDESCRIPTOR pfd;
	int iPixelFormatIndex;
	
	//OpenCL Variables
	cl_platform_id oclPlatformID;
	cl_device_id *oclDeviceIDs;
	cl_uint deviceCount;

	//Get platform ID
	clResult = clGetPlatformIDs(1, &oclPlatformID, NULL);
	if (clResult != CL_SUCCESS) {
		fprintf(gpLogFile, "clGetPlatformIDs() failed clResult = %d, exiting...\n", clResult);
		DestroyWindow(ghwnd);
	}

	//Get Device ID
	// This call is just to get the device Count and hence we are passing 3rd param as 0 and 4th param as NULL
	clResult = clGetDeviceIDs(oclPlatformID, CL_DEVICE_TYPE_GPU, 0, NULL, &deviceCount);
	if (clResult != CL_SUCCESS) {
		fprintf(gpLogFile, "clGetDeviceIDs() failed clResult = %d, exiting...\n", clResult);
		DestroyWindow(ghwnd);
	}
	else if (deviceCount == 0) {
		fprintf(gpLogFile, "No CL_DEVICE_TYPE_GPU found exiting...\n");
		DestroyWindow(ghwnd);
	}
	
	if (deviceCount > 0) {
		oclDeviceIDs = (cl_device_id*)malloc(sizeof(cl_device_id) * deviceCount);
		if (oclDeviceIDs == NULL) {
			fprintf(gpLogFile, "Unable to get memeory for oclDeviceIDs, exiting\n");
			DestroyWindow(ghwnd);
		}

		clResult = clGetDeviceIDs(oclPlatformID, CL_DEVICE_TYPE_GPU, deviceCount, oclDeviceIDs, NULL);
		if (clResult != CL_SUCCESS) {
			fprintf(gpLogFile, "clGetDeviceIDs() failed for deviceCount = %d clResult = %d, exiting...\n", deviceCount, clResult);
			DestroyWindow(ghwnd);
		}

		oclDeviceID = oclDeviceIDs[0];

		free(oclDeviceIDs);
		oclDeviceIDs = NULL;
	}
	
	//code
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cRedBits = 8;
	pfd.cGreenBits = 8;
	pfd.cBlueBits = 8;
	pfd.cAlphaBits = 8;
	pfd.cDepthBits = 32;

	ghdc = GetDC(ghwnd);

	iPixelFormatIndex = ChoosePixelFormat(ghdc, &pfd);
	if (iPixelFormatIndex == 0)
	{
		ReleaseDC(ghwnd, ghdc);
		ghdc = NULL;
	}

	if (SetPixelFormat(ghdc, iPixelFormatIndex, &pfd) == false)
	{
		ReleaseDC(ghwnd, ghdc);
		ghdc = NULL;
	}

	ghrc = wglCreateContext(ghdc);
	if (ghrc == NULL)
	{
		ReleaseDC(ghwnd, ghdc);
		ghdc = NULL;
	}

	if (wglMakeCurrent(ghdc, ghrc) == false)
	{
		wglDeleteContext(ghrc);
		ReleaseDC(ghwnd, ghdc);
		ghrc = NULL;
		ghdc = NULL;
	}

	GLenum glew_error = glewInit();
	if (glew_error != GLEW_OK)
	{
		wglDeleteContext(ghrc);
		ReleaseDC(ghwnd, ghdc);
		ghrc = NULL;
		ghdc = NULL;
	}
	
	//Create OpenCL Context This requires OpenGL context hence the code needs to be written here. 
	// This can be get using Attribute Array
	cl_context_properties contextProperties[] = {
		//Following 
		CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
		CL_WGL_HDC_KHR, (cl_context_properties) wglGetCurrentDC(),
		CL_CONTEXT_PLATFORM, (cl_context_properties) oclPlatformID,
		0
	};

	//Get the OpenCL context using above parameters 
	oclContext = clCreateContext(contextProperties, 1, &oclDeviceID, NULL, NULL, &clResult);
	if (clResult != CL_SUCCESS) {
		fprintf(gpLogFile, "clCreateContext() failed clResult = %d, exiting...\n", clResult);
		DestroyWindow(ghwnd);
	}

	//Create a command Queue 
	oclCommandQueue = clCreateCommandQueueWithProperties(oclContext, oclDeviceID, 0, &clResult);
	if (clResult != CL_SUCCESS) {
		fprintf(gpLogFile, "clCreateCommandQueueWithProperties() failed clResult = %d, exiting...\n", clResult);
		DestroyWindow(ghwnd);
	}

	oclKernelSource = loadOclProgramSource("SineWave.cl", "", &oclKernelSize);

	oclProgram = clCreateProgramWithSource(oclContext, 1, (const char**)&oclKernelSource, &oclKernelSize, &clResult);
	if (clResult != CL_SUCCESS) {
		fprintf(gpLogFile, "clCreateProgramWithSource() failed clResult = %d, exiting...\n", clResult);
		DestroyWindow(ghwnd);
	}

	//Build the CL porogram using commandline argument
	clResult = clBuildProgram(oclProgram, 0, NULL, "-cl-fast-relaxed-math", NULL, NULL);
	if (clResult != CL_SUCCESS) {
		fprintf(gpLogFile, "clBuildProgram() failed clResult = %d, exiting...\n", clResult);
		
		size_t length;
		char buffer[2048];
		clGetProgramBuildInfo(oclProgram, oclDeviceID, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &length);
		
		fprintf(gpLogFile, "OpenCL program build log = %s, exiting...\n", buffer);
		
		DestroyWindow(ghwnd);
	}

	oclKernel = clCreateKernel(oclProgram, "sineWaveKernel", &clResult);
	if (clResult != CL_SUCCESS) {
		fprintf(gpLogFile, "clCreateKernel() failed clResult = %d, exiting...\n", clResult);
		DestroyWindow(ghwnd);
	}

	//Shader log
	fprintf(gpLogFile, "OpenGL Vendor: %s \n", glGetString(GL_VENDOR));
	fprintf(gpLogFile, "OpenGL Renderer: %s \n", glGetString(GL_RENDERER));
	fprintf(gpLogFile, "OpenGL Version: %s \n", glGetString(GL_VERSION));
	fprintf(gpLogFile, "GLSL Version: %s \n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	//Opengl enabled extentions
	GLint numExtensions;
	glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
	fprintf(gpLogFile, "OpenGL enabled Extentions number: %d\n", numExtensions);
	for (int i = 0; i < numExtensions; i++)
	{
		fprintf(gpLogFile, "%s\n", glGetStringi(GL_EXTENSIONS, i));
	}

	gVertexShaderObject = glCreateShader(GL_VERTEX_SHADER);

	const GLchar *souceVertexShader =
		"#version 450 core\n" \
		"in vec4 vPosition;\n" \
		"uniform mat4 u_mvp_matrix;\n" \
		"void main(void)\n" \
		"{\n" \
		"	gl_Position = u_mvp_matrix * vPosition;\n" \
		"}\n";

	glShaderSource(gVertexShaderObject, 1, (GLchar**)&souceVertexShader, NULL);

	glCompileShader(gVertexShaderObject);

	GLint iInfoLogLength = 0;
	GLint compileStatus = 0;
	char* szInfoLog = NULL;

	glGetShaderiv(gVertexShaderObject, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus == GL_FALSE)
	{
		glGetShaderiv(gVertexShaderObject, GL_INFO_LOG_LENGTH, &iInfoLogLength);

		if (iInfoLogLength > 0)
		{
			szInfoLog = (char*)malloc(iInfoLogLength);
			if (szInfoLog != NULL)
			{
				GLsizei written;
				glGetShaderInfoLog(gVertexShaderObject, (GLsizei)iInfoLogLength, &written, szInfoLog);
				fprintf(gpLogFile, "\nVertexShader compile log status: %s \n", szInfoLog);

				free(szInfoLog);
				szInfoLog = NULL;

				DestroyWindow(ghwnd);
			}
		}
	}

	gFragmentShaderObject = glCreateShader(GL_FRAGMENT_SHADER);

	const GLchar* fragmentShaderSource =
		"#version 450 core\n" \
		"out vec4 FragColor;\n" \
		"void main(void)\n" \
		"{\n" \
		"	FragColor = vec4(1.0, 0.5, 0.0, 1.0);\n"
		"}";

	glShaderSource(gFragmentShaderObject, 1, (GLchar**)&fragmentShaderSource, NULL);

	glCompileShader(gFragmentShaderObject);

	iInfoLogLength = 0;
	compileStatus = 0;
	szInfoLog = NULL;

	glGetShaderiv(gFragmentShaderObject, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus == GL_FALSE)
	{
		glGetShaderiv(gFragmentShaderObject, GL_INFO_LOG_LENGTH, &iInfoLogLength);
		if (iInfoLogLength > 0)
		{
			GLsizei written;

			szInfoLog = (char*)malloc(iInfoLogLength);
			if (szInfoLog != NULL)
			{
				glGetShaderInfoLog(gFragmentShaderObject, (GLsizei)iInfoLogLength, &written, szInfoLog);

				fprintf(gpLogFile, "\nFragment shader compile log:%s\n", szInfoLog);

				free(szInfoLog);

				szInfoLog = NULL;
			}
		}
	}
	
	/**Shader Program  */
	gShaderProgramObject = glCreateProgram();

	glAttachShader(gShaderProgramObject, gVertexShaderObject);
	glAttachShader(gShaderProgramObject, gFragmentShaderObject);
	
	//Binding in the variable for vertex
	glBindAttribLocation(gShaderProgramObject, RRB_ATTRIBUTE_POSITION, "vPosition");

	//link shader 
	glLinkProgram(gShaderProgramObject);

	GLint iShaderProgramLinkStatus = 0;
	iInfoLogLength = 0;
	szInfoLog = NULL;

	glGetProgramiv(gShaderProgramObject, GL_LINK_STATUS, &iShaderProgramLinkStatus);
	if (iShaderProgramLinkStatus == GL_FALSE)
	{
		glGetProgramiv(gShaderProgramObject, GL_INFO_LOG_LENGTH, &iInfoLogLength);
		if (iInfoLogLength > 0)
		{
			szInfoLog = (GLchar*)malloc(iInfoLogLength);
			if (szInfoLog)
			{
				GLsizei written;
				glGetProgramInfoLog(gShaderProgramObject, (GLsizei)iInfoLogLength, &written, szInfoLog);
				fprintf(gpLogFile, "\nShader Program Link log: %s \n", szInfoLog);

				free(szInfoLog);
				szInfoLog = NULL;
			}
		}
	}

	gMVPMatrixUniform = glGetUniformLocation(gShaderProgramObject, "u_mvp_matrix");

	//Initilaize Position for sine wave
	for (int i = 0; i < meshWidth; i++) {
		for (int j = 0; j < meshHeight; j++) {
			for (int k = 0; k < 4; k++) {
				position[i][j][k] = 0.0f;
			}
		}
	}
		
	//Send to vPosition 
	glGenVertexArrays(1, &gVao);
	glBindVertexArray(gVao);

	glGenBuffers(1, &gVbo_Position);
	glBindBuffer(GL_ARRAY_BUFFER, gVbo_Position);

	glBufferData(GL_ARRAY_BUFFER, sizeof(position), NULL, GL_DYNAMIC_DRAW);
	//glVertexAttribPointer(RRB_ATTRIBUTE_POSITION, 4, GL_FLOAT, GL_FALSE, 0, NULL);
	//glEnableVertexAttribArray(RRB_ATTRIBUTE_POSITION);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glGenBuffers(1, &gVbo_GPU);
	glBindBuffer(GL_ARRAY_BUFFER, gVbo_GPU);
	glBufferData(GL_ARRAY_BUFFER, sizeof(position), NULL, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	clGraphicsResource = clCreateFromGLBuffer(oclContext, CL_MEM_WRITE_ONLY, gVbo_GPU, &clResult);
	if (clResult != CL_SUCCESS) {
		fprintf(gpLogFile, "clCreateFromGLBuffer() failed clResult = %d, exiting...\n", clResult);
		DestroyWindow(ghwnd);
	}

	//stopping recording
	glBindVertexArray(0);

	//Depth
	glShadeModel(GL_SMOOTH);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glEnable(GL_CULL_FACE);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	//Initialization of perspective matrix 
	gPerspectiveProjectionMatrix = mat4::identity();

	resize(WINWIDTH, WINHEIGHT);
}

void resize(int width, int height)
{
	if (height <= 0)
		height = 1;

	glViewport(0, 0, (GLsizei)width, (GLsizei)height);

	gPerspectiveProjectionMatrix = vmath::perspective(45.0f, (GLfloat)width / (GLfloat)height, 0.1f, 100.0f);
}

void display()
{
	//CPU kernel
	void launchCPUKernel(int meshWidth, int meshHeight, float animationTime);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(gShaderProgramObject);
		
	mat4 modelViewMatrix = mat4::identity();
	mat4 modelViewProjectionMatrix = mat4::identity();
	mat4 translationMatrix = mat4::identity();

	//translationMatrix = vmath::translate(0.0f, 0.0f, 0.0f);
	//modelViewMatrix = translationMatrix;

	modelViewProjectionMatrix = gPerspectiveProjectionMatrix * modelViewMatrix;

	glUniformMatrix4fv(gMVPMatrixUniform, 1, GL_FALSE, modelViewProjectionMatrix);
	

	glBindVertexArray(gVao);	

	if (bOnGPU) {

		clResult = clSetKernelArg(oclKernel, 0, sizeof(cl_mem), (void*)&clGraphicsResource);

		if (clResult != CL_SUCCESS) {
			fprintf(gpLogFile, "clSetKernelArg() failed for clGraphicsResource clResult = %d, exiting...\n", clResult);
			DestroyWindow(ghwnd);
		}

		clResult = clSetKernelArg(oclKernel, 1, sizeof(cl_int), (void*)&meshWidth);

		if (clResult != CL_SUCCESS) {
			fprintf(gpLogFile, "clSetKernelArg() failed for meshWidth clResult = %d, exiting...\n", clResult);
			DestroyWindow(ghwnd);
		}

		clResult = clSetKernelArg(oclKernel, 2, sizeof(cl_int), (void*)&meshHeight);

		if (clResult != CL_SUCCESS) {
			fprintf(gpLogFile, "clSetKernelArg() failed for meshHeight clResult = %d, exiting...\n", clResult);
			DestroyWindow(ghwnd);
		}

		clResult = clSetKernelArg(oclKernel, 3, sizeof(cl_float), (void*)&animationTime);

		if (clResult != CL_SUCCESS) {
			fprintf(gpLogFile, "clSetKernelArg() failed for animationTime clResult = %d, exiting...\n", clResult);
			DestroyWindow(ghwnd);
		}

		clResult = clEnqueueAcquireGLObjects(oclCommandQueue, 1, &clGraphicsResource, 0, NULL, NULL);
		if (clResult != CL_SUCCESS) {
			fprintf(gpLogFile, "clEnqueueAcquireGLObjects() failed, clResult = %d, exiting...\n", clResult);
			DestroyWindow(ghwnd);
		}

		size_t globalWorkSize[2];
		globalWorkSize[0] = meshWidth;
		globalWorkSize[1] = meshHeight;

		clResult = clEnqueueNDRangeKernel(oclCommandQueue, oclKernel, 2, NULL, globalWorkSize, 0, 0, NULL, NULL);
		if (clResult != CL_SUCCESS) {
			fprintf(gpLogFile, "clEnqueueNDRangeKernel() failed, clResult = %d, exiting...\n", clResult);
			DestroyWindow(ghwnd);
		}

		clResult = clEnqueueReleaseGLObjects(oclCommandQueue, 1, &clGraphicsResource, 0, NULL, NULL);
		if (clResult != CL_SUCCESS) {
			fprintf(gpLogFile, "clEnqueueReleaseGLObjects() failed, clResult = %d, exiting...\n", clResult);
			DestroyWindow(ghwnd);
		}

		glBindBuffer(GL_ARRAY_BUFFER, gVbo_GPU);
		
	}
	else {
		launchCPUKernel(meshWidth, meshHeight, animationTime);
		glBindBuffer(GL_ARRAY_BUFFER, gVbo_Position);
		glBufferData(GL_ARRAY_BUFFER, sizeof(position), position, GL_DYNAMIC_DRAW);
	}

	
	glVertexAttribPointer(RRB_ATTRIBUTE_POSITION, 4, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(RRB_ATTRIBUTE_POSITION);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glDrawArrays(GL_POINTS, 0, meshWidth * meshHeight);
		
	glBindVertexArray(0);

	glUseProgram(0);

	SwapBuffers(ghdc);

	animationTime += 0.01f;
}

void uninitialize()
{
	if (gbFullscreen)
	{
		dwStyle = GetWindowLong(ghwnd, GWL_STYLE);
		SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(ghwnd, &wpPrev);
		SetWindowPos(ghwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

		ShowCursor(TRUE);
		gbFullscreen = false;
	}

	if (oclKernelSource) {
		free(oclKernelSource);
		oclKernelSource = NULL;
	}

	if (clGraphicsResource) {
		clReleaseMemObject(clGraphicsResource);		
	}

	clReleaseKernel(oclKernel);
	clReleaseProgram(oclProgram);
	clReleaseCommandQueue(oclCommandQueue);
	clReleaseContext(oclContext);

	if (gVao)
	{
		glDeleteVertexArrays(1, &gVao);
		gVao = NULL;
	}

	if (gVbo_Position)
	{
		glDeleteBuffers(1, &gVbo_Position);
		gVbo_Position = NULL;
	}

	if (gShaderProgramObject)
	{
		glUseProgram(gShaderProgramObject);

		GLsizei shaderCount; 

		glGetProgramiv(gShaderProgramObject, GL_ATTACHED_SHADERS, &shaderCount);

		if (shaderCount > 0)
		{
			GLuint* pShaders = (GLuint*)malloc(sizeof(GLuint) * shaderCount);

			if (!pShaders)
			{
				fprintf(gpLogFile, "Unbale to get memory for Deleting shaders, exiting...");
			}

			glGetAttachedShaders(gShaderProgramObject, shaderCount, &shaderCount, pShaders);

			for (GLsizei i = 0; i < shaderCount; i++)
			{
				glDetachShader(gShaderProgramObject, pShaders[i]);
				glDeleteShader(pShaders[i]);
			}


			if (pShaders)
			{
				free(pShaders);
				pShaders = NULL;
			}
		}

		glDeleteProgram(gShaderProgramObject);
		gShaderProgramObject = 0;

		glUseProgram(0);
	}

	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(ghrc);
	ghrc = NULL;

	ReleaseDC(ghwnd, ghdc);
	ghdc = NULL;

	if (gpLogFile)
	{
		fprintf(gpLogFile, "\n\nProgram Termination\n");
		fclose(gpLogFile);
		gpLogFile = NULL;
	}
}

void launchCPUKernel(int meshWidth, int meshHeight, float animationTime) {
	for (int i = 0; i < meshWidth; i++) {
		for (int j = 0; j < meshHeight; j++) {
			for (int k = 0; k < 4; k++) {
				float u = i / (float)meshWidth;
				float v = j / (float)meshHeight;

				//Normalize 
				u = u * 2.0f - 1.0f;
				v = v * 2.0f - 1.0f;

				float frequency = 4.0f;

				float w = sinf(u * frequency + animationTime) *
					cosf(v * frequency + animationTime) *
					0.5f;

				if (k == 0) 
					position[i][j][k] = u;
				if(k == 1)
					position[i][j][k] = w;
				if (k == 2)
					position[i][j][k] = v;
				if (k == 3)
					position[i][j][k] = 1.0f;
			}
		}
	}
}


char* loadOclProgramSource(const char* fileName, const char* preamble, size_t* sizeFinalLength) {
	//locals
	FILE* pFile = NULL;
	size_t sizeSourceLength;
	
	

	pFile = fopen(fileName, "rb");
	if (pFile == NULL)
		return NULL;

	size_t sizePreambleLength = (size_t)strlen(preamble);

	//get the length of Source Code
	fseek(pFile, 0, SEEK_END);
	sizeSourceLength = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);

	//allocate a buffer for the source code string and read it in
	char* sourceString = (char*)malloc(sizeSourceLength + sizePreambleLength + 1);
	memcpy(sourceString, preamble, sizePreambleLength);

	if (fread((sourceString)+sizePreambleLength, sizeSourceLength, 1, pFile) != 1) {
		fclose(pFile);
		free(sourceString);
		return(0);
	}

	//close the file and return the total length of the combined (preamble + source) string
	fclose(pFile);
	if (sizeFinalLength != 0) {
		*sizeFinalLength = sizeSourceLength + sizePreambleLength;
	}

	sourceString[sizeSourceLength + sizePreambleLength] = '\0';
	return(sourceString);
}