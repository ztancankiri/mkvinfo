#include "vint.h"

vint vint::read_ebml_id(EBMLCallback &in) {
	return read(in, rm_ebml_id);
}

vint vint::read(EBMLCallback &in, vint::read_mode_e read_mode) {
	auto pos = static_cast<int64_t>(in.getFilePointer());
	auto file_size = in.get_size();
	auto mask = 0x80;
	auto value_len = 1;

	if (pos >= file_size)
		return {};

	auto first_byte = in.read_uint8();

	while (0 != mask) {
		if (0 != (first_byte & mask))
			break;

		mask >>= 1;
		value_len++;
	}

	if ((pos + value_len) > file_size)
		return {};

	if ((rm_ebml_id == read_mode) && ((0 == mask) || (4 < value_len)))
		return {};

	auto value = static_cast<int64_t>(first_byte);
	if (rm_normal == read_mode)
		value &= ~mask;

	int i;
	for (i = 1; i < value_len; ++i) {
		value <<= 8;
		value |= in.read_uint8();
	}

	return {value, value_len};
}

vint vint::read(std::shared_ptr<EBMLCallback> const &in, vint::read_mode_e read_mode) {
	return read(*in, read_mode);
}

vint vint::read_ebml_id(std::shared_ptr<EBMLCallback> const &in) {
	return read(*in, rm_ebml_id);
}

bool vint::is_valid() {
	return is_set && (0 <= coded_size);
}

bool vint::is_unknown() {
	return !is_valid() || ((1 == coded_size) && (0x000000000000007fll == value)) || ((2 == coded_size) && (0x0000000000003fffll == value)) || ((3 == coded_size) && (0x00000000001fffffll == value)) || ((4 == coded_size) && (0x000000000fffffffll == value)) || ((5 == coded_size) && (0x00000007ffffffffll == value)) || ((6 == coded_size) && (0x000003ffffffffffll == value)) || ((7 == coded_size) && (0x0001ffffffffffffll == value)) || ((8 == coded_size) && (0x00ffffffffffffffll == value));
}

vint::vint()
	: value{}, coded_size{-1}, is_set{} {
}

vint::vint(int64_t value, int coded_size) : value{value}, coded_size{coded_size}, is_set{true} {
}

vint::vint(libebml::EbmlId const &id) : value{id.GetValue()}, coded_size{static_cast<int>(id.GetLength())}, is_set{true} {
}