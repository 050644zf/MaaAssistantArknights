#include "WinMacro.h"

#include <vector>
#include <utility>
#include <ctime>
#include <cassert>
#include <algorithm>

#include <stdint.h>
#include <WinUser.h>

#include <iostream>

using namespace MeoAssistance;

WinMacro::WinMacro(HandleType type)
    : m_handle_type(type),
    m_rand_engine(time(NULL))
{
}

bool WinMacro::findHandle()
{
    switch (m_handle_type) {
    case HandleType::BlueStacksControl: {
        HWND temp_handle = ::FindWindow(L"BS2CHINAUI", L"BlueStacks App Player");
        temp_handle = ::FindWindowEx(temp_handle, NULL, L"BS2CHINAUI", L"HOSTWND");
        m_handle = ::FindWindowEx(temp_handle, NULL, L"WindowsForms10.Window.8.app.0.34f5582_r6_ad1", L"BlueStacks Android PluginAndroid");
    } break;
    case HandleType::BlueStacksView:
    case HandleType::BlueStacksWindow:
        m_handle = ::FindWindow(L"BS2CHINAUI", L"BlueStacks App Player");
        break;
    default:
        std::cerr << "handle type error! " << static_cast<int>(m_handle_type) << std::endl;
        break;
    }

#ifdef _DEBUG
    std::cout << "type: " << static_cast<int>(m_handle_type) << ", handle: " << m_handle << std::endl;
#endif

    if (m_handle != NULL) {
        return true;
    }
    else {
        return false;
    }
}

bool WinMacro::resizeWindow(int width, int height)
{
    if (!(static_cast<int>(m_handle_type) & static_cast<int>(HandleType::Window))) {
        return false;
    }

    return ::MoveWindow(m_handle, 0, 0, width, height, true);
}

double WinMacro::getScreenScale()
{
    // ��ȡ���ڵ�ǰ��ʾ�ļ�����
    // ʹ������ľ��.
    HWND hWnd = GetDesktopWindow();
    HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);

    // ��ȡ�������߼������߶�
    MONITORINFOEX miex;
    miex.cbSize = sizeof(miex);
    GetMonitorInfo(hMonitor, &miex);
    int cxLogical = (miex.rcMonitor.right - miex.rcMonitor.left);
    int cyLogical = (miex.rcMonitor.bottom - miex.rcMonitor.top);

    // ��ȡ��������������߶�
    DEVMODE dm;
    dm.dmSize = sizeof(dm);
    dm.dmDriverExtra = 0;
    EnumDisplaySettings(miex.szDevice, ENUM_CURRENT_SETTINGS, &dm);
    int cxPhysical = dm.dmPelsWidth;
    int cyPhysical = dm.dmPelsHeight;

    // ����״̬����С���߼��ߴ���ʵ��С
    double horzScale = ((double)cxPhysical / (double)cxLogical);
    double vertScale = ((double)cyPhysical / (double)cyLogical);

    // ����״̬����С��ѡ���������Ǹ�
    return std::max(horzScale, vertScale);
}

bool WinMacro::click(Point p)
{
    if (!(static_cast<int>(m_handle_type) & static_cast<int>(HandleType::Control))) {
        return false;
    }

#ifdef _DEBUG
    std::cout << "click: " << p.x << ", " << p.y << std::endl;
#endif

	LPARAM lparam = MAKELPARAM(p.x, p.y);

	::SendMessage(m_handle, WM_LBUTTONDOWN, MK_LBUTTON, lparam);
	::SendMessage(m_handle, WM_LBUTTONUP, 0, lparam);

    return true;
}

bool WinMacro::clickRange(Rect rect)
{
    if (!(static_cast<int>(m_handle_type) & static_cast<int>(HandleType::Control))) {
        return false;
    }

    int x = 0, y = 0;
    if (rect.width == 0) {
        x = rect.x;
    }
    else {
        std::poisson_distribution<int> x_rand(rect.width);
        x = x_rand(m_rand_engine) + rect.x;

    }
    if (rect.height == 0) {
        y = rect.y;
    }
    else {
        std::poisson_distribution<int> y_rand(rect.height);
        int y = y_rand(m_rand_engine) + rect.y;
    }

    return click({ x, y });
}

Rect WinMacro::getWindowRect()
{
    RECT rect;
    bool ret = ::GetWindowRect(m_handle, &rect);
    if (!ret) {
        return { 0, 0, 0 ,0 };
    }
    double scale = getScreenScale();
    return Rect{ rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top } * scale;
}

cv::Mat WinMacro::getImage(Rect rect)
{
    if (!(static_cast<int>(m_handle_type) & static_cast<int>(HandleType::View))) {
        return cv::Mat();
    }

    HDC pDC;// ԴDC
    //�ж��ǲ��Ǵ��ھ������ǵĻ�����ʹ��GetDC����ȡDC ��Ȼ��ͼ���Ǻ���
    if (m_handle == ::GetDesktopWindow())
    {
        pDC = CreateDCA("DISPLAY", NULL, NULL, NULL);
    }
    else
    {
        pDC = ::GetDC(m_handle);//��ȡ��ĻDC(0Ϊȫ���������Ϊ����)
    }
    int BitPerPixel = ::GetDeviceCaps(pDC, BITSPIXEL);//�����ɫģʽ
    if (rect.width == 0 && rect.height == 0)//Ĭ�Ͽ�Ⱥ͸߶�Ϊȫ��
    {
        rect.width = ::GetDeviceCaps(pDC, HORZRES); //����ͼ����ȫ��
        rect.height = ::GetDeviceCaps(pDC, VERTRES); //����ͼ��߶�ȫ��
    }
    HDC memDC;//�ڴ�DC
    memDC = ::CreateCompatibleDC(pDC);
    HBITMAP memBitmap, oldmemBitmap;//��������Ļ���ݵ�bitmap
    memBitmap = ::CreateCompatibleBitmap(pDC, rect.width, rect.height);
    oldmemBitmap = (HBITMAP)::SelectObject(memDC, memBitmap);//��memBitmapѡ���ڴ�DC
    if (m_handle == ::GetDesktopWindow())
    {
        BitBlt(memDC, 0, 0, rect.width, rect.height, pDC, rect.x, rect.y, SRCCOPY);//ͼ���ȸ߶Ⱥͽ�ȡλ��
    }
    else
    {
        bool bret = ::PrintWindow(m_handle, memDC, PW_CLIENTONLY);
        if (!bret)
        {
            BitBlt(memDC, 0, 0, rect.width, rect.height, pDC, rect.x, rect.y, SRCCOPY);//ͼ���ȸ߶Ⱥͽ�ȡλ��
        }
    }

    BITMAP bmp;
    GetObject(memBitmap, sizeof(BITMAP), &bmp);
    int nChannels = bmp.bmBitsPixel == 1 ? 1 : bmp.bmBitsPixel / 8;
    cv::Mat dst_mat;
    dst_mat.create(cv::Size(bmp.bmWidth, bmp.bmHeight), CV_MAKETYPE(CV_8U, nChannels));
    GetBitmapBits(memBitmap, bmp.bmHeight * bmp.bmWidth * nChannels, dst_mat.data);

    DeleteObject(memBitmap);
    DeleteDC(memDC);
    ReleaseDC(m_handle, pDC);

#ifdef _DEBUG
    // ��ȡ����ǰ·��
    char curpath[_MAX_PATH] = { 0 };
    ::GetModuleFileNameA(NULL, curpath, _MAX_PATH);
    std::string filename(curpath);
    filename = filename.substr(0, filename.find_last_of('\\')) + "\\test.bmp";

    cv::imwrite(filename, dst_mat);
#endif

    return dst_mat;
}