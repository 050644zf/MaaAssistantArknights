#include "InfrastStationTask.h"

#include <thread>
#include <future>
#include <algorithm>

#include <opencv2/opencv.hpp>

#include "Configer.h"
#include "InfrastConfiger.h"
#include "WinMacro.h"
#include "Identify.h"
#include "AsstAux.h"

using namespace asst;

asst::InfrastStationTask::InfrastStationTask(AsstCallback callback, void* callback_arg)
	: OcrAbstractTask(callback, callback_arg)
{
}

bool asst::InfrastStationTask::run()
{
	if (m_view_ptr == NULL
		|| m_identify_ptr == NULL)
	{
		m_callback(AsstMsg::PtrIsNull, json::value(), m_callback_arg);
		return false;
	}

	std::vector<std::vector<std::string>> all_oper_combs;				// ���еĸ�Ա���
	std::unordered_map<std::string, std::string> feature_cond_default;	// �������ؼ��֣����OCRʶ����key�����ݵ���ȴû��value�����ݣ��������������һ��ȷ��
	std::unordered_set<std::string> feature_whatever_default;			// ������ζ�������������

	json::value task_start_json = json::object{
		{ "task_type",  "InfrastStationTask" },
		{ "task_chain", m_task_chain},
	};
	m_callback(AsstMsg::TaskStart, task_start_json, m_callback_arg);

	auto swipe_foo = [&](bool reverse = false) -> bool {
		bool ret = false;
		if (!reverse) {
			ret = m_control_ptr->swipe(m_swipe_begin, m_swipe_end);
		}
		else {
			ret = m_control_ptr->swipe(m_swipe_end, m_swipe_begin);
		}
		ret &= sleep(m_swipe_delay);
		return ret;
	};
	std::unordered_map<std::string, std::string> feature_cond = feature_cond_default;
	std::unordered_set<std::string> feature_whatever = feature_whatever_default;

	auto detect_foo = [&](const cv::Mat& image) -> std::vector<TextArea> {
		std::vector<TextArea> all_text_area = ocr_detect(image);
		/* ���˳���������վ�еĸ�Ա�� */
		std::vector<TextArea> cur_name_textarea = text_search(
			all_text_area,
			InfrastConfiger::get_instance().m_all_opers_name,
			Configer::get_instance().m_infrast_ocr_replace);

		// �����������ɸѡһ��OCRʶ��©�˵�
		for (const TextArea& textarea : all_text_area) {
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
					auto&& ret = m_identify_ptr->feature_match(image(cv_rect), value);
					if (ret) {
						cur_name_textarea.emplace_back(value, textarea.rect);
						iter = feature_cond.erase(iter);
						--iter;
					}
				}
			}
		}
		for (auto iter = feature_whatever.begin(); iter != feature_whatever.end(); ++iter) {
			auto&& ret = m_identify_ptr->feature_match(image, *iter);
			if (ret) {
				cur_name_textarea.emplace_back(std::move(ret.value()));
				iter = feature_whatever.erase(iter);
				--iter;
			}
		}

		return cur_name_textarea;
	};

	std::unordered_set<OperInfrastInfo> detected_opers;
	// һ��ʶ��һ�߻���������������վ��Ա����ץ����
	while (true) {

		const cv::Mat& image = get_format_image(true);
		// �첽���л�������
		std::future<bool> swipe_future = std::async(std::launch::async, swipe_foo);

		auto cur_name_textarea = detect_foo(image);

		int oper_numer = detected_opers.size();
		for (const TextArea& textarea : cur_name_textarea)
		{
			cv::Rect elite_rect;
			// ��Ϊ�е����ֳ��е����ֶ̣������Ҷ���ģ����Ը����ұ���
			elite_rect.x = textarea.rect.x + textarea.rect.width - 250;
			elite_rect.y = textarea.rect.y - 200;
			if (elite_rect.x < 0 || elite_rect.y < 0) {
				continue;
			}
			elite_rect.width = 100;
			elite_rect.height = 150;
			cv::Mat elite_mat = image(elite_rect);

			// for debug
			static cv::Mat elite1 = cv::imread(GetResourceDir() + "operators\\Elite1.png");
			static cv::Mat elite2 = cv::imread(GetResourceDir() + "operators\\Elite2.png");
			auto&& [score1, point1] = m_identify_ptr->match_template(elite_mat, elite1);
			auto&& [score2, point2] = m_identify_ptr->match_template(elite_mat, elite2);
			std::cout << "elite1:" << score1 << ", elite2:" << score2 << std::endl;

			OperInfrastInfo info;
			info.name = textarea.text;
			if (score1 > score2 && score1 > 0.7) {
				info.elite = 1;
			}
			else if (score2 > score1 && score2 > 0.7) {
				info.elite = 2;
			}
			else {
				info.elite = 0;
			}
			detected_opers.emplace(std::move(info));
		}

		json::value opers_json;
		std::vector<json::value> opers_json_vec;
		for (const OperInfrastInfo& info : detected_opers) {
			json::value info_json;
			info_json["name"] = Utf8ToGbk(info.name);
			info_json["elite"] = info.elite;
			//info_json["level"] = info.level;
			opers_json_vec.emplace_back(std::move(info_json));
		}
		opers_json["all"] = json::array(opers_json_vec);
		m_callback(AsstMsg::InfrastOpers, opers_json, m_callback_arg);

		// �����ȴ���������
		if (!swipe_future.get()) {
			return false;
		}
		// ˵������ʶ��һ���µĶ�ûʶ�𵽣�Ӧ���ǻ���������ˣ�ֱ�ӽ���ѭ��
		if (oper_numer == detected_opers.size()) {
			break;
		}
	}

	//// �����ļ��еĸ�Ա��ϣ���ץ�����ĸ�Ա���ȶԣ��������еĸ�Ա���У��Ǿ���������
	//// Todo ʱ�临�Ӷ�����ˣ���Ҫ�Ż���
	//std::vector<std::string> optimal_comb;
	//for (auto&& name_vec : all_oper_combs) {
	//	int count = 0;
	//	for (std::string& name : name_vec) {
	//		if (detected_names.find(name) != detected_names.cend()) {
	//			++count;
	//		}
	//		else {
	//			break;
	//		}
	//	}
	//	if (count == name_vec.size()) {
	//		optimal_comb = name_vec;
	//		break;
	//	}
	//}

	//std::vector<std::string> optimal_comb_gbk;	// ���ص�json�õģ�gbk��
	//for (const std::string& name : optimal_comb)
	//{
	//	optimal_comb_gbk.emplace_back(Utf8ToGbk(name));
	//}

	//opers_json["comb"] = json::array(optimal_comb_gbk);
	//m_callback(AsstMsg::InfrastComb, opers_json, m_callback_arg);

	//// ���������������������治���ˣ����ֱ��move
	//feature_cond = std::move(feature_cond_default);
	//feature_whatever = std::move(feature_whatever_default);

	//// һ�߻���һ�ߵ�����Ž��еĸ�Ա
	//for (int i = 0; i != m_swipe_max_times; ++i) {
	//	const cv::Mat& image = get_format_image(true);
	//	auto cur_name_textarea = detect_foo(image);

	//	for (TextArea& text_area : cur_name_textarea) {
	//		// ����˾Ͳ����ٵ��ˣ�ֱ�Ӵ����Ž�vector����ɾ��
	//		auto iter = std::find(optimal_comb.begin(), optimal_comb.end(), text_area.text);
	//		if (iter != optimal_comb.end()) {
	//			m_control_ptr->click(text_area.rect);
	//			optimal_comb.erase(iter);
	//		}
	//	}
	//	if (optimal_comb.empty()) {
	//		break;
	//	}
	//	// ��Ϊ�����͵����ì�ܵģ�����û���첽��
	//	if (!swipe_foo(true)) {
	//		return false;
	//	}
	//}
	return true;
}
