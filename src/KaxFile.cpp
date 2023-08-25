#include "KaxFile.h"

KaxFile::KaxFile(EBMLCallback &in) : in(in), resynced{}, resync_start_pos{}, file_size{static_cast<uint64_t>(in.get_size())}, segment_end{}, timestamp_scale{TIMESTAMP_SCALE}, last_timestamp{-1}, es{new EbmlStream{in}} {
}

std::shared_ptr<EbmlElement> KaxFile::read_next_level1_element(uint32_t wanted_id) {
	return read_next_level1_element_internal(wanted_id);
}

std::shared_ptr<EbmlElement> KaxFile::read_next_level1_element_internal(uint32_t wanted_id) {
	if (segment_end && (in.getFilePointer() >= segment_end))
		return nullptr;

	resynced = false;
	resync_start_pos = 0;

	// Read the next ID.
	auto search_start_pos = in.getFilePointer();
	auto actual_id = vint::read_ebml_id(in);
	in.setFilePointer(search_start_pos);

	// If no valid ID was read then re-sync right away. No other tests
	// can be run.
	if (!actual_id.is_valid())
		return resync_to_level1_element(wanted_id);

	// Easiest case: next level 1 element following the previous one
	// without any element inbetween.
	if ((wanted_id == actual_id.value) || ((0 == wanted_id) && (is_level1_element_id(actual_id) || is_global_element_id(actual_id)))) {
		auto l1 = read_one_element();

		if (l1) {
			// If a specific level 1 is wanted, make sure it was actually
			// read. Otherwise try again.
			if (!wanted_id || (wanted_id == EbmlId(*l1).GetValue()))
				return l1;
			return read_next_level1_element(wanted_id);
		}
	}

	// If a specific level 1 is wanted then look for next ID by skipping
	// other level 1 or special elements. If that files fallback to a
	// byte-for-byte search for the ID.
	if ((0 != wanted_id) && (is_level1_element_id(actual_id) || is_global_element_id(actual_id))) {
		in.setFilePointer(search_start_pos);
		auto l1 = read_one_element();

		if (l1) {
			auto element_size = get_element_size(*l1);
			auto ok = (0 != element_size) && in.setFilePointer2(l1->GetElementPosition() + element_size);
			return ok ? read_next_level1_element(wanted_id) : nullptr;
		}
	}

	// Last case: no valid ID found. Try a byte-for-byte search for the
	// next wanted/level 1 ID. Also try to locate at least three valid
	// ID/sizes, not just one ID.
	in.setFilePointer(search_start_pos);
	return resync_to_level1_element(wanted_id);
}

std::shared_ptr<EbmlElement> KaxFile::read_one_element() {
	if (segment_end && (in.getFilePointer() >= segment_end))
		return nullptr;

	auto upper_lvl_el = 0;
	auto l1 = std::shared_ptr<EbmlElement>{es->FindNextElement(EBML_CLASS_CONTEXT(libmatroska::KaxSegment), upper_lvl_el, 0xFFFFFFFFL, true)};

	if (!l1)
		return {};

	auto callbacks = find_ebml_callbacks(EBML_INFO(libmatroska::KaxSegment), EbmlId(*l1));
	if (!callbacks)
		callbacks = &EBML_CLASS_CALLBACK(libmatroska::KaxSegment);

	auto l2 = static_cast<EbmlElement *>(nullptr);
	try {
		l1->Read(*es.get(), EBML_INFO_CONTEXT(*callbacks), upper_lvl_el, l2, true);
		if (upper_lvl_el && !found_in(*l1, l2))
			delete l2;

	} catch (std::runtime_error &e) {
		in.setFilePointer(l1->GetElementPosition() + 1);
		if (upper_lvl_el && !found_in(*l1, l2))
			delete l2;
		return {};
	}

	auto element_size = get_element_size(*l1);
	in.setFilePointer(l1->GetElementPosition() + element_size);

	return l1;
}

bool KaxFile::is_level1_element_id(vint id) {
	auto &context = EBML_CLASS_CONTEXT(libmatroska::KaxSegment);
	for (int segment_idx = 0, end = EBML_CTX_SIZE(context); end > segment_idx; ++segment_idx)
		if (EBML_ID_VALUE(EBML_CTX_IDX_ID(context, segment_idx)) == id.value)
			return true;

	return false;
}

bool KaxFile::is_global_element_id(vint id) {
	return (EBML_ID_VALUE(EBML_ID(EbmlVoid)) == id.value) || (EBML_ID_VALUE(EBML_ID(EbmlCrc32)) == id.value);
}

std::shared_ptr<EbmlElement> KaxFile::resync_to_level1_element(uint32_t wanted_id) {
	try {
		return resync_to_level1_element_internal(wanted_id);
	} catch (...) {
		return {};
	}
}

std::shared_ptr<EbmlElement> KaxFile::resync_to_level1_element_internal(uint32_t wanted_id) {
	if (segment_end && (in.getFilePointer() >= segment_end))
		return {};

	resynced = true;
	resync_start_pos = in.getFilePointer();

	auto actual_id = in.read_uint32_be();
	auto start_time = get_current_time_millis();

	while (in.getFilePointer() < file_size) {
		auto now = get_current_time_millis();
		if ((now - start_time) >= 10000) {
			start_time = now;
		}

		actual_id = (actual_id << 8) | in.read_uint8();

		if (((0 != wanted_id) && (wanted_id != actual_id)) || ((0 == wanted_id) && !is_level1_element_id(vint(actual_id, 4))))
			continue;

		auto current_start_pos = in.getFilePointer() - 4;
		auto element_pos = current_start_pos;
		auto num_headers = 1u;
		auto valid_unknown_size = false;

		try {
			for (auto idx = 0; 3 > idx; ++idx) {
				auto length = vint::read(in);

				if (length.is_unknown()) {
					valid_unknown_size = true;
					break;
				}

				if (!length.is_valid() || ((element_pos + length.value + length.coded_size + 2 * 4) >= file_size) || !in.setFilePointer2(element_pos + 4 + length.value + length.coded_size))
					break;

				element_pos = in.getFilePointer();
				auto next_id = in.read_uint32_be();

				if (((0 != wanted_id) && (wanted_id != next_id)) || ((0 == wanted_id) && !is_level1_element_id(vint(next_id, 4))))
					break;

				++num_headers;
			}
		} catch (...) {
		}

		if ((4 == num_headers) || valid_unknown_size) {
			in.setFilePointer(current_start_pos);
			return read_next_level1_element(wanted_id);
		}

		in.setFilePointer(current_start_pos + 4);
	}

	return {};
}

std::shared_ptr<libmatroska::KaxCluster> KaxFile::resync_to_cluster() {
	return std::static_pointer_cast<libmatroska::KaxCluster>(resync_to_level1_element(EBML_ID_VALUE(EBML_ID(libmatroska::KaxCluster))));
}

std::shared_ptr<libmatroska::KaxCluster> KaxFile::read_next_cluster() {
	return std::static_pointer_cast<libmatroska::KaxCluster>(read_next_level1_element(EBML_ID_VALUE(EBML_ID(libmatroska::KaxCluster))));
}

bool KaxFile::was_resynced() const {
	return resynced;
}

int64_t KaxFile::get_resync_start_pos() const {
	return resync_start_pos;
}

unsigned long KaxFile::get_element_size(EbmlElement &e) {
	auto m = dynamic_cast<EbmlMaster *>(&e);

	if (!m || e.IsFiniteSize())
		return e.GetSizeLength() + EBML_ID_LENGTH(static_cast<const EbmlId &>(e)) + e.GetSize();

	auto max_end_pos = e.GetElementPosition() + EBML_ID_LENGTH(static_cast<const EbmlId &>(e));
	for (int idx = 0, end = m->ListSize(); end > idx; ++idx)
		max_end_pos = std::max(max_end_pos, (*m)[idx]->GetElementPosition() + get_element_size(*(*m)[idx]));

	return max_end_pos - e.GetElementPosition();
}

void KaxFile::set_timestamp_scale(int64_t timestamp_scale) {
	timestamp_scale = timestamp_scale;
}

void KaxFile::set_last_timestamp(int64_t last_timestamp) {
	last_timestamp = last_timestamp;
}

void KaxFile::set_segment_end(EbmlElement const &segment) {
	segment_end = segment.IsFiniteSize() ? segment.GetElementPosition() + segment.HeadSize() + segment.GetSize() : in.get_size();
}

uint64_t KaxFile::get_segment_end() const {
	return segment_end;
}

EbmlCallbacks const *find_ebml_callbacks(EbmlCallbacks const &base, EbmlId const &id) {
	static std::unordered_map<uint32_t, EbmlCallbacks const *> s_cache;

	auto itr = s_cache.find(id.GetValue());
	if (itr != s_cache.end())
		return itr->second;

	auto result = do_find_ebml_callbacks(base, id);
	s_cache[id.GetValue()] = result;

	return result;
}

EbmlCallbacks const *do_find_ebml_callbacks(EbmlCallbacks const &base, EbmlId const &id) {
	const EbmlSemanticContext &context = EBML_INFO_CONTEXT(base);
	const EbmlCallbacks *result;
	size_t i;

	if (EBML_INFO_ID(base) == id)
		return &base;

	for (i = 0; i < EBML_CTX_SIZE(context); i++)
		if (id == EBML_CTX_IDX_ID(context, i))
			return &EBML_CTX_IDX_INFO(context, i);

	for (i = 0; i < EBML_CTX_SIZE(context); i++) {
		if (!(context != EBML_SEM_CONTEXT(EBML_CTX_IDX(context, i))))
			continue;
		result = do_find_ebml_callbacks(EBML_CTX_IDX_INFO(context, i), id);
		if (result)
			return result;
	}

	return nullptr;
}

bool found_in(EbmlElement &haystack, EbmlElement const *needle) {
	if (!needle)
		return false;

	if (needle == &haystack)
		return true;

	auto master = dynamic_cast<EbmlMaster *>(&haystack);
	if (!master)
		return false;

	for (auto &child : *master) {
		if (child == needle)
			return true;

		if (dynamic_cast<EbmlMaster *>(child) && found_in(*child, needle))
			return true;
	}

	return false;
}