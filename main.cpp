#include<iostream>

#include<glad/glad.h>
#include<GLFW/glfw3.h>


#include<string>
#include<map>
#include<vector>
#include<fstream>
#include<algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include<ft2build.h>
#include FT_FREETYPE_H

static float SCR_WIDTH = 800;
static float SCR_HEIGHT = 600;

struct Character {
	unsigned int TextureID;
	glm::ivec2   Size;
	glm::ivec2   Bearing;
	unsigned int Advance;
};
void RenderText(unsigned int shader, std::string text, float x, float y, float scale, glm::vec3 color);

std::map<GLchar, Character> Characters;

FT_Library ft;

unsigned int VAO, VBO;

unsigned int focus = 0;


void charCallback(GLFWwindow* window, unsigned int codepoint);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void framebufferSizeCallback(GLFWwindow* window, int width, int height);
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

void replaceAll(std::string& str, const std::string& from, const std::string& to);

std::vector<std::string> lines;
unsigned int currentLine = 0;
float scale = 0.75;
bool overwrite = true;
unsigned int program;

std::string fileName = "KT Editor.cpp";

int main(int argc, char** argv) 
{
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(800, 600, "KT Editor", NULL, NULL);

	glfwSetCharCallback(window, charCallback);
	glfwSetKeyCallback(window, keyCallback);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
	glfwSetScrollCallback(window, scrollCallback);

	glfwMakeContextCurrent(window);
	gladLoadGL();
	program = glCreateProgram();
	{
		unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
		const char* source = 
		"#version 330 core\n"
		"layout (location = 0) in vec4 vertex;\n"
		"out vec2 TexCoords;\n"
		"uniform mat4 projection;\n"
		"void main()\n"
		"{\n"
		"    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);\n"
		"    TexCoords = vertex.zw;\n"
		"}\n";
		glShaderSource(vertexShader,1, &source, 0);
		glCompileShader(vertexShader);
		glAttachShader(program, vertexShader);
		glDeleteShader(vertexShader);
	}
	{
		unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		const char* source =
			"#version 330 core\n"
			"in vec2 TexCoords;\n"
			"out vec4 color;\n"
			"uniform sampler2D text;\n"
			"uniform vec3 textColor;\n"
			"void main()\n"
			"{\n"
			"    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);\n"
			"    color = vec4(textColor, 1.0) * sampled;\n"
			"}\n";
		glShaderSource(fragmentShader, 1, &source, 0);
		glCompileShader(fragmentShader);
		glAttachShader(program, fragmentShader);
		glDeleteShader(fragmentShader);
	}
	glLinkProgram(program);

	glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(SCR_WIDTH), 0.0f, static_cast<float>(SCR_HEIGHT));

	glUseProgram(program);
	glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

	if (FT_Init_FreeType(&ft))
	{
		std::cout << "--Could not init FreeType Library" << std::endl;
		return EXIT_FAILURE;
	}

	const char* font_name = "consolab.ttf";

	FT_Face face;
	if (FT_New_Face(ft, font_name, 0, &face)) {
		std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
		return -1;
	}
	else {
		FT_Set_Pixel_Sizes(face, 0, 48);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		for (unsigned char c = 0; c < 128; c++)
		{
			if (FT_Load_Char(face, c, FT_LOAD_RENDER))
			{
				std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
				continue;
			}
			unsigned int texture;
			glGenTextures(1, &texture);
			glBindTexture(GL_TEXTURE_2D, texture);
			glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_RED,
				face->glyph->bitmap.width,
				face->glyph->bitmap.rows,
				0,
				GL_RED,
				GL_UNSIGNED_BYTE,
				face->glyph->bitmap.buffer
			);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			Character character = {
				texture,
				glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
				glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
				static_cast<unsigned int>(face->glyph->advance.x)
			};
			Characters.insert(std::pair<char, Character>(c, character));
		}
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	FT_Done_Face(face);

	FT_Done_FreeType(ft);

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	std::map<char, bool> key;
	if (argc > 1)
	{
		std::ifstream file(argv[1]);
		std::string line;
		while (std::getline(file, line))
		{
			replaceAll(line, "\t", "   ");
			lines.push_back(line);
		}
		file.close();
		fileName = argv[1];
	}
	lines.push_back(std::string());


	glm::vec2 chSize = Characters['a'].Size;
	glm::vec2 chBearing = Characters['a'].Bearing;

	double cTime = 0;
	double pTime = 0;
	while(!glfwWindowShouldClose(window))
	{
		cTime = glfwGetTime();
		float deltaTime = cTime - pTime;
		pTime = cTime;
		glClear(GL_COLOR_BUFFER_BIT);
		
		glEnable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glUseProgram(program);
		for (int i = 0; i < lines.size(); i++)
		{
			RenderText(program, std::to_string(i + 1), 1, SCR_HEIGHT - ((i+1) * (chSize.y * scale * 1.5f)), scale, glm::vec3(0.2f, 0.3f, 0.8f));
			RenderText(program, lines[i], (scale* (Characters['a'].Advance >> 6))*3.5, SCR_HEIGHT - ((i+1) * chSize.y * scale * 1.5f), scale, glm::vec3(sinf(cTime) * 0.5 + 0.5f, cosf(cTime) * 0.5 + 0.5f, 0.5f));
		}
		if (focus == lines[currentLine].size() or !overwrite)
			RenderText(program, "|", (scale * (Characters['a'].Advance >> 6))*3 + (focus * (scale * (Characters['a'].Advance>>6))), SCR_HEIGHT - ((currentLine+1) * (chSize.y * scale * 1.5f)), scale, glm::vec3(sinf(cTime) * 0.5 + 0.5f, cosf(cTime) * 0.5 + 0.5f, 0.5f));
		else
			RenderText(program, "[]", (scale* (Characters['a'].Advance >> 6))*3 + (focus * (scale * (Characters['a'].Advance >> 6))), SCR_HEIGHT - ((currentLine+1) * (chSize.y * scale * 1.5f)), scale, glm::vec3(sinf(cTime) * 0.5 + 0.5f, cosf(cTime) * 0.5 + 0.5f, 0.5f));
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	glfwTerminate();
	return EXIT_SUCCESS;
}

void charCallback(GLFWwindow* window, unsigned int codepoint)
{
	if (codepoint > 127) codepoint = 287;
	if (overwrite)
	{
		if (lines[currentLine].size() == focus)
			lines[currentLine] += codepoint;
		else
			lines[currentLine][focus] = codepoint;
	}
	else
	{
		if (lines[currentLine].size() == focus)
			lines[currentLine] += codepoint;
		else
			lines[currentLine].insert(lines[currentLine].begin() + focus, codepoint);
	}
	focus++;

}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	switch (scancode)
	{
		case 14:
			if (lines[currentLine].size() and action)
			{
				if (focus)
				{
					lines[currentLine].erase(lines[currentLine].begin() + focus-1);
					focus--;
				}
				else
				{
					lines[currentLine - 1] += lines[currentLine];
					lines.erase(std::next(lines.begin(), currentLine));
					currentLine--;
					focus = lines[currentLine].size();

				}
			}
			else if(lines[currentLine].size()==0 and action and lines.size()>1 and currentLine)
			{
				lines.erase(std::next(lines.begin(), currentLine));
				currentLine--;
				focus = lines[currentLine].size();
			}
			break;
		case 328:
			if (action and currentLine)
			{
				currentLine--;
				if(focus == lines[currentLine+1].size())
					focus = lines[currentLine].size();
				else if (focus > lines[currentLine].size())
				{
					focus = lines[currentLine].size();
				}
			}
			break;
		case 336:
			if (action and lines.size() - 1 != currentLine)
			{
				currentLine++;
				if (focus == lines[currentLine-1].size())
					focus = lines[currentLine].size();
				else if (focus > lines[currentLine].size())
					focus = lines[currentLine].size();
			}
			break;
		case 331:
			if (!mods and action)
			{
				if (focus)
					focus--;
				else
				{
					if (currentLine)
					{
						currentLine--;
						focus = lines[currentLine].size();
					}
				}
			}
			break;
		case 333:
			if (!mods and action)
			{
				if (lines[currentLine].size() > focus)
					focus++;
				else if(lines.size()-1!=currentLine)
				{
					focus = 0;
					currentLine++;
				}
			}
			break;
		case 28:
			if(!mods and action)
			{
				lines.insert(lines.begin() + currentLine +1, lines[currentLine].substr(focus,lines[currentLine].size()));
				lines[currentLine].erase(focus, lines[currentLine].size());
				currentLine++;
				focus = lines[currentLine].size();
			}
			break;
		case 15:
			if (!mods and action)
			{
				focus += 3;
				lines[currentLine] += "   ";
			}
			break;
		case 327:
			if (!mods and action)
			{
				focus = 0;
			}
			else if (mods == 2 and action)
			{
				focus = 0;
				currentLine = 0;
			}
			break;
		case 335:
			if (!mods and action)
			{
				focus = lines[currentLine].size();
			}
			else if (mods == 2 and action)
			{
				currentLine = lines.size() - 1;
				focus = lines[currentLine].size();
			}
			break;
		case 47:
			if(mods==2 and action)lines[currentLine] += glfwGetClipboardString(window);
			break;
		case 31:
			if (mods == 2 and action == 1)
			{
				std::ofstream file(fileName);
				for (int i = 0; i < lines.size(); i++)
					file << lines[i].c_str() << "\n";
				file.close();
			}
			break;
		case 82:
			if (mods == 2 and action == 1)
			{
				overwrite = !overwrite;
			}
			break;
	}

}
void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height));
	glUseProgram(program);
	glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
	SCR_WIDTH = width;
	SCR_HEIGHT = height;
}
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	scale += yoffset*0.05;
}
void RenderText(unsigned int shader, std::string text, float x, float y, float scale, glm::vec3 color)
{
	glUniform3f(glGetUniformLocation(shader, "textColor"), color.x, color.y, color.z);
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(VAO);

	std::string::const_iterator c;
	for (c = text.begin(); c != text.end(); c++)
	{
		Character ch = Characters[*c];

		float xpos = x + ch.Bearing.x * scale;
		float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

		float w = ch.Size.x * scale;
		float h = ch.Size.y * scale;
		float vertices[6][4] = {
			{ xpos,     ypos + h,   0.0f, 0.0f },
			{ xpos,     ypos,       0.0f, 1.0f },
			{ xpos + w, ypos,       1.0f, 1.0f },

			{ xpos,     ypos + h,   0.0f, 0.0f },
			{ xpos + w, ypos,       1.0f, 1.0f },
			{ xpos + w, ypos + h,   1.0f, 0.0f }
		};
		glBindTexture(GL_TEXTURE_2D, ch.TextureID);

		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glDrawArrays(GL_TRIANGLES, 0, 6);
		x += (ch.Advance >> 6) * scale; 
	}
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
}
void replaceAll(std::string& str, const std::string& from, const std::string& to)
{
	if (from.empty())
		return;
	size_t start_pos = 0;
	while((start_pos = str.find(from, start_pos))!= std::string::npos)
	{
		str.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}
}
