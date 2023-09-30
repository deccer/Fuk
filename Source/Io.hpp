#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <fstream>

std::vector<char> ReadFile(const std::string& filePath)
{
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open())
    {
		throw std::runtime_error("Failed to open file");
	}

	size_t file_size = (size_t)file.tellg();
	std::vector<char> buffer(file_size);

	file.seekg (0);
	file.read (buffer.data(), static_cast<std::streamsize>(file_size));
	file.close ();

	return buffer;
}