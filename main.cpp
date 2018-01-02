// GLCompute.cpp : Defines the entry point for the console application.
//

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <ctime>


GLuint vertexArray;
GLuint vertexBuffer;

GLuint indexBuffer;

GLuint program, computeProgram;

GLuint frontBuffer, backBuffer;

std::string vertexSource = R"(
#version 430 core

in vec3 position;
in vec2 texCoord;

out vec2 outTexCoord;

void main() {
	outTexCoord = texCoord;
	gl_Position = vec4(position, 1.0);
}
)";

std::string fragmentSource = R"(
#version 430 core

in vec2 outTexCoord;
out vec4 outColor;

uniform sampler2D tex;

void main() {
	outColor = texture(tex, outTexCoord) ;
}
)";

std::string computeSource = R"(
#version 440 core

layout (binding = 1, rgba32f) uniform readonly image2D sourceTex;
layout (rgba32f, binding = 0)  uniform writeonly image2D destTex;

layout (local_size_x = 1, local_size_y = 1) in;

void main() {
	ivec2 position = ivec2(gl_GlobalInvocationID.xy);
	
	ivec2 positions[8] = ivec2[] (
		ivec2(position.x + 1, position.y),
		ivec2(position.x - 1, position.y),
		ivec2(position.x, position.y - 1),
		ivec2(position.x, position.y + 1),

		ivec2(position.x + 1, position.y + 1),
		ivec2(position.x + 1, position.y - 1),
		ivec2(position.x - 1, position.y + 1),
		ivec2(position.x - 1, position.y - 1)
	);

	int neighborCount = 0;
	for (int i = 0; i < 8; i++) {
		vec4 pixel = imageLoad(sourceTex, positions[i]);
		if (pixel.x == 1.0 && pixel.y == 1.0 && pixel.z == 1.0) {
			neighborCount++;
		}
	}

	vec4 pixel = imageLoad(sourceTex, position);

	vec4 alive = vec4(1.0, 1.0, 1.0, 1.0);
	vec4 dead = vec4(0.0, 0.0, 0.0, 1.0);
	vec4 outputPixel = vec4(0.0, 0.0, 0.0, 1.0);

	if (pixel.x == 1.0 && pixel.y == 1.0 && pixel.z == 1.0) {
		if (neighborCount < 2) {
			outputPixel = dead;
		} else if (neighborCount == 2 || neighborCount == 3) {
			outputPixel = pixel;
		} else {
			outputPixel = dead;
		}
		
	} else {
		if (neighborCount == 3) {
			outputPixel = alive;
		}
	}


	imageStore(destTex, position, outputPixel);

}
)";

struct Vector4 {
	float x, y, z, w;
};

void generateTexture(GLuint *texture, std::vector<Vector4> &pixels, int width, int height) {
	glGenTextures(1, texture);
	glBindTexture(GL_TEXTURE_2D, *texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, pixels.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glGenerateMipmap(GL_TEXTURE_2D);
}

void init(int width, int height) {
	
	glGenVertexArrays(1, &vertexArray);
	glBindVertexArray(vertexArray);

	const GLfloat triangle[] = {
		-1, 1, 0,  0, 0,
		1, 1, 0,   1, 0,
		1, -1, 0,  1, 1,
		-1, -1, 0, 0, 1,
	};

	glGenBuffers(1, &vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(triangle), triangle, GL_STATIC_DRAW);

	GLuint indicies[] = {
		
		0, 1, 2,
		2, 3, 0,
		//2, 0, 3,
		//1, 0, 2,
		
	};

	glGenBuffers(1, &indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indicies), indicies, GL_STATIC_DRAW);


	glActiveTexture(GL_TEXTURE0);
	
	
	std::vector<Vector4> pixels(width * height);
	std::srand(std::time(0));
	std::generate(pixels.begin(), pixels.end(), []() -> Vector4 { return std::rand() % 300 > 100 ? Vector4{ 1, 1, 1, 1 } : Vector4{ 0, 0, 0, 1 }; });
	
	generateTexture(&frontBuffer, pixels, width, height);

	std::generate(pixels.begin(), pixels.end(), []() -> Vector4 { return Vector4{ 0, 0, 0, 1 }; });

	glActiveTexture(GL_TEXTURE1);
	generateTexture(&backBuffer, pixels, width, height);

	glBindImageTexture(0, frontBuffer, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	glBindImageTexture(1, backBuffer, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

	glActiveTexture(GL_TEXTURE0);
	program = glCreateProgram();
	computeProgram = glCreateProgram();

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);

	GLchar log[10240];
	GLsizei length;
	const char *src = vertexSource.c_str();

	glShaderSource(vertexShader, 1, &src, nullptr);

	src = fragmentSource.c_str();
	glShaderSource(fragmentShader, 1, &src, nullptr);

	src = computeSource.c_str();
	glShaderSource(computeShader, 1, &src, nullptr);
	

	glCompileShader(vertexShader);
	glCompileShader(fragmentShader);
	glCompileShader(computeShader);
	glGetShaderInfoLog(computeShader, 10239, &length, log);
	std::cout << log;
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glAttachShader(computeProgram, computeShader);

	glLinkProgram(program);
	glLinkProgram(computeProgram);
	glUseProgram(computeProgram);
	//glUniform1i(glGetUniformLocation(program, "sourceTex"), sourceTexture);
	
}

void render(int displayTexture, int sourceTexture, int width, int height) {
	glUseProgram(computeProgram);
	
	
	glUniform1i(glGetUniformLocation(computeProgram, "sourceTex"), sourceTexture);
	glUniform1i(glGetUniformLocation(computeProgram, "destTex"), displayTexture);
	
	glDispatchCompute(width , height , 1);

	glMemoryBarrier(GL_ALL_BARRIER_BITS);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(program);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	int positionAttrib = glGetAttribLocation(program, "position");
	glEnableVertexAttribArray(positionAttrib);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(0 * sizeof(GLfloat)));

	int texCoordAttrib = glGetAttribLocation(program, "texCoord");
	glEnableVertexAttribArray(texCoordAttrib);
	glVertexAttribPointer(texCoordAttrib, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(GLfloat)));

	
	glUniform1i(glGetUniformLocation(program, "tex"), displayTexture);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	int error = glGetError();
	//glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
	// glDrawArrays(GL_TRIANGLES, 3, 3);
	glDisableVertexAttribArray(0);
	
}

static void APIENTRY openglDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userData) {
	std::cout << message << '\n';
}

void swap(int &texture) {
	if (texture == 1) {
		texture = 0;
	}
	else {
		texture = 1;
	}
}

int main() {
	int width = 1280;
	int height = 720;
	std::cout << computeSource;
	glfwInit();
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	auto window = glfwCreateWindow(width, height, "Compute Test", nullptr, nullptr);
	
	glfwMakeContextCurrent(window);
	glewInit();
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(openglDebugCallback, nullptr);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
	init(width, height);

	int sourceTexture = 0;
	int displayTexture = 1;
	glfwSwapInterval(0);

	while (!glfwWindowShouldClose(window)) {
		
		render(displayTexture, sourceTexture, width, height);
		glfwSwapBuffers(window);
		glfwPollEvents();

		swap(sourceTexture);
		swap(displayTexture);
	}
	return 0;
}

