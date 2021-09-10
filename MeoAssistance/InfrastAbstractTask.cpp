#include "InfrastAbstractTask.h"

#include <opencv2/opencv.hpp>

#include "WinMacro.h"
#include "Identify.h"
#include "Configer.h"
#include "InfrastConfiger.h"

using namespace asst;

asst::InfrastAbstractTask::InfrastAbstractTask(AsstCallback callback, void* callback_arg)
	: OcrAbstractTask(callback, callback_arg),
	m_swipe_begin(Configer::WindowWidthDefault * 0.9, Configer::WindowHeightDefault * 0.5, 0, 0),
	m_swipe_end(Configer::WindowWidthDefault * 0.5, Configer::WindowHeightDefault * 0.5, 0, 0)
{
}

bool asst::InfrastAbstractTask::swipe_to_the_left()
{
	constexpr int SwipeTimes = 5;

	m_swipe_duration = 100;
	m_swipe_extra_delay = 0;
	// ����ʹ��������
	bool ret = false;
	for (int i = 0; i != SwipeTimes; ++i) {
		ret = swipe(true);
		if (!ret) {
			break;
		}
	}
	m_swipe_duration = SwipeDurationDefault;
	m_swipe_extra_delay = SwipeExtraDelayDefault;
	sleep(SwipeExtraDelayDefault);
	return ret;
}

bool asst::InfrastAbstractTask::click_clear_button()
{
	const static Rect ClearButtonRect(430, 655, 150, 40);
	return m_controller_ptr->click(ClearButtonRect);
}

bool asst::InfrastAbstractTask::click_confirm_button()
{
	const static Rect ConfirmButtonRect(1105, 655, 150, 40);
	return m_controller_ptr->click(ConfirmButtonRect);
}

bool asst::InfrastAbstractTask::click_return_button()
{
	const static Rect ConfirmButtonRect(20, 20, 135, 35);
	return m_controller_ptr->click(ConfirmButtonRect);
}

bool asst::InfrastAbstractTask::swipe(bool reverse)
{
//#ifndef LOG_TRACE
	bool ret = true;
	if (!reverse) {
		ret &= m_controller_ptr->swipe(m_swipe_begin, m_swipe_end, m_swipe_duration);
		++m_swipe_times;
	}
	else {
		ret &= m_controller_ptr->swipe(m_swipe_end, m_swipe_begin, m_swipe_duration);
		--m_swipe_times;
	}
	ret &= sleep(m_swipe_extra_delay);
	return ret;
//#else
//	return sleep(SwipeExtraDelay);
//#endif
}

std::vector<TextArea> asst::InfrastAbstractTask::detect_operators_name(
	const cv::Mat& image,
	std::unordered_map<std::string,
	std::string>& feature_cond,
	std::unordered_set<std::string>& feature_whatever)
{
	// �ü�������Ա����һ��������ͼƬ��û��Ҫ������ͼƬ��ȥʶ��
	int cropped_height = image.rows * m_cropped_height_ratio;
	int cropped_upper_y = image.rows * m_cropped_upper_y_ratio;
	cv::Mat upper_part_name_image = image(cv::Rect(0, cropped_upper_y, image.cols, cropped_height));
	// ocr�⣬��ɫͼƬʶ��Ч���úܶࣻ����ֻ������ͨ����ͼƬ����������ת���Σ��ͽ�ȥ��ɫ�ġ���ͨ����ͼƬ
	cv::cvtColor(upper_part_name_image, upper_part_name_image, cv::COLOR_BGR2GRAY);
	cv::cvtColor(upper_part_name_image, upper_part_name_image, cv::COLOR_GRAY2BGR);

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
	for (const TextArea& textarea : upper_part_names) {
		cv::Rect rect(textarea.rect.x, textarea.rect.y - cropped_upper_y, textarea.rect.width, textarea.rect.height);
		// ������ת���Ҷ�ͼ��ת�����ģ��൱�����������Ӱ��ԭͼ
		cv::rectangle(upper_part_name_image, rect, cv::Scalar(0, 0, 0), -1);
	}

	// �°벿�ֵĸ�Ա
	int cropped_lower_y = image.rows * m_cropped_lower_y_ratio;
	cv::Mat lower_part_name_image = image(cv::Rect(0, cropped_lower_y, image.cols, cropped_height));
	// ocr�⣬��ɫͼƬʶ��Ч���úܶࣻ����ֻ������ͨ����ͼƬ����������ת���Σ��ͽ�ȥ��ɫ�ġ���ͨ����ͼƬ
	cv::cvtColor(lower_part_name_image, lower_part_name_image, cv::COLOR_BGR2GRAY);
	cv::cvtColor(lower_part_name_image, lower_part_name_image, cv::COLOR_GRAY2BGR);

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
		cv::Rect rect(textarea.rect.x, textarea.rect.y - cropped_lower_y, textarea.rect.width, textarea.rect.height);
		// ������ת���Ҷ�ͼ��ת�����ģ��൱�����������Ӱ��ԭͼ
		cv::rectangle(lower_part_name_image, rect, cv::Scalar(0, 0, 0), -1);
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
					//cv::rectangle(draw_image, cv_rect, cv::Scalar(0, 0, 0), -1);
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

bool asst::InfrastAbstractTask::enter_station(const std::vector<std::string>& templ_names, int index, double threshold)
{
	cv::Mat image = m_controller_ptr->get_image();

	std::vector<asst::Identify::FindImageResult> max_size_reslut;
	for (const auto& templ : templ_names) {
		auto temp_result = m_identify_ptr->find_all_images(image, templ, threshold);
		if (temp_result.size() > max_size_reslut.size()) {
			max_size_reslut = temp_result;
		}
	}

	if (max_size_reslut.empty()) {
		return false;
	}
	if (index >= max_size_reslut.size()) {
		return false;
	}

	std::sort(max_size_reslut.begin(), max_size_reslut.end(), [](
		const auto& lhs, const auto& rhs) -> bool {
			if (std::abs(lhs.rect.y - rhs.rect.y) < 5) {	// ͬһ�ŵ�
				return lhs.rect.x < rhs.rect.x;
			}
			else {
				return lhs.rect.y < rhs.rect.y;
			}
		});

	m_controller_ptr->click(max_size_reslut.at(index).rect);
	sleep(1000);

	return false;
}
