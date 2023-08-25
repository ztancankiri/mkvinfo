#ifndef ZTAN_CALLBACK_H
#define ZTAN_CALLBACK_H

#include <ebml/IOCallback.h>
#include <sys/stat.h>

#include <iostream>

#include "Utils.h"

class EBMLCallback : public libebml::IOCallback {
   public:
	EBMLCallback(const char* filename);
	virtual ~EBMLCallback();

	std::size_t read(void* Buffer, std::size_t Size) override;
	void setFilePointer(std::int64_t Offset, libebml::seek_mode Mode = libebml::seek_beginning) override;
	std::size_t write(const void* Buffer, std::size_t Size) override;
	std::uint64_t getFilePointer() override;
	void close() override;

	bool setFilePointer2(int64_t offset, libebml::seek_mode mode = libebml::seek_beginning);
	std::uint64_t get_size();
	virtual unsigned char read_uint8();
	virtual uint32_t read_uint32_be();

   private:
	FILE* file;
};

#endif	// ZTAN_CALLBACK_H