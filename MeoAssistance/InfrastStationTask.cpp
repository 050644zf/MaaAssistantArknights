#include "InfrastStationTask.h"

#include <thread>
#include <future>
#include <algorithm>
#include <future>
#include <list>

#include <opencv2/opencv.hpp>

#include "Configer.h"
#include "InfrastConfiger.h"
#include "WinMacro.h"
#include "Identify.h"
#include "AsstAux.h"

using namespace asst;

bool asst::InfrastStationTask::run()
{
	if (m_controller_ptr == nullptr
		|| m_identify_ptr == nullptr)
	{
		m_callback(AsstMsg::PtrIsNull, json::value(), m_callback_arg);
		return false;
	}

	cv::Mat image = get_format_image();
	// ��ʶ��һ���м�������վ/ó��վ
	const static std::vector<std::string> facility_number_key = { "02", "03", "04", "05" };
	std::vector<Rect> facility_number_rect;
	facility_number_rect.emplace_back(Rect());	// ��װ��01 pushһ��������ѭ����д=��=

	for (const std::string& key : facility_number_key) {
		auto&& [algorithm, score, temp_rect] = m_identify_ptr->find_image(image, key);
		if (score >= Configer::TemplThresholdDefault) {
			facility_number_rect.emplace_back(temp_rect);
		}
		else {
			break;
		}
	}
	for (size_t i = 0; i != facility_number_rect.size(); ++i) {
		if (i != 0) {
			// �㵽�������
			m_controller_ptr->click(facility_number_rect[i]);
			sleep(300);
			image = get_format_image();
		}
		// �����ǰ����û����Ӹ�Ա�İ�ť���ǾͲ�����
		auto&& [algorithm1, score1, add_rect1] = m_identify_ptr->find_image(image, "AddOperator");
		auto&& [algorithm2, score2, add_rect2] = m_identify_ptr->find_image(image, "AddOperator-Trade");
		if (score1 < Configer::TemplThresholdDefault && score2 < Configer::TemplThresholdDefault) {
			continue;
		}

		// ʶ��ǰ������ʲô
		for (const auto& [key, useless_value] : InfrastConfiger::get_instance().m_infrast_combs) {
			auto&& [algorithm, score, useless_rect] = m_identify_ptr->find_image(image, key);
			if (score >= Configer::TemplThresholdDefault) {
				m_facility = key;
				break;
			}
		}
		//�����Ӹ�Ա���Ǹ���ť
		Rect add_rect = score1 >= score2 ? std::move(add_rect1) : std::move(add_rect2);
		m_controller_ptr->click(add_rect);
		sleep(2000);

		click_clear_button();
		sleep(300);

		auto&& [width, height] = m_controller_ptr->getDisplaySize();
		m_swipe_begin = Rect(width * 0.9, height * 0.5, 0, 0);
		m_swipe_end = Rect(width * 0.5, height * 0.5, 0, 0);

		auto detect_ret = swipe_and_detect();
		if (!detect_ret) {
			return false;
		}
		auto cur_opers_info = std::move(detect_ret.value());
		// TODO������ʶ��һ�Σ���ǰ6�����Žⶼ�г��������6������վ����Ȼ��Ͳ���ÿ�ζ�ʶ�𲢼������Ž���
		std::list<std::string> optimal_comb = calc_optimal_comb(cur_opers_info);
		bool select_ret = swipe_and_select(optimal_comb);
	}

	return true;
}

std::optional<std::unordered_map<std::string, OperInfrastInfo>> asst::InfrastStationTask::swipe_and_detect()
{
	if (!swipe_to_the_left()) {
		return std::nullopt;
	}

	std::unordered_map<std::string, std::string> feature_cond = InfrastConfiger::get_instance().m_oper_name_feat;
	std::unordered_set<std::string> feature_whatever = InfrastConfiger::get_instance().m_oper_name_feat_whatever;

	std::vector<std::string> end_flag_vec = InfrastConfiger::get_instance().m_infrast_end_flag[m_facility];

	std::unordered_map<std::string, OperInfrastInfo> cur_opers_info;
	// ��Ϊ��Щ��Ա��������Ϣ�����߱���������ռ���ˣ�������Ҫ����ʶ��һ�鵱ǰ���õĸ�Ա
	for (int i = 0; i != SwipeMaxTimes; ++i) {
		const cv::Mat& image = get_format_image(true);
		// �첽���л�������
		std::future<bool> swipe_future = std::async(
			std::launch::async, &InfrastStationTask::swipe, this, false);

		auto cur_name_textarea = detect_operators_name(image, feature_cond, feature_whatever);
		for (const TextArea& textarea : cur_name_textarea) {
			OperInfrastInfo info;
			// ����map��û��������ֵ����������һ��ʼʶ��©�ˡ��鵽���¸�Ա��û���µȣ�Ҳ�п����Ǳ���ʶ�����
			// TODO����������׸��ص���ȥ����ʾ����case
			if (m_all_opers_info.find(textarea.text) == m_all_opers_info.cend()) {
				info.name = textarea.text;
				info.elite = 0;
				info.level = 0;
			}
			else {
				info = m_all_opers_info[textarea.text];
			}
			cur_opers_info.emplace(textarea.text, std::move(info));
		}

		json::value opers_json;
		std::vector<json::value> opers_json_vec;
		for (const auto& [name, info] : cur_opers_info) {
			json::value info_json;
			info_json["name"] = Utf8ToGbk(info.name);
			info_json["elite"] = info.elite;
			//info_json["level"] = info.level;
			opers_json_vec.emplace_back(std::move(info_json));
		}
		opers_json["all"] = json::array(opers_json_vec);
		m_callback(AsstMsg::OpersDetected, opers_json, m_callback_arg);

		bool break_flag = false;
		// ����ҵ���end_flag_vec�е����֣�˵���Ѿ�ʶ���е�ǰ�������ܵ����һ����Ա�ˣ��Ͳ��ý���ʶ����
		auto find_iter = std::find_first_of(
			cur_opers_info.cbegin(), cur_opers_info.cend(),
			end_flag_vec.cbegin(), end_flag_vec.cend(),
			[](const auto& lhs, const auto& rhs) ->bool {
				return lhs.first == rhs;
			});
		if (find_iter != cur_opers_info.cend()) {
			break_flag = true;
		}
		// �����ȴ��첽�Ļ�������
		if (!swipe_future.get()) {
			return std::nullopt;
		}
		if (break_flag) {
			break;
		}
	}
	return cur_opers_info;
}

std::list<std::string> asst::InfrastStationTask::calc_optimal_comb(
	const std::unordered_map<std::string, OperInfrastInfo>& cur_opers_info) const
{
	// �����ļ��еĸ�Ա��ϣ���ץ�����ĸ�Ա���ȶԣ��������еĸ�Ա���У��Ǿ���������
	// ע�������еĸ�Ա�����Ҫ�������
	// Todo ʱ�临�Ӷ�����ˣ���Ҫ�Ż���

	OperInfrastComb three_optimal_comb;		// ������ϵ����Ž�
	OperInfrastComb single_optimal_comb;	// ������������ɵ���� �е����Ž�
	for (const OperInfrastComb& comb : InfrastConfiger::get_instance().m_infrast_combs[m_facility]) {
		if (comb.comb.size() == 3) {
			int count = 0;
			for (const OperInfrastInfo& info : comb.comb) {
				// �ҵ��˸�Ա�������ҵ�ǰ��Ӣ���ȼ���Ҫ���ڵ��������ļ���Ҫ��ľ�Ӣ���ȼ�
				auto find_iter = cur_opers_info.find(info.name);
				if (find_iter != cur_opers_info.cend()
					&& find_iter->second.elite >= info.elite) {
					++count;
				}
				else {
					break;
				}
			}
			if (count == 3) {
				three_optimal_comb = comb;
				break;
			}
		}
		else if (comb.comb.size() == 1) {
			const auto& cur_info = comb.comb.at(0);
			auto find_iter = cur_opers_info.find(cur_info.name);
			if (find_iter != cur_opers_info.cend()
				&& find_iter->second.elite >= cur_info.elite) {
				single_optimal_comb.comb.emplace_back(cur_info);
				single_optimal_comb.efficiency += comb.efficiency;
				if (single_optimal_comb.comb.size() >= 3) {
					break;
				}
			}
		}
		else {
			// �����ļ���û�����˵ģ��������ߵ����TODO������
			// �Ժ���Ϸ��������˫�˵���˵��˼·��һ���ģ��ҳ�˫������и�Ա���е�+����������еġ�
			// �ٽ�һ��OperInfrastComb����һ��Ч��
		}
	}
	OperInfrastComb optimal_comb = three_optimal_comb.efficiency >= single_optimal_comb.efficiency 
		? three_optimal_comb : single_optimal_comb;

	std::list<std::string> optimal_opers_name;
	std::vector<std::string> optimal_comb_gbk;	// ���ص�json�õģ�gbk��
	for (const auto& info : optimal_comb.comb) {
		optimal_opers_name.emplace_back(info.name);
		optimal_comb_gbk.emplace_back(Utf8ToGbk(info.name));
	}

	json::value opers_json;
	opers_json["comb"] = json::array(optimal_comb_gbk);
	m_callback(AsstMsg::InfrastComb, opers_json, m_callback_arg);

	return optimal_opers_name;
}

bool asst::InfrastStationTask::swipe_and_select(std::list<std::string>& name_comb, int swipe_max_times)
{
	//if (!swipe_to_the_left()) {
	//	return false;
	//}

	std::unordered_map<std::string, std::string> feature_cond = InfrastConfiger::get_instance().m_oper_name_feat;
	std::unordered_set<std::string> feature_whatever = InfrastConfiger::get_instance().m_oper_name_feat_whatever;
	// һ�߻���һ�ߵ�����Ž��еĸ�Ա
	for (int i = 0; i != swipe_max_times; ++i) {
		const cv::Mat& image = get_format_image(true);
		auto cur_name_textarea = detect_operators_name(image, feature_cond, feature_whatever);

		for (TextArea& text_area : cur_name_textarea) {
			// ����˾Ͳ����ٵ��ˣ�ֱ�Ӵ����Ž�vector����ɾ��
			auto iter = std::find(name_comb.begin(), name_comb.end(), text_area.text);
			if (iter != name_comb.end()) {
				m_controller_ptr->click(text_area.rect);
				sleep(200);
				name_comb.erase(iter);
			}
		}
		if (name_comb.empty()) {
			break;
		}
		// ��Ϊ�����͵����ì�ܵģ�����û���첽��
		if (!swipe(true)) {
			return false;
		}
	}
	// �����ȷ������ť��ȷ����Ҫ�������صģ��Ƚ�������sleepһ��
	get_format_image();	// ����get imageûʲô�ã���������Ϊ�˴������������ţ�TODO �Ż���
	m_controller_ptr->click(Rect(1105, 655, 150, 40));
	sleep(2000);
	return true;
}