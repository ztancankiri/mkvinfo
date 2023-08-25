#include "Traverser.h"

std::unordered_map<uint32_t, std::string> ms_names;

void traverse(std::string filename, std::string &result) {
	result = "<root>";

	EBMLPointer pointer;
	pointer.source_file_name = filename;
	EBMLPointer *p = &pointer;

	p->in = std::make_shared<EBMLCallback>(p->source_file_name.c_str());

	p->file_size = p->in->get_size();
	p->es = std::make_shared<EbmlStream>(*p->in);

	// Find the EbmlHead element. Must be the first one.
	auto l0 = std::shared_ptr<libebml::EbmlElement>{p->es->FindNextID(EBML_INFO(EbmlHead), 0xFFFFFFFFL)};

	int upper_lvl_el = 0;
	EbmlElement *element_found = nullptr;

	// read_master
	EbmlMaster *m = static_cast<EbmlMaster *>(l0.get());
	m->Read(*p->es, EBML_CONTEXT(l0), upper_lvl_el, element_found, true);
	if (m->ListSize() == 0)
		return;
	std::sort(m->begin(), m->end(), [](auto const *a, auto const *b) { return a->GetElementPosition() < b->GetElementPosition(); });
	delete element_found;

	// handle_elements_generic
	handle_elements_generic(p, *l0, result);
	l0->SkipData(*p->es, EBML_CONTEXT(l0));

	while (true) {
		l0 = std::shared_ptr<libebml::EbmlElement>{p->es->FindNextID(EBML_INFO(KaxSegment), 0xFFFFFFFFFFFFFFFFLL)};
		if (!l0)
			break;

		if (!Is<KaxSegment>(*l0)) {
			handle_elements_generic(p, *l0, result);
			l0->SkipData(*p->es, EBML_CONTEXT(l0));
			continue;
		}

		handle_segment(p, l0.get(), result);
		l0->SkipData(*p->es, EBML_CONTEXT(l0));
	}

	std::string c_key = p->key_stack.top();
	p->key_stack.pop();
	result += "</" + c_key + ">";
	result += "</root>";
}

void handle_elements_generic(EBMLPointer *p, EbmlElement &e, std::string &result) {
	// run_generic_pre_processors
	auto is_dummy = !!dynamic_cast<EbmlDummy *>(&e);

	if (!is_dummy) {
		auto pre_processor = p->custom_element_pre_processors.find(EbmlId(e).GetValue());
		if (pre_processor != p->custom_element_pre_processors.end())
			if (!pre_processor->second(e))
				return;
	}

	// ui_show_element
	ui_show_element(p, e, result);

	if (dynamic_cast<EbmlMaster *>(&e)) {
		++p->level;

		for (auto child : static_cast<EbmlMaster &>(e)) {
			handle_elements_generic(p, *child, result);
		}

		--p->level;
	}

	// run_generic_post_processors
	if (!!dynamic_cast<EbmlDummy *>(&e))
		return;

	auto post_processor = p->custom_element_post_processors.find(EbmlId(e).GetValue());
	if (post_processor != p->custom_element_post_processors.end())
		post_processor->second(e);
}

void ui_show_element(EBMLPointer *p, EbmlElement &e, std::string &result) {
	auto text = get(e);

	if (text.find("EBML_void") != std::string::npos) {
		return;
	}

	if (p->prev_level > p->level) {
		int diff = p->prev_level - p->level;
		for (int i = 0; i < diff; i++) {
			std::string c_key = p->key_stack.top();
			p->key_stack.pop();
			result += "</" + c_key + ">";
		}
	}

	if (text.find("Cluster") != std::string::npos) {
		return;
	}

	if (text.empty()) {
		text = "Unknown";
	} else if (dynamic_cast<EbmlDummy *>(&e)) {
		text = "Forbidden";
	} else {
		auto value = format_element_value(p, e);
		if (!value.empty()) {
			result += "<" + text + ">" + value + "</" + text + ">";
		} else {
			result += "<" + text + ">";
			p->key_stack.push(text);
		}
	}

	p->prev_level = p->level;
}

void handle_segment(EBMLPointer *p, EbmlElement *l0, std::string &result) {
	ui_show_element(p, *l0, result);

	auto l1 = std::shared_ptr<EbmlElement>{};
	auto kax_file = std::make_shared<KaxFile>(*p->in);
	p->level = 1;

	kax_file->set_segment_end(*l0);
	kax_file->set_timestamp_scale(-1);

	while ((l1 = kax_file->read_next_level1_element())) {
		if (Is<KaxCluster>(*l1) && !p->continue_at_cluster && !p->show_summary) {
			return ui_show_element(p, *l1, result);
		} else
			handle_elements_generic(p, *l1, result);

		if (!p->in->setFilePointer2(l1->GetElementPosition() + kax_file->get_element_size(*l1)))
			break;

		auto in_parent = !l0->IsFiniteSize() || (p->in->getFilePointer() < (l0->GetElementPosition() + l0->HeadSize() + l0->GetSize()));
		if (!in_parent)
			break;
	}
}

std::string format_element_value(EBMLPointer *p, EbmlElement &e) {
	auto formatter = p->custom_element_value_formatters.find(EbmlId(e).GetValue());

	if (formatter == p->custom_element_value_formatters.end() || dynamic_cast<EbmlDummy *>(&e)) {
		if (Is<EbmlVoid>(e)) {
			if (e.IsFiniteSize())
				return fmt::format(Y("size {0}"), e.GetSize());
			return Y("size unknown");
		}

		if (Is<EbmlCrc32>(e))
			return fmt::format("0x{0:08x}", static_cast<EbmlCrc32 &>(e).GetCrc32());

		if (dynamic_cast<EbmlUInteger *>(&e))
			return fmt::to_string(static_cast<EbmlUInteger &>(e).GetValue());

		if (dynamic_cast<EbmlSInteger *>(&e))
			return fmt::to_string(static_cast<EbmlSInteger &>(e).GetValue());

		if (dynamic_cast<EbmlString *>(&e))
			return static_cast<EbmlString &>(e).GetValue();

		if (dynamic_cast<EbmlUnicodeString *>(&e))
			return static_cast<EbmlUnicodeString &>(e).GetValueUTF8();

		if (dynamic_cast<EbmlBinary *>(&e)) {
			EbmlBinary &bin = static_cast<EbmlBinary &>(e);
			auto len = std::min<std::size_t>(p->hexdump_max_size, bin.GetSize());
			auto const b = bin.GetBuffer();
			std::ostringstream oss;
			oss << " (length " << bin.GetSize() << "), data: " << to_hex(b, len, true);
			std::string result = oss.str();

			if (len < bin.GetSize())
				result += u8"…";

			strip(result);
			return result;
		}

		if (dynamic_cast<EbmlDate *>(&e))
			return "Date not implemented";

		if (dynamic_cast<EbmlMaster *>(&e))
			return {};

		if (dynamic_cast<EbmlFloat *>(&e))
			return std::to_string(static_cast<EbmlFloat &>(e).GetValue());
	} else {
		return formatter->second(e);
	}

	return "";
}

std::string get(libebml::EbmlElement const &elt) {
	uint32_t id = libebml::EbmlId(elt).GetValue();

	if (ms_names.empty()) {
		auto add = [](libebml::EbmlId const &id, char const *description) {
			ms_names.insert({id.GetValue(), description});
		};

		add(EBML_ID(EbmlHead), Y("EBML_head"));
		add(EBML_ID(EVersion), Y("EBML_version"));
		add(EBML_ID(EReadVersion), Y("EBML_read_version"));
		add(EBML_ID(EMaxIdLength), Y("Maximum_EBML_ID_length"));
		add(EBML_ID(EMaxSizeLength), Y("Maximum_EBML_size_length"));
		add(EBML_ID(EDocType), Y("Document_type"));
		add(EBML_ID(EDocTypeVersion), Y("Document_type_version"));
		add(EBML_ID(EDocTypeReadVersion), Y("Document_type_read_version"));
		add(EBML_ID(EbmlVoid), Y("EBML_void"));
		add(EBML_ID(EbmlCrc32), Y("EBML_CRC-32"));

		add(EBML_ID(KaxSegment), Y("Segment"));
		add(EBML_ID(KaxInfo), Y("Segment_information"));
		add(EBML_ID(KaxTimecodeScale), Y("Timestamp_scale"));
		add(EBML_ID(KaxDuration), Y("Duration"));
		add(EBML_ID(KaxMuxingApp), Y("Multiplexing_application"));
		add(EBML_ID(KaxWritingApp), Y("Writing_application"));
		add(EBML_ID(KaxDateUTC), Y("Date"));
		add(EBML_ID(KaxSegmentUID), Y("Segment_UID"));
		add(EBML_ID(KaxSegmentFamily), Y("Family_UID"));
		add(EBML_ID(KaxPrevUID), Y("Previous_segment_UID"));
		add(EBML_ID(KaxPrevFilename), Y("Previous_filename"));
		add(EBML_ID(KaxNextUID), Y("Next_segment_UID"));
		add(EBML_ID(KaxNextFilename), Y("Next_filename"));
		add(EBML_ID(KaxSegmentFilename), Y("Segment_filename"));
		add(EBML_ID(KaxTitle), Y("Title"));

		add(EBML_ID(KaxChapterTranslate), Y("Chapter_translate"));
		add(EBML_ID(KaxChapterTranslateEditionUID), Y("Chapter_translate_edition_UID"));
		add(EBML_ID(KaxChapterTranslateCodec), Y("Chapter_translate_codec"));
		add(EBML_ID(KaxChapterTranslateID), Y("Chapter_translate_ID"));

		add(EBML_ID(KaxTrackAudio), Y("Audio_track"));
		add(EBML_ID(KaxAudioSamplingFreq), Y("Sampling_frequency"));
		add(EBML_ID(KaxAudioOutputSamplingFreq), Y("Output_sampling_frequency"));
		add(EBML_ID(KaxAudioChannels), Y("Channels"));
		add(EBML_ID(KaxAudioPosition), Y("Channel_positions"));
		add(EBML_ID(KaxAudioBitDepth), Y("Bit_depth"));
		add(EBML_ID(KaxEmphasis), Y("Emphasis"));

		add(EBML_ID(KaxVideoColourMasterMeta), Y("Video_color_mastering_metadata"));
		add(EBML_ID(KaxVideoRChromaX), Y("Red_color_coordinate_x"));
		add(EBML_ID(KaxVideoRChromaY), Y("Red_color_coordinate_y"));
		add(EBML_ID(KaxVideoGChromaX), Y("Green_color_coordinate_x"));
		add(EBML_ID(KaxVideoGChromaY), Y("Green_color_coordinate_y"));
		add(EBML_ID(KaxVideoBChromaX), Y("Blue_color_coordinate_x"));
		add(EBML_ID(KaxVideoBChromaY), Y("Blue_color_coordinate_y"));
		add(EBML_ID(KaxVideoWhitePointChromaX), Y("White_color_coordinate_x"));
		add(EBML_ID(KaxVideoWhitePointChromaY), Y("White_color_coordinate_y"));
		add(EBML_ID(KaxVideoLuminanceMax), Y("Maximum_luminance"));
		add(EBML_ID(KaxVideoLuminanceMin), Y("Minimum_luminance"));

		add(EBML_ID(KaxVideoColour), Y("Video_color_information"));
		add(EBML_ID(KaxVideoColourMatrix), Y("Color_matrix_coefficients"));
		add(EBML_ID(KaxVideoBitsPerChannel), Y("Bits_per_channel"));
		add(EBML_ID(KaxVideoChromaSubsampHorz), Y("Horizontal_chroma_subsample"));
		add(EBML_ID(KaxVideoChromaSubsampVert), Y("Vertical_chroma_subsample"));
		add(EBML_ID(KaxVideoCbSubsampHorz), Y("Horizontal_Cb_subsample"));
		add(EBML_ID(KaxVideoCbSubsampVert), Y("Vertical_Cb_subsample"));
		add(EBML_ID(KaxVideoChromaSitHorz), Y("Horizontal_chroma_siting"));
		add(EBML_ID(KaxVideoChromaSitVert), Y("Vertical_chroma_siting"));
		add(EBML_ID(KaxVideoColourRange), Y("Color_range"));
		add(EBML_ID(KaxVideoColourTransferCharacter), Y("Color_transfer"));
		add(EBML_ID(KaxVideoColourPrimaries), Y("Color_primaries"));
		add(EBML_ID(KaxVideoColourMaxCLL), Y("Maximum_content_light"));
		add(EBML_ID(KaxVideoColourMaxFALL), Y("Maximum_frame_light"));

		add(EBML_ID(KaxVideoProjection), Y("Video_projection"));
		add(EBML_ID(KaxVideoProjectionType), Y("Projection_type"));
		add(EBML_ID(KaxVideoProjectionPrivate), Y("Projections_private_data"));
		add(EBML_ID(KaxVideoProjectionPoseYaw), Y("Projections_yaw_rotation"));
		add(EBML_ID(KaxVideoProjectionPosePitch), Y("Projections_pitch_rotation"));
		add(EBML_ID(KaxVideoProjectionPoseRoll), Y("Projections_roll_rotation"));

		add(EBML_ID(KaxTrackVideo), Y("Video_track"));
		add(EBML_ID(KaxVideoPixelWidth), Y("Pixel_width"));
		add(EBML_ID(KaxVideoPixelHeight), Y("Pixel_height"));
		add(EBML_ID(KaxVideoDisplayWidth), Y("Display_width"));
		add(EBML_ID(KaxVideoDisplayHeight), Y("Display_height"));
		add(EBML_ID(KaxVideoPixelCropLeft), Y("Pixel_crop_left"));
		add(EBML_ID(KaxVideoPixelCropTop), Y("Pixel_crop_top"));
		add(EBML_ID(KaxVideoPixelCropRight), Y("Pixel_crop_right"));
		add(EBML_ID(KaxVideoPixelCropBottom), Y("Pixel_crop_bottom"));
		add(EBML_ID(KaxVideoDisplayUnit), Y("Display_unit"));
		add(EBML_ID(KaxVideoGamma), Y("Gamma"));
		add(EBML_ID(KaxVideoFlagInterlaced), Y("Interlaced"));
		add(EBML_ID(KaxVideoFieldOrder), Y("Field_order"));
		add(EBML_ID(KaxVideoStereoMode), Y("Stereo_mode"));
		add(EBML_ID(KaxVideoAspectRatio), Y("Aspect_ratio_type"));
		add(EBML_ID(KaxVideoColourSpace), Y("Color_space"));
		add(EBML_ID(KaxVideoFrameRate), Y("Frame_rate"));
		add(EBML_ID(KaxOldStereoMode), Y("Stereo_mode_(deprecated_element)"));

		add(EBML_ID(KaxContentEncodings), Y("Content_encodings"));
		add(EBML_ID(KaxContentEncoding), Y("Content_encoding"));
		add(EBML_ID(KaxContentEncodingOrder), Y("Order"));
		add(EBML_ID(KaxContentEncodingScope), Y("Scope"));
		add(EBML_ID(KaxContentEncodingType), Y("Type"));
		add(EBML_ID(KaxContentCompression), Y("Content_compression"));
		add(EBML_ID(KaxContentCompAlgo), Y("Algorithm"));
		add(EBML_ID(KaxContentCompSettings), Y("Settings"));
		add(EBML_ID(KaxContentEncryption), Y("Content_encryption"));
		add(EBML_ID(KaxContentEncAlgo), Y("Encryption_algorithm"));
		add(EBML_ID(KaxContentEncKeyID), Y("Encryption_key_ID"));
		add(EBML_ID(KaxContentSigAlgo), Y("Signature_algorithm"));
		add(EBML_ID(KaxContentSigHashAlgo), Y("Signature_hash_algorithm"));
		add(EBML_ID(KaxContentSigKeyID), Y("Signature_key_ID"));
		add(EBML_ID(KaxContentSignature), Y("Signature"));

		add(EBML_ID(KaxBlockAdditionMapping), Y("Block_addition_mapping"));
		add(EBML_ID(KaxBlockAddIDExtraData), Y("Block_addition_ID_extra_data"));
		add(EBML_ID(KaxBlockAddIDName), Y("Block_addition_ID_name"));
		add(EBML_ID(KaxBlockAddIDType), Y("Block_addition_ID_type"));
		add(EBML_ID(KaxBlockAddIDValue), Y("Block_addition_ID_value"));

		add(EBML_ID(KaxTracks), Y("Tracks"));
		add(EBML_ID(KaxTrackEntry), Y("Track"));
		add(EBML_ID(KaxTrackNumber), Y("Track_number"));
		add(EBML_ID(KaxTrackUID), Y("Track_UID"));
		add(EBML_ID(KaxTrackType), Y("Track_type"));
		add(EBML_ID(KaxTrackFlagEnabled), Y("Enabled_flag"));
		add(EBML_ID(KaxTrackName), Y("Name"));
		add(EBML_ID(KaxCodecID), Y("Codec_ID"));
		add(EBML_ID(KaxCodecPrivate), Y("Codecs_private_data"));
		add(EBML_ID(KaxCodecName), Y("Codec_name"));
		add(EBML_ID(KaxCodecDelay), Y("Codec-inherent_delay"));
		add(EBML_ID(KaxCodecSettings), Y("Codec_settings"));
		add(EBML_ID(KaxCodecInfoURL), Y("Codec_info_URL"));
		add(EBML_ID(KaxCodecDownloadURL), Y("Codec_download_URL"));
		add(EBML_ID(KaxCodecDecodeAll), Y("Codec_decode_all"));
		add(EBML_ID(KaxTrackOverlay), Y("Track_overlay"));
		add(EBML_ID(KaxTrackMinCache), Y("Minimum_cache"));
		add(EBML_ID(KaxTrackMaxCache), Y("Maximum_cache"));
		add(EBML_ID(KaxTrackDefaultDuration), Y("Default_duration"));
		add(EBML_ID(KaxTrackFlagLacing), Y("Lacing_flag"));
		add(EBML_ID(KaxTrackFlagDefault), Y("Default_track_flag"));
		add(EBML_ID(KaxTrackFlagForced), Y("Forced_display_flag"));
		add(EBML_ID(KaxTrackLanguage), Y("Language"));
		add(EBML_ID(KaxLanguageIETF), Y("Language_(IETF_BCP_47)"));
		add(EBML_ID(KaxTrackTimecodeScale), Y("Timestamp_scale"));
		add(EBML_ID(KaxMaxBlockAdditionID), Y("Maximum_block_additional_ID"));
		add(EBML_ID(KaxContentEncodings), Y("Content_encodings"));
		add(EBML_ID(KaxSeekPreRoll), Y("Seek_pre-roll"));
		add(EBML_ID(KaxFlagHearingImpaired), Y("Hearing_impaired_flag"));
		add(EBML_ID(KaxFlagVisualImpaired), Y("Visual_impaired_flag"));
		add(EBML_ID(KaxFlagTextDescriptions), Y("Text_descriptions_flag"));
		add(EBML_ID(KaxFlagOriginal), Y("Original_language_flag"));
		add(EBML_ID(KaxFlagCommentary), Y("Commentary_flag"));

		add(EBML_ID(KaxSeekHead), Y("Seek_head"));
		add(EBML_ID(KaxSeek), Y("Seek_entry"));
		add(EBML_ID(KaxSeekID), Y("Seek_ID"));
		add(EBML_ID(KaxSeekPosition), Y("Seek_position"));

		add(EBML_ID(KaxCues), Y("Cues"));
		add(EBML_ID(KaxCuePoint), Y("Cue_point"));
		add(EBML_ID(KaxCueTime), Y("Cue_time"));
		add(EBML_ID(KaxCueTrackPositions), Y("Cue_track_positions"));
		add(EBML_ID(KaxCueTrack), Y("Cue_track"));
		add(EBML_ID(KaxCueClusterPosition), Y("Cue_cluster_position"));
		add(EBML_ID(KaxCueRelativePosition), Y("Cue_relative_position"));
		add(EBML_ID(KaxCueDuration), Y("Cue_duration"));
		add(EBML_ID(KaxCueBlockNumber), Y("Cue_block_number"));
		add(EBML_ID(KaxCueCodecState), Y("Cue_codec_state"));
		add(EBML_ID(KaxCueReference), Y("Cue_reference"));
		add(EBML_ID(KaxCueRefTime), Y("Cue_ref_time"));
		add(EBML_ID(KaxCueRefCluster), Y("Cue_ref_cluster"));
		add(EBML_ID(KaxCueRefNumber), Y("Cue_ref_number"));
		add(EBML_ID(KaxCueRefCodecState), Y("Cue_ref_codec_state"));

		add(EBML_ID(KaxAttachments), Y("Attachments"));
		add(EBML_ID(KaxAttached), Y("Attached"));
		add(EBML_ID(KaxFileDescription), Y("File_description"));
		add(EBML_ID(KaxFileName), Y("File_name"));
		add(EBML_ID(KaxMimeType), Y("MIME_type"));
		add(EBML_ID(KaxFileData), Y("File_data"));
		add(EBML_ID(KaxFileUID), Y("File_UID"));

		add(EBML_ID(KaxClusterSilentTracks), Y("Silent_tracks"));
		add(EBML_ID(KaxClusterSilentTrackNumber), Y("Silent_track_number"));

		add(EBML_ID(KaxBlockGroup), Y("Block_group"));
		add(EBML_ID(KaxBlock), Y("Block"));
		add(EBML_ID(KaxBlockDuration), Y("Block_duration"));
		add(EBML_ID(KaxReferenceBlock), Y("Reference_block"));
		add(EBML_ID(KaxReferencePriority), Y("Reference_priority"));
		add(EBML_ID(KaxBlockVirtual), Y("Block_virtual"));
		add(EBML_ID(KaxReferenceVirtual), Y("Reference_virtual"));
		add(EBML_ID(KaxCodecState), Y("Codec_state"));
		add(EBML_ID(KaxBlockAddID), Y("Block_additional_ID"));
		add(EBML_ID(KaxBlockAdditional), Y("Block_additional"));
		add(EBML_ID(KaxSliceLaceNumber), Y("Lace_number"));
		add(EBML_ID(KaxSliceFrameNumber), Y("Frame_number"));
		add(EBML_ID(KaxSliceDelay), Y("Delay"));
		add(EBML_ID(KaxSliceDuration), Y("Duration"));
		add(EBML_ID(KaxSliceBlockAddID), Y("Block_additional_ID"));
		add(EBML_ID(KaxDiscardPadding), Y("Discard_padding"));
		add(EBML_ID(KaxBlockAdditions), Y("Additions"));
		add(EBML_ID(KaxBlockMore), Y("More"));
		add(EBML_ID(KaxSlices), Y("Slices"));
		add(EBML_ID(KaxTimeSlice), Y("Time_slice"));
		add(EBML_ID(KaxSimpleBlock), Y("Simple_block"));

		add(EBML_ID(KaxCluster), Y("Cluster"));
		add(EBML_ID(KaxClusterTimecode), Y("Cluster_timestamp"));
		add(EBML_ID(KaxClusterPosition), Y("Cluster_position"));
		add(EBML_ID(KaxClusterPrevSize), Y("Cluster_previous_size"));

		add(EBML_ID(KaxChapters), Y("Chapters"));
		add(EBML_ID(KaxEditionEntry), Y("Edition_entry"));
		add(EBML_ID(KaxEditionUID), Y("Edition_UID"));
		add(EBML_ID(KaxEditionFlagHidden), Y("Edition_flag_hidden"));
		add(EBML_ID(KaxEditionFlagDefault), Y("Edition_flag_default"));
		add(EBML_ID(KaxEditionFlagOrdered), Y("Edition_flag_ordered"));
		add(EBML_ID(KaxEditionDisplay), Y("Edition_display"));
		add(EBML_ID(KaxEditionString), Y("Edition_string"));
		add(EBML_ID(KaxEditionLanguageIETF), Y("Edition_language_(IETF_BCP_47)"));
		add(EBML_ID(KaxChapterAtom), Y("Chapter_atom"));
		add(EBML_ID(KaxChapterUID), Y("Chapter_UID"));
		add(EBML_ID(KaxChapterStringUID), Y("Chapter_string_UID"));
		add(EBML_ID(KaxChapterTimeStart), Y("Chapter_time_start"));
		add(EBML_ID(KaxChapterTimeEnd), Y("Chapter_time_end"));
		add(EBML_ID(KaxChapterFlagHidden), Y("Chapter_flag_hidden"));
		add(EBML_ID(KaxChapterFlagEnabled), Y("Chapter_flag_enabled"));
		add(EBML_ID(KaxChapterSegmentUID), Y("Chapter_segment_UID"));
		add(EBML_ID(KaxChapterSegmentEditionUID), Y("Chapter_segment_edition_UID"));
		add(EBML_ID(KaxChapterPhysicalEquiv), Y("Chapter_physical_equivalent"));
		add(EBML_ID(KaxChapterTrack), Y("Chapter_track"));
		add(EBML_ID(KaxChapterTrackNumber), Y("Chapter_track_number"));
		add(EBML_ID(KaxChapterDisplay), Y("Chapter_display"));
		add(EBML_ID(KaxChapterString), Y("Chapter_string"));
		add(EBML_ID(KaxChapterLanguage), Y("Chapter_language"));
		add(EBML_ID(KaxChapLanguageIETF), Y("Chapter_language_(IETF_BCP_47)"));
		add(EBML_ID(KaxChapterCountry), Y("Chapter_country"));
		add(EBML_ID(KaxChapterProcess), Y("Chapter_process"));
		add(EBML_ID(KaxChapterProcessCodecID), Y("Chapter_process_codec_ID"));
		add(EBML_ID(KaxChapterProcessPrivate), Y("Chapter_process_private"));
		add(EBML_ID(KaxChapterProcessCommand), Y("Chapter_process_command"));
		add(EBML_ID(KaxChapterProcessTime), Y("Chapter_process_time"));
		add(EBML_ID(KaxChapterProcessData), Y("Chapter_process_data"));
		add(EBML_ID(KaxChapterSkipType), Y("Chapter_skip_type"));

		add(EBML_ID(KaxTags), Y("Tags"));
		add(EBML_ID(KaxTag), Y("Tag"));
		add(EBML_ID(KaxTagTargets), Y("Targets"));
		add(EBML_ID(KaxTagTrackUID), Y("Track_UID"));
		add(EBML_ID(KaxTagEditionUID), Y("Edition_UID"));
		add(EBML_ID(KaxTagChapterUID), Y("Chapter_UID"));
		add(EBML_ID(KaxTagAttachmentUID), Y("Attachment_UID"));
		add(EBML_ID(KaxTagTargetType), Y("Target_type"));
		add(EBML_ID(KaxTagTargetTypeValue), Y("Target_type_value"));
		add(EBML_ID(KaxTagSimple), Y("Simple"));
		add(EBML_ID(KaxTagName), Y("Name"));
		add(EBML_ID(KaxTagString), Y("String"));
		add(EBML_ID(KaxTagBinary), Y("Binary"));
		add(EBML_ID(KaxTagLangue), Y("Tag_language"));
		add(EBML_ID(KaxTagLanguageIETF), Y("Tag_language_(IETF_BCP_47)"));
		add(EBML_ID(KaxTagDefault), Y("Default_language"));
		add(EBML_ID(KaxTagDefaultBogus), Y("Default_language_(bogus_element_with_invalid_ID)"));
	}

	auto itr = ms_names.find(id);

	if (itr != ms_names.end())
		return itr->second;

	return {};
}