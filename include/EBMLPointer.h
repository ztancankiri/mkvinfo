#ifndef EBML_POINTER_H
#define EBML_POINTER_H

#include <ebml/EbmlElement.h>
#include <ebml/EbmlStream.h>
#include <matroska/KaxCluster.h>

#include <functional>
#include <memory>
#include <optional>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "EBMLCallback.h"
#include "Utils.h"

struct track_t {
	uint64_t tnum{}, tuid{};
	char type{' '};
	int64_t default_duration{};
	std::size_t mkvmerge_track_id{};
	std::string codec_id, fourcc;
};

struct track_info_t {
	int64_t size{}, blocks{}, blocks_by_ref_num[3]{0, 0, 0}, add_duration_for_n_packets{};
	std::optional<int64_t> min_timestamp, max_timestamp;
};

class EBMLPointer {
   public:
	std::vector<std::shared_ptr<track_t>> tracks;
	std::unordered_map<unsigned int, std::shared_ptr<track_t>> tracks_by_number;
	std::unordered_map<unsigned int, track_info_t> track_info;
	std::vector<std::shared_ptr<EbmlElement>> retained_elements;
	std::unordered_map<EbmlElement *, std::shared_ptr<track_t>> track_by_element;
	uint64_t ts_scale{TIMESTAMP_SCALE}, file_size{};
	std::size_t mkvmerge_track_id{};
	std::shared_ptr<EbmlStream> es;
	std::shared_ptr<EBMLCallback> in;
	std::string source_file_name, destination_file_name;
	int level{};
	std::vector<std::string> summary;
	std::shared_ptr<track_t> track;
	libmatroska::KaxCluster *cluster{};
	std::vector<int> frame_sizes;
	std::vector<uint32_t> frame_adlers;
	std::vector<std::string> frame_hexdumps;
	int64_t num_references{}, lf_timestamp{}, lf_tnum{};
	std::optional<int64_t> block_duration;

	bool use_gui{}, calc_checksums{}, show_summary{}, show_hexdump{}, show_size{}, show_positions{}, show_track_info{}, hex_positions{}, retain_elements{}, continue_at_cluster{}, show_all_elements{};
	int hexdump_max_size{};

	bool abort{};

	std::unordered_map<uint32_t, std::function<std::string(EbmlElement &)>> custom_element_value_formatters;
	std::unordered_map<uint32_t, std::function<bool(EbmlElement &)>> custom_element_pre_processors;
	std::unordered_map<uint32_t, std::function<void(EbmlElement &)>> custom_element_post_processors;

   public:
	EBMLPointer() = default;
	virtual ~EBMLPointer() = default;

	int prev_level{};

	std::stack<std::string> key_stack;
};

#endif
