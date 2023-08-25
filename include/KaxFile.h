#ifndef KAX_FILE_H
#define KAX_FILE_H

#include <ebml/EbmlElement.h>
#include <ebml/EbmlId.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxSegment.h>

#include <memory>
#include <unordered_map>

#include "EBMLCallback.h"
#include "vint.h"

class KaxFile {
   protected:
	EBMLCallback &in;
	bool resynced{true};
	uint64_t resync_start_pos, file_size, segment_end;
	int64_t timestamp_scale, last_timestamp;
	std::shared_ptr<libebml::EbmlStream> es;

   public:
	KaxFile(EBMLCallback &in);
	virtual ~KaxFile() = default;

	virtual bool was_resynced() const;
	virtual int64_t get_resync_start_pos() const;

	virtual std::shared_ptr<libebml::EbmlElement> read_next_level1_element(uint32_t wanted_id = 0);
	virtual std::shared_ptr<libmatroska::KaxCluster> read_next_cluster();

	virtual std::shared_ptr<libebml::EbmlElement> resync_to_level1_element(uint32_t wanted_id = 0);
	virtual std::shared_ptr<libmatroska::KaxCluster> resync_to_cluster();

	virtual void set_timestamp_scale(int64_t timestamp_scale);
	virtual void set_last_timestamp(int64_t last_timestamp);
	virtual void set_segment_end(libebml::EbmlElement const &segment);
	virtual uint64_t get_segment_end() const;

   protected:
	virtual std::shared_ptr<libebml::EbmlElement> read_one_element();

	virtual std::shared_ptr<libebml::EbmlElement> read_next_level1_element_internal(uint32_t wanted_id = 0);
	virtual std::shared_ptr<libebml::EbmlElement> resync_to_level1_element_internal(uint32_t wanted_id = 0);

   public:
	static bool is_level1_element_id(vint id);
	static bool is_global_element_id(vint id);
	static unsigned long get_element_size(libebml::EbmlElement &e);
};

EbmlCallbacks const *find_ebml_callbacks(EbmlCallbacks const &base, EbmlId const &id);
EbmlCallbacks const *do_find_ebml_callbacks(EbmlCallbacks const &base, EbmlId const &id);
bool found_in(EbmlElement &haystack, EbmlElement const *needle);

#endif