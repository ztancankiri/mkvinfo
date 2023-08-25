#include "EBMLCallback.h"

EBMLCallback::EBMLCallback(const char* filename) {
	file = fopen(filename, "rb+");
	if (!file) {
		std::cerr << "Failed to open file: " << filename << std::endl;
	}
}

EBMLCallback::~EBMLCallback() {
	close();
}

std::size_t EBMLCallback::read(void* Buffer, std::size_t Size) {
	if (Buffer == nullptr || Size == 0) {
		return 0;
	}

	std::size_t bytesRead = fread(Buffer, 1, Size, file);
	return bytesRead;
}

void EBMLCallback::setFilePointer(std::int64_t Offset, libebml::seek_mode Mode) {
	int seekResult = fseek(file, static_cast<long>(Offset), Mode);
	if (seekResult != 0) {
		std::cerr << "Seek operation failed." << std::endl;
	}
}

std::size_t EBMLCallback::write(const void* Buffer, std::size_t Size) {
	if (Buffer == nullptr || Size == 0) {
		return 0;
	}

	std::size_t bytesWritten = fwrite(Buffer, 1, Size, file);
	return bytesWritten;
}

std::uint64_t EBMLCallback::getFilePointer() {
	long currentPosition = ftell(file);
	return static_cast<std::uint64_t>(currentPosition);
}

void EBMLCallback::close() {
	if (file != nullptr) {
		int closeResult = fclose(file);
		if (closeResult != 0) {
			std::cerr << "File close operation failed." << std::endl;
		}
		file = nullptr;
	}
}

std::uint64_t EBMLCallback::get_size() {
	if (file == nullptr) {
		std::cerr << "File handle is not valid." << std::endl;
		return 0;
	}

	struct stat fileStat;
	if (fstat(fileno(file), &fileStat) != 0) {
		std::cerr << "Failed to get file size." << std::endl;
		return 0;
	}

	return static_cast<std::uint64_t>(fileStat.st_size);
}

bool EBMLCallback::setFilePointer2(int64_t offset, libebml::seek_mode mode) {
	try {
		setFilePointer(offset, mode);
		return true;
	} catch (...) {
		return false;
	}
}

unsigned char EBMLCallback::read_uint8() {
	unsigned char value;
	read(&value, 1);
	return value;
}

uint32_t EBMLCallback::read_uint32_be() {
	unsigned char buffer[4];
	read(buffer, 4);
	return get_uint32_be(buffer);
}