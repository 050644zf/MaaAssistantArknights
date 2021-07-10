#include "WinMacro.h"

#include <vector>
#include <utility>
#include <ctime>

#include <stdint.h>
#include <WinUser.h>

#ifdef _DEBUG
#include <iostream>
#endif

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
    std::cout << "handle: " << m_handle << std::endl;
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

bool WinMacro::getImage(Rect rect)
{
    if (!(static_cast<int>(m_handle_type) & static_cast<int>(HandleType::View))) {
        return false;
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

#ifdef _DEBUG
    //���´��뱣��memDC�е�λͼ���ļ�
    BITMAP bmp;
    ::GetObject(memBitmap, sizeof(BITMAP), &bmp);;//���λͼ��Ϣ
    FILE* fp;
    fopen_s(&fp, "D:\\Code\\MeoAssistance\\Debug\\test.bmp", "w+b");//ͼƬ����·���ͷ�ʽ

    BITMAPINFOHEADER bih = { 0 };//λͼ��Ϣͷ
    bih.biBitCount = bmp.bmBitsPixel;//ÿ�������ֽڴ�С
    bih.biCompression = BI_RGB;
    bih.biHeight = bmp.bmHeight;//�߶�
    bih.biPlanes = 1;
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biSizeImage = bmp.bmWidthBytes * bmp.bmHeight;//ͼ�����ݴ�С
    bih.biWidth = bmp.bmWidth;//���

    BITMAPFILEHEADER bfh = { 0 };//λͼ�ļ�ͷ
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);//��λͼ���ݵ�ƫ����
    bfh.bfSize = bfh.bfOffBits + bmp.bmWidthBytes * bmp.bmHeight;//�ļ��ܵĴ�С
    bfh.bfType = (WORD)0x4d42;

    fwrite(&bfh, 1, sizeof(BITMAPFILEHEADER), fp);//д��λͼ�ļ�ͷ
    fwrite(&bih, 1, sizeof(BITMAPINFOHEADER), fp);//д��λͼ��Ϣͷ
    byte* p = new byte[bmp.bmWidthBytes * bmp.bmHeight];//�����ڴ汣��λͼ����
    GetDIBits(memDC, (HBITMAP)memBitmap, 0, rect.height, p,
        (LPBITMAPINFO)&bih, DIB_RGB_COLORS);//��ȡλͼ����
    fwrite(p, 1, bmp.bmWidthBytes * bmp.bmHeight, fp);//д��λͼ����
    delete[] p;
    fclose(fp);
#endif

    DeleteObject(memBitmap);
    DeleteDC(memDC);
    ReleaseDC(m_handle, pDC);

    return true;
}