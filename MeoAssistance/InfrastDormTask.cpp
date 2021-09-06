#include "InfrastDormTask.h"

#include <future>
#include <thread>

#include <opencv2/opencv.hpp>

#include "WinMacro.h"
#include "Identify.h"
#include "Configer.h"

using namespace asst;

asst::InfrastDormTask::InfrastDormTask(AsstCallback callback, void* callback_arg)
	: OcrAbstractTask(callback, callback_arg)
{
	;
}

bool asst::InfrastDormTask::run()
{
	if (m_view_ptr == nullptr
		|| m_identify_ptr == nullptr
		|| m_control_ptr == nullptr)
	{
		m_callback(AsstMsg::PtrIsNull, json::value(), m_callback_arg);
		return false;
	}

	enter_dorm(0);
	for (int i = 0; i != 4; ++i) {
		if (i != 0) {
			enter_next_dorm();
		}
		enter_operator_selection();
		select_operators();
	}

	return true;
}

bool asst::InfrastDormTask::enter_dorm(int index)
{
	cv::Mat image = get_format_image();
	// ��ͨ�ĺ�mini�ģ��������Ӧ��ֻ��һ���н������һ����empty
	auto dorm_result = m_identify_ptr->find_all_images(image, "Dorm", 0.8);
	auto dorm_mini_result = m_identify_ptr->find_all_images(image, "DormMini", 0.8);

	decltype(dorm_result) cur_dorm_result;
	if (dorm_result.empty() && dorm_mini_result.empty()) {
		// û�ҵ����ᣬTODO������
		return false;
	}
	else if (dorm_result.empty()) {
		cur_dorm_result = std::move(dorm_mini_result);
	}
	else if (dorm_mini_result.empty()) {
		cur_dorm_result = std::move(dorm_result);
	}
	if (index >= cur_dorm_result.size()) {
		return false;
	}

	std::sort(cur_dorm_result.begin(), cur_dorm_result.end(), [](
		const auto& lhs, const auto& rhs) -> bool {
			return lhs.rect.y < rhs.rect.y;
		});

	m_control_ptr->click(cur_dorm_result.at(index).rect);
	sleep(1000);

	return false;
}

bool asst::InfrastDormTask::enter_next_dorm()
{
	static const Rect swipe_down_begin(		// ���»������
		Configer::WindowWidthDefault * 0.3,
		Configer::WindowHeightDefault * 0.8,
		Configer::WindowWidthDefault * 0.2,
		Configer::WindowWidthDefault * 0.1);
	static const Rect swipe_down_end(		// ���»����յ�
		Configer::WindowWidthDefault * 0.3,
		Configer::WindowHeightDefault * 0.2,
		Configer::WindowWidthDefault * 0.2,
		Configer::WindowWidthDefault * 0.1);

	static const int swipe_dwon_duration = 1000;	// ���»�������ʱ��

	// ��Ϸbug���������������פ��Ϣ���ѱ�ѡ�У�ֱ�ӽ��л����ᱻ���ĺ�Զ
	// ���������ȼ��һ�£������פ��Ϣ��ѡ���ˣ����Ȱ������ˣ��ٽ��л���
	auto find_result = m_identify_ptr->find_image(get_format_image(), "StationInfoSelected");
	if (find_result.score >= 0.75) {
		m_control_ptr->click(find_result.rect);
	}

	m_control_ptr->swipe(swipe_down_begin, swipe_down_end, swipe_dwon_duration);

	static const Rect double_click_rect(
		Configer::WindowWidthDefault * 0.4,
		Configer::WindowHeightDefault * 0.4,
		Configer::WindowWidthDefault * 0.2,
		Configer::WindowWidthDefault * 0.2
	);

	// ��Ϸ�е������˫�������õ�ǰ��ʩ�ص���ȷ��λ��
	m_control_ptr->click(double_click_rect);
	m_control_ptr->click(double_click_rect);

	return true;
}

bool asst::InfrastDormTask::enter_operator_selection()
{
	// ����Щ����֮һ��˵������פ��Ϣ�������ť�Ѿ��㿪��
	static const std::vector<std::string> info_opened_flags = {
		GbkToUtf8("��ǰ������ס��Ϣ"),
		GbkToUtf8("��פ����"),
		GbkToUtf8("���")
	};

	std::vector<TextArea> ocr_result = ocr_detect();

	bool is_info_opened =
		std::find_first_of(
			ocr_result.cbegin(), ocr_result.cend(),
			info_opened_flags.cbegin(), info_opened_flags.cend(),
			[](const TextArea& lhs, const std::string& rhs)
			-> bool { return lhs.text == rhs; })
		!= ocr_result.cend();

	static const std::string station_info = GbkToUtf8("��פ��Ϣ");
	// �������פ��Ϣ������û�㿪���Ǿ͵㿪
	if (!is_info_opened) {
		auto station_info_iter = std::find_if(ocr_result.cbegin(), ocr_result.cend(),
			[](const TextArea& textarea) -> bool {
				return textarea.text == station_info;
			});
		m_control_ptr->click(station_info_iter->rect);
		sleep(1000);
		ocr_result = ocr_detect();
	}

	// �������������һ���������Խ����Աѡ��ҳ��
	static const std::vector<std::string> enter_operator_page_buttons = {
		GbkToUtf8("��פ"),
		GbkToUtf8("��Ϣ��"),
		GbkToUtf8("������"),
		GbkToUtf8("����")
	};

	auto button_iter = std::find_first_of(
		ocr_result.cbegin(), ocr_result.cend(),
		enter_operator_page_buttons.cbegin(), enter_operator_page_buttons.cend(),
		[](const TextArea& lhs, const std::string& rhs)
		-> bool { return lhs.text == rhs; });
	if (button_iter == ocr_result.cend()) {
		// TODO ����
		return false;
	}
	m_control_ptr->click(button_iter->rect);
	sleep(3000);

	return true;
}

bool asst::InfrastDormTask::select_operators()
{
	// ��������ѡ�񡱰�ť
	auto click_clear_button = [&]() {
		const static Rect clear_button(430, 655, 150, 40);
		m_control_ptr->click(clear_button);
		sleep(300);
	};
	// �����ȷ������ť
	auto click_confirm_button = [&]() {
		const static Rect confirm_button(1105, 655, 150, 40);
		m_control_ptr->click(confirm_button);
		sleep(500);
	};

	cv::Mat image = get_format_image();

	// ʶ����Ϣ�С��ĸ�Ա
	auto resting_result = m_identify_ptr->find_all_images(image, "Resting", 0.8);
	if (resting_result.size() == MaxOperNumInDorm) {	// ��������˶�����Ϣ����������᲻�û��ֱ࣬�ӹ���
		click_confirm_button();
		return true;
	}

	// ʶ��ע������ɢ���ĸ�Ա
	// TODO�������ֵ̫���ˣ�����������ʱ���ٵ���һ��ģ��ͼƬ
	auto listless_result = m_identify_ptr->find_all_images(image, "Listless", 0.6);
	// ʶ�����ڹ����еĸ�Ա������״̬
	auto work_mood_result = detect_mood_status_at_work(image, Configer::get_instance().m_infrast_options.dorm_threshold);

	if (listless_result.size() == 0 && work_mood_result.size() == 0) {	// ���û��ע������ɢ�ĺ�����͵ģ�Ҳֱ�ӹ���
		click_confirm_button();
		return true;
	}
	click_clear_button();

	int count = 0;
	// �ѡ���Ϣ�С��ĸ�Ա�����ٴ�ѡ��
	for (const auto& resting : resting_result) {
		m_control_ptr->click(resting.rect);
		++count;
	}
	// ѡ��ע������ɢ���ĸ�Ա
	for (const auto& listless : listless_result) {
		if (count++ >= MaxOperNumInDorm) {
			break;
		}
		m_control_ptr->click(listless.rect);
	}
	// ѡ������ϵ͵ĸ�Ա
	for (const auto& mood_status : work_mood_result) {
		if (count++ >= MaxOperNumInDorm) {
			break;
		}
		m_control_ptr->click(mood_status.rect);
	}

	click_confirm_button();

	// ����ȷ��������ѹ����еĸ�Ա�������ˣ����ٵ�����һ��ȷ�ϵĽ��棬���û�������򲻻ᵯ������ʶ��һ���پ���Ҫ��Ҫ���
	auto&& [algorithm, score, second_confirm_rect] = m_identify_ptr->find_image(get_format_image(), "DormConfirm");
	if (score >= Configer::TemplThresholdDefault) {
		m_control_ptr->click(second_confirm_rect);
	}
	sleep(2000);

	return true;
}

std::vector<InfrastDormTask::MoodStatus> InfrastDormTask::detect_mood_status_at_work(const cv::Mat& image, double process_threshold) const
{
	constexpr static int MoodWidth = Configer::WindowWidthDefault * 0.0664 + 0.5;		// ������������ȣ��������ʱ��
	constexpr static int MoodHeight = Configer::WindowHeightDefault * 0.00416 + 0.5;	// ����������߶�

#ifdef LOG_TRACE
	cv::Mat draw_image = image.clone();
#endif

	// �ѹ����е��Ǹ���ɫЦ��ȫץ����
	auto smiley_result = m_identify_ptr->find_all_images(image, "SmileyAtWork", 0.8, false);

	std::vector<MoodStatus> moods_vec;
	for (const auto& smiley : smiley_result) {
		// ���������Ƿ񳬳���ͼƬ��Χ
		if (smiley.rect.x + smiley.rect.width + MoodWidth >= image.cols) {
			continue;
		}
		// ���ݻ�ɫЦ�����������������������
		cv::Rect process_rect(
			smiley.rect.x + smiley.rect.width,
			smiley.rect.y,
			MoodWidth,
			smiley.rect.height);
		cv::Mat process_mat = image(process_rect);

		cv::Mat process_gray;
		cv::cvtColor(process_mat, process_gray, cv::COLOR_BGR2GRAY);

		int max_white_length = 0;	// ��ĺ��������İ�ɫ�����ĳ���
		for (int i = 0; i != process_gray.rows; ++i) {
			int cur_white_length = 0;
			cv::uint8_t left_value = 250;	// ��ǰ�����ĵ��ֵ
			for (int j = 0; j != process_gray.cols; ++j) {
				constexpr static cv::uint8_t LowerLimit = 180;
				constexpr static cv::uint8_t DiffThreshold = 20;

				auto value = process_gray.at<cv::uint8_t>(i, j);
				if (value >= LowerLimit && left_value - value < DiffThreshold) {	// �ұߵ���ɫ�����߱仯����ֵ���ڣ�����Ϊ��������
					left_value = value;
					++cur_white_length;
					if (max_white_length < cur_white_length) {
						max_white_length = cur_white_length;
					}
				}
				else {
					if (max_white_length < cur_white_length) {
						max_white_length = cur_white_length;
					}
					left_value = 250;
					cur_white_length = 0;
				}
			}
		}

		MoodStatus mood_status;
		mood_status.actual_length = max_white_length;
		mood_status.process = static_cast<double>(max_white_length) / MoodWidth;
		mood_status.rect = asst::Rect(smiley.rect.x, smiley.rect.y, 
			smiley.rect.width + process_rect.width, smiley.rect.height + process_rect.height);
		mood_status.actual_rect = asst::Rect(process_rect.x, process_rect.y, 
			max_white_length, process_rect.height);
#ifdef LOG_TRACE
		cv::Rect cv_actual_rect(mood_status.actual_rect.x, mood_status.actual_rect.y,
			mood_status.actual_rect.width, mood_status.actual_rect.height);
		cv::rectangle(draw_image, cv_actual_rect, cv::Scalar(0, 0, 255), 1);
		cv::putText(draw_image, std::to_string(mood_status.process), cv::Point(cv_actual_rect.x, cv_actual_rect.y), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar(0, 0, 255));
#endif
		if (mood_status.process <= process_threshold) {	// ����С����ֵ��ֱ�Ӽ�����
			moods_vec.emplace_back(std::move(mood_status));
		}
		else {
			// �Ȳ��ڹ����У�Ҳ������Ϣ�У���������ֵ��������ֵ�ģ�Ҳ��Ҫ��������
			// ˼·�����������������λ�ã����ϲü����������С�or����Ϣ�С�����һ������
			// ����ģ��ƥ�䣬���ûƥ���ϣ�����Ϊ�����Ա�Ȳ��ڹ�����Ҳ������Ϣ
			constexpr static int HeightDiff = Configer::WindowHeightDefault * 0.13;
			constexpr static int OnShiftHeight = Configer::WindowHeightDefault * 0.08;
			cv::Rect on_shift_rect(mood_status.rect.x, mood_status.rect.y - HeightDiff, mood_status.rect.width, OnShiftHeight);
			// ����Ϣ�С���Ц��ǰ���ģ��ƥ���ǶԲ��ϵģ��߲��������������ֻƥ�䡰�����С�����
			auto find_result = m_identify_ptr->find_image(image(on_shift_rect), "OnShift");
			if (find_result.score < 0.5) {
				moods_vec.emplace_back(std::move(mood_status));
			}
		}
	}

	std::sort(moods_vec.begin(), moods_vec.end(),
		[](const auto& lhs, const auto& rhs) -> bool {
			// ��ʣ����������Ÿ����ٵ���ǰ��
			if (lhs.actual_length != rhs.actual_length) {
				return lhs.actual_length < rhs.actual_length;
			}
			// ���ʣ��������ȣ�����ѡ����ࡢ���ϲ�ģ��������û�ֱ��
			else if (lhs.rect.x != rhs.rect.x) {
				return lhs.rect.x < rhs.rect.x;
			}
			else {
				return lhs.rect.y < rhs.rect.y;
			}
		});

	return moods_vec;
}
