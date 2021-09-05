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
	sleep(1000);
	enter_operator_selection();
	sleep(1000);
	clear_and_select_the_resting();

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

	m_control_ptr->swipe(swipe_down_begin, swipe_down_end, swipe_dwon_duration);

	static const Rect double_click_rect(
		Configer::WindowWidthDefault * 0.4,
		Configer::WindowHeightDefault * 0.4,
		Configer::WindowWidthDefault * 0.2,
		Configer::WindowWidthDefault * 0.2
	);

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

	return true;
}

bool asst::InfrastDormTask::clear_and_select_the_resting()
{
	auto click_clear_button = [&]() {
		// ��������ѡ�񡱰�ť
		m_control_ptr->click(Rect(430, 655, 150, 40));
		sleep(300);
	};
	cv::Mat image = get_format_image();
	// �첽�����հ�ť
	std::future<void> click_clear_button_future = std::async(std::launch::async, click_clear_button);
	// ʶ����Ϣ�С��ĸ�Ա
	auto resting_result = m_identify_ptr->find_all_images(image, "Resting", 0.8);
	click_clear_button_future.wait();

	// �ѡ���Ϣ�С��ĸ�Ա�����ٴ�ѡ��
	for (const auto& resting : resting_result) {
		m_control_ptr->click(resting.rect);
	}

	return true;
}
