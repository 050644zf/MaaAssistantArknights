#include "Configer.h"

#include <fstream>
#include <sstream>
#include <algorithm>

#include <json.h>
#include "Logger.hpp"

using namespace asst;

//Configer::Configer(const Configer& rhs)
//	: m_version(rhs.m_version),
//	m_options(rhs.m_options),
//	m_tasks(rhs.m_tasks),
//	m_handles(rhs.m_handles)
//{
//}
//
//Configer::Configer(Configer&& rhs) noexcept
//	: m_version(std::move(rhs.m_version)),
//	m_options(std::move(rhs.m_options)),
//	m_tasks(std::move(rhs.m_tasks)),
//	m_handles(std::move(rhs.m_handles))
//{
//}

bool Configer::load(const std::string& filename)
{
	DebugTraceFunction;
	DebugTrace("Configer::load | ", filename);
	
	Configer temp;
	if (temp._load(filename)) {
		*this = std::move(temp);
		return true;
	}
	else {
		return false;
	}
}

bool Configer::set_param(const std::string& type, const std::string& param, const std::string& value)
{
	// ��ʱֻ�õ�����Щ���ܵĲ���̫���ˣ�����Ҫ��ɶ�ټ���
	if (type == "task.type") {
		if (m_all_tasks_info.find(param) == m_all_tasks_info.cend()) {
			return false;
		}
		auto task_info_ptr = m_all_tasks_info[param];
		std::string type = value;
		std::transform(type.begin(), type.end(), type.begin(), std::tolower);
		if (type == "clickself") {
			task_info_ptr->type = ProcessTaskType::ClickSelf;
		}
		else if (type == "clickrand") {
			task_info_ptr->type = ProcessTaskType::ClickRand;
		}
		else if (type == "donothing" || type.empty()) {
			task_info_ptr->type = ProcessTaskType::DoNothing;
		}
		else if (type == "stop") {
			task_info_ptr->type = ProcessTaskType::Stop;
		}
		else if (type == "clickrect") {
			task_info_ptr->type = ProcessTaskType::ClickRect;
		}
		else {
			DebugTraceError("Task", param, "'s type error:", type);
			return false;
		}
	}
	else if (type == "task.maxTimes") {
		if (m_all_tasks_info.find(param) == m_all_tasks_info.cend()) {
			return false;
		}
		m_all_tasks_info[param]->max_times = std::stoi(value);
	}
	return true;
}

bool asst::Configer::_load(const std::string& filename)
{
	std::ifstream ifs(filename, std::ios::in);
	if (!ifs.is_open()) {
		return false;
	}
	std::stringstream iss;
	iss << ifs.rdbuf();
	ifs.close();
	std::string content(iss.str());

	auto&& ret = json::parser::parse(content);
	if (!ret) {
		DebugTrace("parse error", content);
		return false;
	}

	json::value root = ret.value();
	try {
		m_version = root["version"].as_string();

		json::value& options_json = root["options"];
		{
			m_options.task_identify_delay = options_json["taskIdentifyDelay"].as_integer();
			m_options.task_control_delay = options_json["taskControlDelay"].as_integer();
			m_options.identify_cache = options_json["identifyCache"].as_boolean();
			m_options.control_delay_lower = options_json["controlDelayRange"][0].as_integer();
			m_options.control_delay_upper = options_json["controlDelayRange"][1].as_integer();
			m_options.print_window = options_json["printWindow"].as_boolean();
			m_options.print_window_delay = options_json["printWindowDelay"].as_integer();
			m_options.print_window_crop_offset = options_json["printWindowCropOffset"].as_integer();
			m_options.ocr_gpu_index = options_json["ocrGpuIndex"].as_integer();
			m_options.ocr_thread_number = options_json["ocrThreadNumber"].as_integer();
		}
		DebugTrace("Options", Utf8ToGbk(options_json.to_string()));

		for (auto&& [name, task_json] : root["tasks"].as_object()) {
			std::string algorithm_str = task_json.get("algorithm", "matchtemplate");
			std::transform(algorithm_str.begin(), algorithm_str.end(), algorithm_str.begin(), std::tolower);
			AlgorithmType algorithm = AlgorithmType::Invaild;
			if (algorithm_str == "matchtemplate") {
				algorithm = AlgorithmType::MatchTemplate;
			}
			else if (algorithm_str == "justreturn") {
				algorithm = AlgorithmType::JustReturn;
			}
			else if (algorithm_str == "ocrdetect") {
				algorithm = AlgorithmType::OcrDetect;
			}
			//else if (algorithm_str == "comparehist") {} // CompareHist��MatchTemplate�������㷨����Ӧ��Ϊ���������ò�������
			else {
				DebugTraceError("Algorithm error:", algorithm_str);
				return false;
			}

			std::shared_ptr<TaskInfo> task_info_ptr = nullptr;
			switch (algorithm) {
			case AlgorithmType::JustReturn:
				task_info_ptr = std::make_shared<TaskInfo>();
				break;
			case AlgorithmType::MatchTemplate:
			{
				auto match_task_info_ptr = std::make_shared<MatchTaskInfo>();
				match_task_info_ptr->template_filename = task_json["template"].as_string();
				match_task_info_ptr->templ_threshold = task_json.get("templThreshold", Defaulttempl_threshold);
				match_task_info_ptr->hist_threshold = task_json.get("histThreshold", DefaultCachetempl_threshold);
				task_info_ptr = match_task_info_ptr;
			}
				break;
			case AlgorithmType::OcrDetect:
			{
				auto ocr_task_info_ptr = std::make_shared<OcrTaskInfo>();
				for (const json::value& text : task_json["text"].as_array())
				{
					ocr_task_info_ptr->text.emplace_back(text.as_string());
				}
				ocr_task_info_ptr->need_match = task_json.get("need_match", false);
				if (task_json.exist("ocrReplace"))
				{
					for (const auto& [key, value] : task_json["ocrReplace"].as_object()) {
						ocr_task_info_ptr->replace_map.emplace(key, value.as_string());
					}
				}
				task_info_ptr = ocr_task_info_ptr;
			}
				break;
			}
			task_info_ptr->algorithm = algorithm;
			task_info_ptr->name = name;
			std::string type = task_json["type"].as_string();
			std::transform(type.begin(), type.end(), type.begin(), std::tolower);
			if (type == "clickself") {
				task_info_ptr->type = ProcessTaskType::ClickSelf;
			}
			else if (type == "clickrand") {
				task_info_ptr->type = ProcessTaskType::ClickRand;
			}
			else if (type == "donothing" || type.empty()) {
				task_info_ptr->type = ProcessTaskType::DoNothing;
			}
			else if (type == "stop") {
				task_info_ptr->type = ProcessTaskType::Stop;
			}
			else if (type == "clickrect") {
				task_info_ptr->type = ProcessTaskType::ClickRect;
				json::value & area_json = task_json["specificArea"];
				task_info_ptr->specific_area = Rect(
					area_json[0].as_integer(),
					area_json[1].as_integer(),
					area_json[2].as_integer(),
					area_json[3].as_integer());
			}
			else if (type == "printwindow") {
				task_info_ptr->type = ProcessTaskType::PrintWindow;
			}
			else {
				DebugTraceError("Task:", name, "error:", type);
				return false;
			}

			task_info_ptr->max_times = task_json.get("maxTimes", INT_MAX);
			if (task_json.exist("exceededNext")) {
				json::array & excceed_next_arr = task_json["exceededNext"].as_array();
				for (const json::value & excceed_next : excceed_next_arr) {
					task_info_ptr->exceeded_next.emplace_back(excceed_next.as_string());
				}
			}
			else {
				task_info_ptr->exceeded_next.emplace_back("Stop");
			}
			task_info_ptr->pre_delay = task_json.get("preDelay", 0);
			task_info_ptr->rear_delay = task_json.get("rearDelay", 0);
			if (task_json.exist("reduceOtherTimes")) {
				json::array & reduce_arr = task_json["reduceOtherTimes"].as_array();
				for (const json::value& reduce : reduce_arr) {
					task_info_ptr->reduce_other_times.emplace_back(reduce.as_string());
				}
			}

			json::array & next_arr = task_json["next"].as_array();
			for (const json::value& next : next_arr) {
				task_info_ptr->next.emplace_back(next.as_string());
			}

			m_all_tasks_info.emplace(name, task_info_ptr);
		}

		for (auto&& [name, emulator_json] : root["handle"].as_object()) {
			EmulatorInfo emulator_info;
			emulator_info.name = name;

			for (json::value& info : emulator_json["window"].as_array()) {
				HandleInfo handle_info;
				handle_info.class_name = info["class"].as_string();
				handle_info.window_name = info["window"].as_string();
				emulator_info.window.emplace_back(std::move(handle_info));
			}
			for (json::value& info : emulator_json["view"].as_array()) {
				HandleInfo handle_info;
				handle_info.class_name = info["class"].as_string();
				handle_info.window_name = info["window"].as_string();
				emulator_info.view.emplace_back(std::move(handle_info));
			}
			for (json::value& info : emulator_json["control"].as_array()) {
				HandleInfo handle_info;
				handle_info.class_name = info["class"].as_string();
				handle_info.window_name = info["window"].as_string();
				emulator_info.control.emplace_back(std::move(handle_info));
			}
			if (emulator_json.exist("adb")) {
				emulator_info.is_adb = true;
				std::cout << emulator_json.to_string() << std::endl;
				emulator_info.adb.path = emulator_json["adb"]["path"].as_string();
				emulator_info.adb.connect = emulator_json["adb"]["connect"].as_string();
				emulator_info.adb.click = emulator_json["adb"]["click"].as_string();
				emulator_info.adb.display = emulator_json["adb"]["display"].as_string();
				emulator_info.adb.display_regex = emulator_json["adb"]["displayRegex"].as_string();
			}
			emulator_info.x_offset = emulator_json.get("xOffset", 0);
			emulator_info.y_offset = emulator_json.get("yOffset", 0);
			emulator_info.right_offset = emulator_json.get("rightOffset", 0);
			emulator_info.bottom_offset = emulator_json.get("bottomOffset", 0);

			emulator_info.width = DefaultWindowWidth + emulator_info.x_offset + emulator_info.right_offset;
			emulator_info.height = DefaultWindowHeight + emulator_info.y_offset + emulator_info.bottom_offset;

			m_handles.emplace(name, std::move(emulator_info));
		}

		for (json::value& rep : root["infrastOcrReplace"].as_array()) {
			m_infrast_ocr_replace.emplace(rep.as_array()[0].as_string(), rep.as_array()[1].as_string());
		}
		for (json::value& rep : root["recruitOcrReplace"].as_array()) {
			m_recruit_ocr_replace.emplace(rep.as_array()[0].as_string(), rep.as_array()[1].as_string());
		}
	}
	catch (json::exception& e) {
		DebugTraceError("Load config json error!", e.what());
		return false;
	}
	DebugTrace("Load config succeed");

	return true;
}