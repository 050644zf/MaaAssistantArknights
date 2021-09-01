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
	: OcrAbstractTask(callback, callback_arg),
	m_cropped_height_ratio(0.043),
	m_cropped_upper_y_ratio(0.480),
	m_cropped_lower_y_ratio(0.923)
{
	;
}

bool asst::IdentifyOperTask::run()
{
	if (m_view_ptr == nullptr
		|| m_identify_ptr == nullptr
		|| m_control_ptr == nullptr)
	{
		m_callback(AsstMsg::PtrIsNull, json::value(), m_callback_arg);
		return false;
	}

	auto&& [width, height] = m_view_ptr->getAdbDisplaySize();

	m_swipe_begin = Rect(width * 0.9, height * 0.5, 0, 0);
	m_swipe_end = Rect(width * 0.1, height * 0.5, 0, 0);

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
		{ "task_chain", OcrAbstractTask::m_task_chain},
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
		const cv::Mat& image = OcrAbstractTask::get_format_image(true);

		// �첽���л�������
		std::future<bool> swipe_future = std::async(
			std::launch::async, &IdentifyOperTask::swipe, this, reverse);

		auto cur_name_textarea = detect_opers(image, feature_cond, feature_whatever);

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

std::vector<TextArea> asst::IdentifyOperTask::detect_opers(
	const cv::Mat& image, 
	std::unordered_map<std::string, 
	std::string>& feature_cond, 
	std::unordered_set<std::string>& feature_whatever)
{
	// �ü�������Ա����һ��������ͼƬ��û��Ҫ������ͼƬ��ȥʶ��
	int cropped_height = image.rows * m_cropped_height_ratio;
	int cropped_upper_y = image.rows * m_cropped_upper_y_ratio;
	cv::Mat upper_part_name_image = image(cv::Rect(0, cropped_upper_y, image.cols, cropped_height));

	std::vector<TextArea> upper_text_area = ocr_detect(upper_part_name_image);	// ��������
	// ��ΪͼƬ�ǲü����ģ����Զ�Ӧԭͼ������Ҫ���ϲü��Ĳ���
	for (TextArea& textarea : upper_text_area) {
		textarea.rect.y += cropped_upper_y;
	}
	// ���˳����еĸ�Ա��
	std::vector<TextArea> upper_part_names = text_match(
		upper_text_area,
		InfrastConfiger::get_instance().m_all_opers_name,
		Configer::get_instance().m_infrast_ocr_replace);
	// ����һ��Ϳ�ڣ�������汻����������ʶ����
	cv::Mat draw_image = image;
	for (const TextArea& textarea : upper_part_names) {
		cv::Rect rect(textarea.rect.x, textarea.rect.y, textarea.rect.width, textarea.rect.height);
		// ע��������ǳ������ԭͼimageҲ�ᱻͿ��
		cv::rectangle(draw_image, rect, cv::Scalar(0, 0, 0), -1);
	}

	// �°벿�ֵĸ�Ա
	int cropped_lower_y = image.rows * m_cropped_lower_y_ratio;
	cv::Mat lower_part_name_image = image(cv::Rect(0, cropped_lower_y, image.cols, cropped_height));

	std::vector<TextArea> lower_text_area = ocr_detect(lower_part_name_image);	// ��������
	// ��ΪͼƬ�ǲü����ģ����Զ�Ӧԭͼ������Ҫ���ϲü��Ĳ���
	for (TextArea& textarea : lower_text_area) {
		textarea.rect.y += cropped_lower_y;
	}
	// ���˳����еĸ�Ա��
	std::vector<TextArea> lower_part_names = text_match(
		lower_text_area,
		InfrastConfiger::get_instance().m_all_opers_name,
		Configer::get_instance().m_infrast_ocr_replace);
	// ����һ��Ϳ�ڣ�������汻����������ʶ����
	for (const TextArea& textarea : lower_part_names) {
		cv::Rect rect(textarea.rect.x, textarea.rect.y, textarea.rect.width, textarea.rect.height);
		// ע��������ǳ������ԭͼimageҲ�ᱻͿ��
		cv::rectangle(draw_image, rect, cv::Scalar(0, 0, 0), -1);
	}

	// ����������ʶ�����ϲ�
	std::vector<TextArea> all_text_area = std::move(upper_text_area);
	all_text_area.insert(all_text_area.end(),
		std::make_move_iterator(lower_text_area.begin()),
		std::make_move_iterator(lower_text_area.end()));
	std::vector<TextArea> all_opers_textarea = std::move(upper_part_names);
	all_opers_textarea.insert(all_opers_textarea.end(),
		std::make_move_iterator(lower_part_names.begin()),
		std::make_move_iterator(lower_part_names.end()));

	// ���ocr������Ѿ���ĳ����Ա�ˣ���û��Ҫ�ٳ��Զ�����������ˣ�ֱ��ɾ��
	for (const TextArea& textarea : all_opers_textarea) {
		auto cond_iter = std::find_if(feature_cond.begin(), feature_cond.end(),
			[&textarea](const auto& pair) -> bool {
				return textarea.text == pair.second;
			});
		if (cond_iter != feature_cond.end()) {
			feature_cond.erase(cond_iter);
		}

		auto whatever_iter = std::find_if(feature_whatever.begin(), feature_whatever.end(),
			[&textarea](const std::string& str) -> bool {
				return textarea.text == str;
			});
		if (whatever_iter != feature_whatever.end()) {
			feature_whatever.erase(whatever_iter);
		}
	}

	// �����������ɸѡһ��OCRʶ��©�˵ġ����йؼ��ֵ�
	for (const TextArea& textarea : all_text_area) {
		auto find_iter = std::find_if(all_opers_textarea.cbegin(), all_opers_textarea.cend(),
			[&textarea](const auto& rhs) -> bool {
				return textarea.text == rhs.text; });
		if (find_iter != all_opers_textarea.cend()) {
			continue;
		}
		for (auto iter = feature_cond.begin(); iter != feature_cond.end(); ++iter) {
			auto& [key, value] = *iter;
			// ʶ����key������ûʶ��value�������������Ҫ������������һ��ȷ����
			if (textarea.text.find(key) != std::string::npos
				&& textarea.text.find(value) == std::string::npos) {
				// ��key���ڵľ��ηŴ�һ����ȥ��������⣬����Ҫ������ͼƬ����ȥ���
				Rect magnified_area = textarea.rect.center_zoom(2.0);
				magnified_area.x = (std::max)(0, magnified_area.x);
				magnified_area.y = (std::max)(0, magnified_area.y);
				if (magnified_area.x + magnified_area.width >= image.cols) {
					magnified_area.width = image.cols - magnified_area.x - 1;
				}
				if (magnified_area.y + magnified_area.height >= image.rows) {
					magnified_area.height = image.rows - magnified_area.y - 1;
				}
				cv::Rect cv_rect(magnified_area.x, magnified_area.y, magnified_area.width, magnified_area.height);
				// key�ǹؼ��ֶ��ѣ�����Ҫʶ�����value
				auto&& ret = OcrAbstractTask::m_identify_ptr->feature_match(image(cv_rect), value);
				if (ret) {
					// ƥ�������´ξͲ�����ƥ������ˣ�ֱ��ɾ��
					all_opers_textarea.emplace_back(value, textarea.rect);
					iter = feature_cond.erase(iter);
					--iter;
					// Ҳ��whatever����ɾ��
					auto whatever_iter = std::find_if(feature_whatever.begin(), feature_whatever.end(),
						[&textarea](const std::string& str) -> bool {
							return textarea.text == str;
						});
					if (whatever_iter != feature_whatever.end()) {
						feature_whatever.erase(whatever_iter);
					}
					// ˳����Ϳ���ˣ�������汻whatever����������ʶ��
					// ע��������ǳ������ԭͼimageҲ�ᱻͿ��
					cv::rectangle(draw_image, cv_rect, cv::Scalar(0, 0, 0), -1);
				}
			}
		}
	}

	// �����������ɸѡһ��OCRʶ��©�˵ġ���������ζ�����ʶ���
	for (auto iter = feature_whatever.begin(); iter != feature_whatever.end(); ++iter) {
		// �ϰ벿�ֳ����ε�ͼƬ
		auto&& upper_ret = OcrAbstractTask::m_identify_ptr->feature_match(upper_part_name_image, *iter);
		if (upper_ret) {
			TextArea temp = std::move(upper_ret.value());
#ifdef LOG_TRACE	// Ҳ˳��Ϳ��һ�£����㿴˭û��ʶ�����
			cv::Rect draw_rect(temp.rect.x, temp.rect.y, temp.rect.width, temp.rect.height);
			// ע��������ǳ������ԭͼimageҲ�ᱻͿ��
			cv::rectangle(upper_part_name_image, draw_rect, cv::Scalar(0, 0, 0), -1);
#endif
			// ��ΪͼƬ�ǲü����ģ����Զ�Ӧԭͼ������Ҫ���ϲü��Ĳ���
			temp.rect.y += cropped_upper_y;
			all_opers_textarea.emplace_back(std::move(temp));
			iter = feature_whatever.erase(iter);
			--iter;
			continue;
		}
		// �°벿�ֳ����ε�ͼƬ
		auto&& lower_ret = OcrAbstractTask::m_identify_ptr->feature_match(lower_part_name_image, *iter);
		if (lower_ret) {
			TextArea temp = std::move(lower_ret.value());
#ifdef LOG_TRACE	// Ҳ˳��Ϳ��һ�£����㿴˭û��ʶ�����
			cv::Rect draw_rect(temp.rect.x, temp.rect.y, temp.rect.width, temp.rect.height);
			// ע��������ǳ������ԭͼimageҲ�ᱻͿ��
			cv::rectangle(lower_part_name_image, draw_rect, cv::Scalar(0, 0, 0), -1);
#endif
			// ��ΪͼƬ�ǲü����ģ����Զ�Ӧԭͼ������Ҫ���ϲü��Ĳ���
			temp.rect.y += cropped_lower_y;
			all_opers_textarea.emplace_back(std::move(temp));
			iter = feature_whatever.erase(iter);
			--iter;
			continue;
		}
	}

	return all_opers_textarea;
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


	auto&& [score1, point1] = OcrAbstractTask::m_identify_ptr->match_template(elite_mat, elite1);
	auto&& [score2, point2] = OcrAbstractTask::m_identify_ptr->match_template(elite_mat, elite2);
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

bool IdentifyOperTask::swipe(bool reverse)
{
//#ifndef LOG_TRACE
	bool ret = true;
	if (!reverse) {
		ret &= m_control_ptr->swipe(m_swipe_begin, m_swipe_end, SwipeDuration);
	}
	else {
		ret &= m_control_ptr->swipe(m_swipe_end, m_swipe_begin, SwipeDuration);
	}
	ret &= sleep(SwipeExtraDelay);
	return ret;
//#else
//	return sleep(SwipeExtraDelay);
//#endif
}

bool IdentifyOperTask::keep_swipe(bool reverse)
{
	bool ret = true;
	while (m_keep_swipe && !ret) {
		ret = swipe(reverse);
	}
	return ret;
}