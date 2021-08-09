#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <ostream>
#include <functional>

namespace json
{
	class value;
}

namespace asst {

	enum class HandleType
	{
		Window = 1,
		View = 2,
		Control = 4
	};

	static std::ostream& operator<<(std::ostream& os, const HandleType& type)
	{
		static std::unordered_map<HandleType, std::string> _type_name = {
			{HandleType::Window, "Window"},
			{HandleType::View, "View"},
			{HandleType::Control, "Control"}
		};
		return os << _type_name.at(type);
	}

	enum class MatchTaskType {
		Invalid = 0,
		BasicClick = 0x100,
		ClickSelf = BasicClick | 1,		// ���ģ������λ��
		ClickRect = BasicClick | 2,		// ���ָ������
		ClickRand = BasicClick | 4,		// ����������
		DoNothing = 0x200,				// ʲô������
		Stop =  0x400,					// ֹͣ�����߳�
		PrintWindow =  0x800,			// ��ͼ
		Ocr =  0x1000					// ����ʶ������
	};

	enum class AlgorithmType {
		JustReturn,
		MatchTemplate,
		CompareHist,
		OcrDetect
	};

	struct Point
	{
		Point() = default;
		Point(int x, int y) : x(x), y(y) {}
		int x = 0;
		int y = 0;
	};

	struct Rect
	{
		Rect() = default;
		Rect(int x, int y, int width, int height)
			: x(x), y(y), width(width), height(height) {}
		Rect operator*(double rhs) const
		{
			return { x, y, static_cast<int>(width * rhs), static_cast<int>(height * rhs) };
		}
		Rect center_zoom(double scale) const
		{
			int half_width_scale = static_cast<int>(width * (1 - scale) / 2);
			int half_hight_scale = static_cast<int>(height * (1 - scale) / 2);
			return { x + half_width_scale, y + half_hight_scale, width - half_width_scale,  height - half_hight_scale };
		}
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;
	};

	struct TextArea {
		TextArea() = default;
		TextArea(const TextArea&) = default;
		TextArea(TextArea&&) = default;
		template<typename ...RectArgs>
		TextArea(std::string text, RectArgs &&... rect_args)
			: text(std::move(text)),
			rect(std::forward<RectArgs>(rect_args)...) {
			static_assert(std::is_constructible<asst::Rect, RectArgs...>::value,
				"Parameter can't be used to construct a asst::Rect");
		}
		operator std::string() const { return text; }
		std::string text;
		Rect rect;
	};

	struct HandleInfo {
		std::string class_name;
		std::string window_name;
	};

	struct AdbCmd {
		std::string path;
		std::string connect;
		std::string click;
		std::string display;
		std::string display_regex;
		int display_width = 0;
		int display_height = 0;
	};

	struct EmulatorInfo {
		std::string name;
		std::vector<HandleInfo> window;
		std::vector<HandleInfo> view;
		std::vector<HandleInfo> control;
		bool is_adb = false;
		AdbCmd adb;
		int width = 0;
		int height = 0;
		int x_offset = 0;
		int y_offset = 0;
		int right_offset = 0;
		int bottom_offset = 0;
	};


	struct TaskInfo {
		virtual ~TaskInfo() = 0;
		std::string name;								// ������
		MatchTaskType type = MatchTaskType::Invalid;	// ��������
		std::vector<std::string> next;					// ��һ�����ܵ������б�
		int exec_times = 0;								// ������ִ���˶��ٴ�
		int max_times = INT_MAX;						// �������ִ�ж��ٴ�
		std::vector<std::string> exceeded_next;			// �ﵽ��������֮����һ�����ܵ������б�
		std::vector<std::string> reduce_other_times;	// ִ���˸��������Ҫ���ٱ�������ִ�д���������ִ���˳�����ҩ����˵����һ�ε����ɫ��ʼ�ж���ťû��Ч��������ɫ��ʼ�ж�Ҫ-1
		asst::Rect specific_area;						// ָ������Ŀǰ�����ClickRect�������ã�����������
		int pre_delay = 0;								// ִ�и�����ǰ����ʱ
		int rear_delay = 0;								// ִ�и���������ʱ
		int retry_times = INT_MAX;						// δ�ҵ�ͼ��ʱ�����Դ���
	};

	struct OcrTaskInfo : public TaskInfo {
		virtual ~OcrTaskInfo() = default;
		std::vector<std::string> text;					// ���ֵ�������ƥ�䵽��������һ��������ƥ������
		bool need_match = false;						// �Ƿ���Ҫȫƥ�䣬�����������Ӵ�����ƥ������
		std::unordered_map<std::string, std::string>
			replace_map;								// ������������ʶ����ַ���ǿ��replace֮���ٽ���ƥ��
	};

	struct MatchTaskInfo : public TaskInfo {
		virtual ~MatchTaskInfo() = default;
		std::string template_filename;					// ƥ��ģ��ͼƬ�ļ���
		double templ_threshold = 0;						// ģ��ƥ����ֵ
		double hist_threshold = 0;						// ֱ��ͼ�Ƚ���ֵ
	};

	struct Options {
		bool identify_cache = false;		// ͼ��ʶ�𻺴湦�ܣ���������Դ������CPU���ģ�����Ҫ��֤Ҫʶ��İ�ťÿ�ε�λ�ò���ı�
		int task_identify_delay = 0;		// ʶ���������ʱ��Խ�����Խ�죬��������CPU����
		int task_control_delay = 0;			// ���������ӳ٣�Խ����Խ�죬����̫���˾�����ܲ���Ӧ
		int control_delay_lower = 0;		// ��������ʱ���ޣ�ÿ�ε����������������ʱ
		int control_delay_upper = 0;		// ��������ʱ���ޣ�ÿ�ε����������������ʱ
		bool print_window = false;			// ��ͼ���ܣ�������ÿ�ν��������ͼ��screenshotĿ¼��
		int print_window_delay = 0;			// ��ͼ��ʱ��ÿ�ε�������棬������Ʒ����һ���Գ����ģ��и�������������Ҫ��һ���ٽ�ͼ
		int print_window_crop_offset = 0;	// ��ͼ����ü����ٶ���ѱ߿�ü���һȦ����Ȼ��������п���ʶ�𲻳���
		int ocr_gpu_index = -1;				// OcrLiteʹ��GPU��ţ�-1(ʹ��CPU)/0(ʹ��GPU0)/1(ʹ��GPU1)/...
		int ocr_thread_number = 0;			// OcrLite�߳�����
	};

	// ��Ա��Ϣ
	struct OperInfo {
		std::string name;
		std::string type;
		int level = 0;
		std::string sex;
		std::unordered_set<std::string> tags;
		bool hidden = false;
		std::string name_en;
	};

	// ��Ա���
	struct OperCombs {
		std::vector<OperInfo> opers;
		int max_level = 0;
		int min_level = 0;
		double avg_level = 0;
	};

	constexpr double DoubleDiff = 1e-12;
}