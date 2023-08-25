#include <Traverser.h>
#include <Utils.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;

std::string info(std::string filename) {
	std::string result;
	traverse(filename, result);

	pugi::xml_document xmlDoc;
	if (xmlDoc.load_string(result.c_str())) {
		json rootJsonObj;
		xmlToJsonObject(xmlDoc.child("root"), rootJsonObj);
		recursiveIteration(rootJsonObj);
		return rootJsonObj.dump(4);
	}

	return "";
}

PYBIND11_MODULE(mkvinfo, module_handle) {
	module_handle.doc() = "mkvinfo";
	module_handle.def("info", &info);
}