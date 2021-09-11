#include "InfrastDormTask.h"

#include <future>
#include <thread>

#include <opencv2/opencv.hpp>

#include "WinMacro.h"
#include "Identify.h"
#include "Configer.h"

using namespace asst;

bool asst::InfrastDormTask::run()
{
	if (m_controller_ptr == nullptr
		|| m_identify_ptr == nullptr)
	{
		m_callback(AsstMsg::PtrIsNull, json::value(), m_callback_arg);
		return false;
	}

	// for debug 
	//m_select_with_swipe = true;

	enter_station({"Dorm", "DormMini"}, m_dorm_begin, 0.8);

	int dorm_index = m_dorm_begin;
	for (; dorm_index < DormNum; ++dorm_index) {
		bool to_left = false;
		if (dorm_index != m_dorm_begin) {
			enter_next_dorm();
			to_left = false;
		}
		else {
			to_left = true;	// ��һ�������������Ҫ�����������һ�¡���������ᶼ������
		}
		enter_operator_selection();
		int selected = 0;
		if (m_select_with_swipe) {
			selected = select_operators_with_swipe(to_left);
		}
		else {
			selected = select_operators(to_left);
		}
		if (selected < MaxOperNumInDorm) {	// ���ѡ����5���ˣ�˵��û�и�����Ҫ��Ϣ���ˣ�ֱ�ӽ�����������
			break;
		}
	}
	m_dorm_begin = dorm_index;

	return true;
}

bool asst::InfrastDormTask::click_confirm_button()
{
	InfrastAbstractTask::click_confirm_button();
	sleep(300);

	// ����ȷ��������ѹ����еĸ�Ա�������ˣ����ٵ�����һ��ȷ�ϵĽ��棬���û�������򲻻ᵯ������ʶ��һ���پ���Ҫ��Ҫ���
	auto&& [algorithm, score, second_confirm_rect] =
		m_identify_ptr->find_image(m_controller_ptr->get_image(), "DormConfirm");
	if (score >= Configer::TemplThresholdDefault) {
		m_controller_ptr->click(second_confirm_rect);
	}
	sleep(2000);
	return true;
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
	auto find_result = m_identify_ptr->find_image(m_controller_ptr->get_image(), "StationInfoSelected");
	if (find_result.score >= 0.75) {
		m_controller_ptr->click(find_result.rect);
	}

	m_controller_ptr->swipe(swipe_down_begin, swipe_down_end, swipe_dwon_duration);

	static const Rect double_click_rect(
		Configer::WindowWidthDefault * 0.4,
		Configer::WindowHeightDefault * 0.4,
		Configer::WindowWidthDefault * 0.2,
		Configer::WindowWidthDefault * 0.2
	);

	// ��Ϸ�е������˫�������õ�ǰ��ʩ�ص���ȷ��λ��
	m_controller_ptr->click(double_click_rect);
	m_controller_ptr->click(double_click_rect);

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
		m_controller_ptr->click(station_info_iter->rect);
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
	m_controller_ptr->click(button_iter->rect);
	sleep(3000);

	return true;
}

int asst::InfrastDormTask::select_operators(const cv::Mat& image)
{
	// ʶ����Ϣ�С��ĸ�Ա
	auto resting_result = m_identify_ptr->find_all_images(image, "Resting", 0.8);
	if (resting_result.size() == MaxOperNumInDorm) {	// ��������˶�����Ϣ����������᲻�û��ֱ࣬�ӹ���
		return MaxOperNumInDorm;
	}

	// ʶ��ע������ɢ���ĸ�Ա
	// TODO�������ֵ̫���ˣ�����������ʱ���ٵ���һ��ģ��ͼƬ
	auto listless_result = m_identify_ptr->find_all_images(image, "Listless", 0.6);
	// ʶ�����ڹ����еĸ�Ա������״̬
	auto work_mood_result = detect_mood_status_at_work(image, Configer::get_instance().m_infrast_options.dorm_threshold);

	int count = 0;
	// �ѡ���Ϣ�С��ĸ�Ա�����ٴ�ѡ��
	for (const auto& resting : resting_result) {
		m_controller_ptr->click(resting.rect);
		++count;
	}

	if (listless_result.size() == 0 && work_mood_result.size() == 0) {	// ���û��ע������ɢ�ĺ�����͵ģ�Ҳֱ�ӹ���
		return 0;
	}

	// ѡ��ע������ɢ���ĸ�Ա
	for (const auto& listless : listless_result) {
		if (count++ >= MaxOperNumInDorm) {
			break;
		}
		m_controller_ptr->click(listless.rect);
	}
	// ѡ������ϵ͵ĸ�Ա
	for (const auto& mood_status : work_mood_result) {
		if (count++ >= MaxOperNumInDorm) {
			break;
		}
		m_controller_ptr->click(mood_status.rect);
	}
	return count;
}

int asst::InfrastDormTask::select_operators(bool need_to_the_left)
{
	if (need_to_the_left) {
		swipe_to_the_left();
	}

	cv::Mat image = m_controller_ptr->get_image();
	click_clear_button();
	int count = select_operators(image);
	if (count == 0) {
		click_return_button();
	}
	else {
		click_confirm_button();
	}

	return count;
}

int asst::InfrastDormTask::select_operators_with_swipe(bool need_to_the_left)
{
	if (need_to_the_left) {
		swipe_to_the_left();
	}
	cv::Mat image = m_controller_ptr->get_image();
	click_clear_button();

	int count = 0;
	while (true) {
		count += select_operators(image);

		if (count >= MaxOperNumInDorm) {
			break;
		}
			 
		auto rest_result = m_identify_ptr->find_image(image, "SmileyAtRest");
		if (rest_result.score > Configer::TemplThresholdDefault) {
			break;
		}
		swipe();
		image = m_controller_ptr->get_image();
	}

	if (count == 0) {
		click_return_button();
	}
	else {
		click_confirm_button();
	}

	return count;
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
		if (mood_status.process == 0) {
			// ֵΪ0˵���ǡ�ע������ɢ������ɫ�����������ʶ��ɻ�ɫЦ���ˣ�����ֱ�Ӻ������ֵ
		}
		else if (mood_status.process <= process_threshold) {	// ����С����ֵ��ֱ�Ӽ�����
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