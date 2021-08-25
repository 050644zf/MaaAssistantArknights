#pragma once

#include <string>
#include <random>
#include <optional>

#include <Windows.h>

#include "AsstDef.h"

namespace cv {
	class Mat;
}

namespace asst {
	class WinMacro
	{
	public:
		WinMacro(const EmulatorInfo& info, HandleType type);
		~WinMacro() = default;

		bool captured() const noexcept;
		bool resizeWindow(int Width, int Height);
		bool resizeWindow();	// by configer
		bool showWindow();
		bool hideWindow();
		bool click(const Point& p);
		bool click(const Rect& rect);
		bool swipe(const Point& p1, const Point& p2, int duration = 0);
		bool swipe(const Rect& r1, const Rect& r2, int duration = 0);
		void setControlScale(double scale);
		[[ deprecated ]] cv::Mat getImage(const Rect& rect);	// ͨ��Win32 Api�Դ��ڽ�ͼ
		cv::Mat getAdbImage();				// ͨ��Adb��ͼ�������һ�㣬���ǱȽ�����ͨ��adb pull��������io������
		Rect getWindowRect();
		const EmulatorInfo& getEmulatorInfo() const noexcept { return m_emulator_info; }
		const HandleType& getHandleType() const noexcept { return m_handle_type; }

		static double getScreenScale();
	private:
		bool findHandle();
		std::optional<std::string> callCmd(const std::string& cmd, bool use_pipe = true);
		Point randPointInRect(const Rect& rect);

		const EmulatorInfo m_emulator_info;
		const HandleType m_handle_type;
		HWND m_handle = NULL;
		bool m_is_adb = false;
		std::string m_click_cmd;		// adb����������adb�ľ���ò������
		std::string m_swipe_cmd;		// adb�����������adb�ľ���ò������
		std::string m_screencap_cmd;	// adb��ͼ�������adb�ľ���ò������
		std::string m_pullscreen_cmd;	// adb��ȡ��ͼ�������adb�ľ���ò������
		const std::string m_adb_screen_filename;
		std::minstd_rand m_rand_engine;
		int m_width = 0;
		int m_height = 0;
		//int m_x_offset = 0;
		//int m_y_offset = 0;
		double m_control_scale = 1;
	};
}
