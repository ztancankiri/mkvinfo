#ifndef TRAVERSER_H
#define TRAVERSER_H

#include <ebml/EbmlDummy.h>
#include <ebml/EbmlElement.h>
#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlVersion.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxBlockData.h>
#include <matroska/KaxCuesData.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSemantic.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "EBMLCallback.h"
#include "EBMLPointer.h"
#include "KaxFile.h"
#include "Utils.h"
#include "vint.h"

using namespace libebml;
using namespace libmatroska;

extern std::unordered_map<uint32_t, std::string> ms_names;

void traverse(std::string filename, std::string &result);

void handle_elements_generic(EBMLPointer *p, EbmlElement &e, std::string &result);
void ui_show_element(EBMLPointer *p, EbmlElement &e, std::string &result);
void handle_segment(EBMLPointer *p, EbmlElement *l0, std::string &result);

std::string format_element_value(EBMLPointer *p, EbmlElement &e);
std::string get(libebml::EbmlElement const &elt);

template <typename T>
bool Is(libebml::EbmlElement const &e) {
	return libebml::EbmlId(e) == EBML_ID(T);
}

#endif	// TRAVERSER_H