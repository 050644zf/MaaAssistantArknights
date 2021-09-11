#include "InfrastPowerTask.h"

#include "WinMacro.h"
#include "Identify.h"
#include "Configer.h"

bool asst::InfrastPowerTask::run()
{
	if (m_controller_ptr == nullptr
		|| m_identify_ptr == nullptr)
	{
		m_callback(AsstMsg::PtrIsNull, json::value(), m_callback_arg);
		return false;
	}

	bool is_the_left = false;
	for (int i = 0; i != PowerNum; ++i) {
		enter_station({ "Power", "PowerMini"}, i);
		sleep(1000);
		if (enter_operator_selection()) {
			if (is_the_left) {
				select_operators(false);
			}
			else {
				select_operators(true);
				is_the_left = true;
			}
		}
		sleep(1000);
		click_return_button();
		sleep(1000);
	}
    return true;
}

bool asst::InfrastPowerTask::enter_operator_selection()
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
	}

	cv::Mat image = m_controller_ptr->get_image();
	auto goin_result = m_identify_ptr->find_image(image, "GoIn");
	// ����ҵ���GoIn������פ����ť���������ȥ��׼��ѡ���Ա
	if (goin_result.score > Configer::TemplThresholdDefault) {
		m_controller_ptr->click(goin_result.rect);
		sleep(1000);
		return true;
	}
	else {
		return false;	// ����˵���������վ�������ڵģ����û���
	}
}

int asst::InfrastPowerTask::select_operators(bool need_to_the_left)
{
	if (need_to_the_left) {
		swipe_to_the_left();
	}
	// ����վ��Ա������ʶ��ֱ��ѡ���һ������
	click_first_operator();
	click_confirm_button();

	return 1;
}
