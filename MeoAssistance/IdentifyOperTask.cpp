#include "IdentifyOperTask.h"

#include <functional>
#include <thread>
#include <future>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <fstream>

#include <opencv2/opencv.hpp>

#include "Configer.h"
#include "InfrastConfiger.h"
#include "Identify.h"
#include "WinMacro.h"
#include "Logger.hpp"

using namespace asst;

asst::IdentifyOperTask::IdentifyOperTask(AsstCallback callback, void* callback_arg)
	: InfrastAbstractTask(callback, callback_arg)
{
	m_cropped_height_ratio = 0.043;
	m_cropped_upper_y_ratio = 0.480;
	m_cropped_lower_y_ratio = 0.923;

	m_swipe_begin = Rect(Configer::WindowWidthDefault * 0.9, Configer::WindowHeightDefault * 0.5, 0, 0);
	m_swipe_end = Rect(Configer::WindowWidthDefault * 0.1, Configer::WindowHeightDefault * 0.5, 0, 0);
}

bool asst::IdentifyOperTask::run()
{
	if (m_controller_ptr == nullptr
		|| m_identify_ptr == nullptr)
	{
		m_callback(AsstMsg::PtrIsNull, json::value(), m_callback_arg);
		return false;
	}

	auto&& detect_ret = swipe_and_detect();
	if (!detect_ret) {
		return false;
	}
	const auto& detected_opers = std::move(detect_ret.value());

	json::value result_json;
	std::vector<json::value> opers_json_vec;
	for (const auto& [name, info] : detected_opers) {
		json::value info_json;
		info_json["name"] = info.name;
		info_json["elite"] = info.elite;
		//info_json["level"] = info.level;
		opers_json_vec.emplace_back(std::move(info_json));
	}
	result_json["all"] = json::array(opers_json_vec);

	// ����û����Щ��Ա��Ҳ������ûʶ�𵽣�
	std::vector<json::value> not_found_vector;
	for (const std::string& name : InfrastConfiger::get_instance().m_all_opers_name) {
		auto iter = std::find_if(detected_opers.cbegin(), detected_opers.cend(), 
			[&](const auto& pair) -> bool {
				return pair.first == name;
			});
		if (iter == detected_opers.cend()) {
			not_found_vector.emplace_back(name);
			DebugTrace("Not Found", Utf8ToGbk(name));
		}
	}
	result_json["not_found"] = json::array(not_found_vector);

	m_callback(AsstMsg::OpersIdtfResult, result_json, m_callback_arg);

	return true;
}

std::optional<std::unordered_map<std::string, OperInfrastInfo>> asst::IdentifyOperTask::swipe_and_detect()
{
	json::value task_start_json = json::object{
		{ "task_type",  "InfrastStationTask" },
		{ "task_chain", m_task_chain},
	};
	m_callback(AsstMsg::TaskStart, task_start_json, m_callback_arg);

	std::unordered_map<std::string, std::string> feature_cond = InfrastConfiger::get_instance().m_oper_name_feat;
	std::unordered_set<std::string> feature_whatever = InfrastConfiger::get_instance().m_oper_name_feat_whatever;
	std::unordered_map<std::string, OperInfrastInfo> detected_opers;

	int times = 0;
	bool reverse = false;
	// һ��ʶ��һ�߻����������и�Ա����ץ����
	// ����������һ�飬�ٷ���������һ�飬���ʶ����
	while (true) {
		const cv::Mat& image = m_controller_ptr->get_image(true);

		// �첽���л�������
		std::future<bool> swipe_future = std::async(
			std::launch::async, &IdentifyOperTask::swipe, this, reverse);

		auto cur_name_textarea = detect_operators_name(image, feature_cond, feature_whatever);

		int oper_numer = detected_opers.size();
		for (const TextArea& textarea : cur_name_textarea)
		{
			int elite = detect_elite(image, textarea.rect);
			if (elite == -1) {
				continue;
			}
			OperInfrastInfo info;
			info.elite = elite;
			info.name = textarea.text;
			detected_opers.emplace(textarea.text, std::move(info));
		}

		json::value opers_json;
		std::vector<json::value> opers_json_vec;
		for (const auto& [name, info] : detected_opers) {
			json::value info_json;
			info_json["name"] = Utf8ToGbk(info.name);
			info_json["elite"] = info.elite;
			//info_json["level"] = info.level;
			opers_json_vec.emplace_back(std::move(info_json));
		}
		opers_json["all"] = json::array(opers_json_vec);
		m_callback(AsstMsg::OpersDetected, opers_json, m_callback_arg);

		// ���򻬶���ʱ��
		if (!reverse) {
			++times;
			// ˵������ʶ��һ���µĶ�ûʶ�𵽣�Ӧ���ǻ���������ˣ�ֱ�ӽ���ѭ��
			if (oper_numer == detected_opers.size()) {
				reverse = true;
			}
		}
		else {	// ���򻬶���ʱ��
			if (--times <= 0) {
				break;
			}
		}
		// �����ȴ����λ�������
		if (!swipe_future.get()) {
			return std::nullopt;
		}
	}
	return detected_opers;
}


int asst::IdentifyOperTask::detect_elite(const cv::Mat& image, const asst::Rect name_rect)
{
	cv::Rect elite_rect;
	// ��Ϊ�е����ֳ��е����ֶ̣������Ҷ���ģ����Ը����ұ���
	elite_rect.x = name_rect.x + name_rect.width - image.cols * 0.1;
	elite_rect.y = name_rect.y - image.rows * 0.14;
	if (elite_rect.x < 0 || elite_rect.y < 0) {
		return -1;
	}
	elite_rect.width = image.cols * 0.04;
	elite_rect.height = image.rows * 0.1;
	cv::Mat elite_mat = image(elite_rect);

	// for debug
	// ������ͼ����2560*1440�½صģ�׼����ģ��ƥ�䣬����Ҫ����һ��
	// TODO��������Ū�����Ĺ��̻����ȼ�������
	static cv::Mat elite1 = cv::imread(GetResourceDir() + "operators\\Elite1.png");
	static cv::Mat elite2 = cv::imread(GetResourceDir() + "operators\\Elite2.png");
	static bool scaled = false;
	if (!scaled) {
		scaled = true;

		double ratio = image.rows / 1440.0;
		cv::Size size1(elite1.size().width * ratio, elite1.size().height * ratio);
		cv::resize(elite1, elite1, size1);
		cv::Size size2(elite2.size().width * ratio, elite2.size().height * ratio);
		cv::resize(elite2, elite2, size2);
	}


	auto&& [score1, point1] = m_identify_ptr->match_template(elite_mat, elite1);
	auto&& [score2, point2] = m_identify_ptr->match_template(elite_mat, elite2);
	if (score1 > score2 && score1 > 0.7) {
		return 1;
	}
	else if (score2 > score1 && score2 > 0.7) {
		return 2;
	}
	else {
		return 0;
	}
}