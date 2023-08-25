#ifndef VINT_H
#define VINT_H

#include <ebml/EbmlId.h>

#include <iostream>
#include <memory>

#include "EBMLCallback.h"

class vint {
   public:
	int64_t value;
	int coded_size;
	bool is_set;

   public:
	enum read_mode_e {
		rm_normal,
		rm_ebml_id,
	};

	vint();
	vint(int64_t value, int coded_size);
	vint(libebml::EbmlId const &id);
	bool is_unknown();
	bool is_valid();

	operator libebml::EbmlId() const;

   public:
	static vint read(EBMLCallback &in, read_mode_e read_mode = rm_normal);
	static vint read(std::shared_ptr<EBMLCallback> const &in, read_mode_e read_mode = rm_normal);

	static vint read_ebml_id(EBMLCallback &in);
	static vint read_ebml_id(std::shared_ptr<EBMLCallback> const &in);
};

#endif