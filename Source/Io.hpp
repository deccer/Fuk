#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <fstream>

template<typename T>
std::vector<T> ReadFile(const std::string& filePath)
{
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open())
    {
		throw std::runtime_error("Failed to open file");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<T> buffer(fileSize);

	file.seekg(0);
	file.read((char*)buffer.data(), static_cast<std::streamsize>(fileSize));
	file.close();

	return buffer;
}