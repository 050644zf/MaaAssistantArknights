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
	: IdentifyOperTask(callback, callback_arg)
{
	m_cropped_height_ratio = 0.052;
	m_cropped_upper_y_ratio = 0.441;
	m_cropped_lower_y_ratio = 0.831;
}

bool asst::InfrastStationTask::run()
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
	m_swipe_end = Rect(width * 0.4, height * 0.5, 0, 0);


	// �����ļ��еĸ�Ա��ϣ���ץ�����ĸ�Ա���ȶԣ��������еĸ�Ա���У��Ǿ���������
	// Todo ʱ�临�Ӷ�����ˣ���Ҫ�Ż���
	std::vector<std::string> optimal_comb; // OperInfrastInfo�Ǵ���Ӣ���͵ȼ���Ϣ�ģ�������ʶ�𲻵���Ҳ�ò���������ֻ������Ա��
	for (const auto& name_vec : InfrastConfiger::get_instance().m_infrast_combs[m_facility]) {
		int count = 0;
		std::vector<std::string> temp_comb;
		for (const OperInfrastInfo& info : name_vec) {
			if (m_all_opers_info.find(info) != m_all_opers_info.cend()) {
				++count;
				temp_comb.emplace_back(info.name);
			}
			else {
				break;
			}
		}
		if (count == temp_comb.size()) {
			optimal_comb = temp_comb;
			break;
		}
	}

	std::vector<std::string> optimal_comb_gbk;	// ���ص�json�õģ�gbk��
	for (const std::string& name : optimal_comb)
	{
		optimal_comb_gbk.emplace_back(Utf8ToGbk(name));
	}

	json::value opers_json;
	opers_json["comb"] = json::array(optimal_comb_gbk);
	m_callback(AsstMsg::InfrastComb, opers_json, m_callback_arg);

	std::unordered_map<std::string, std::string> feature_cond = InfrastConfiger::get_instance().m_oper_name_feat;
	std::unordered_set<std::string> feature_whatever = InfrastConfiger::get_instance().m_oper_name_feat_whatever;

	// һ�߻���һ�ߵ�����Ž��еĸ�Ա
	for (int i = 0; i != SwipeMaxTimes; ++i) {
		const cv::Mat& image = get_format_image(true);
		auto cur_name_textarea = detect_opers(image, feature_cond, feature_whatever);

		for (TextArea& text_area : cur_name_textarea) {
			// ����˾Ͳ����ٵ��ˣ�ֱ�Ӵ����Ž�vector����ɾ��
			auto iter = std::find(optimal_comb.begin(), optimal_comb.end(), text_area.text);
			if (iter != optimal_comb.end()) {
				m_control_ptr->click(text_area.rect);
				optimal_comb.erase(iter);
			}
		}
		if (optimal_comb.empty()) {
			break;
		}
		// ��Ϊ�����͵����ì�ܵģ�����û���첽��
		if (!swipe()) {
			return false;
		}
	}
	return true;
}
