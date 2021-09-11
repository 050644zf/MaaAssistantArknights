﻿#include "AsstCaller.h"

#include <string.h>

#include <json_value.h>

#include "Updater.h"
#include "AsstAux.h"
#include "Assistance.h"

#if 0
#if _MSC_VER
// Win32平台下Dll的入口
BOOL APIENTRY DllMain(HANDLE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
) {
	UNREFERENCED_PARAMETER(hModule);
	UNREFERENCED_PARAMETER(lpReserved);
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
#elif VA_GNUC

#endif
#endif

AsstCallback _callback = nullptr;

void CallbackTrans(asst::AsstMsg msg, const json::value& json, void* custom_arg)
{
	_callback(static_cast<int>(msg), asst::Utf8ToGbk(json.to_string()).c_str(), custom_arg);
}

asst::Assistance* AsstCreate()
{
	return new asst::Assistance();
}

MEOAPI_PORT asst::Assistance* AsstCreateEx(AsstCallback callback, void* custom_arg)
{
	// 创建多实例回调会有问题，有空再慢慢整
	_callback = callback;
	return new asst::Assistance(CallbackTrans, custom_arg);
}

void AsstDestory(asst::Assistance* p_asst)
{
	if (p_asst == NULL) {
		return;
	}

	delete p_asst;
	p_asst = NULL;
}

bool AsstCatchEmulator(asst::Assistance* p_asst)
{
	if (p_asst == NULL) {
		return false;
	}

	auto&& ret = p_asst->catch_emulator();
	if (ret) {
		return true;
	}
	else {
		return false;
	}
}

bool AsstStart(asst::Assistance* p_asst, const char* task)
{
	if (p_asst == NULL) {
		return false;
	}

	return p_asst->start_match_task(task, asst::Assistance::ProcessTaskRetryTimesDefault);
}

void AsstStop(asst::Assistance* p_asst)
{
	if (p_asst == NULL) {
		return;
	}

	p_asst->stop();
}

bool AsstSetParam(asst::Assistance* p_asst, const char* type, const char* param, const char* value)
{
	if (p_asst == NULL) {
		return false;
	}

	return p_asst->set_param(type, param, value);
}

bool AsstRunOpenRecruit(asst::Assistance* p_asst, const int required_level[], int required_len, bool set_time)
{
	if (p_asst == NULL) {
		return false;
	}
	std::vector<int> level_vector;
	level_vector.assign(required_level, required_level + required_len);
	p_asst->start_open_recruit(level_vector, set_time);
	return true;
}

bool AsstStartInfrast(asst::Assistance* p_asst)
{
	if (p_asst == NULL) {
		return false;
	}
	p_asst->start_infrast();
	return true;
}

MEOAPI_PORT bool AsstStartIndertifyOpers(asst::Assistance* p_asst)
{
	if (p_asst == NULL) {
		return false;
	}
	p_asst->start_to_identify_opers();
	return true;
}

bool AsstDebugTask(asst::Assistance* p_asst)
{
	if (p_asst == NULL) {
		return false;
	}
#if LOG_TRACE
	p_asst->start_debug_task();
#endif // LOG_TRACE

	return true;
}

bool CheckVersionUpdate(char* tag_buffer, int tag_bufsize, char* html_url_buffer, int html_bufsize, char* body_buffer, int body_bufsize)
{
	bool ret = asst::Updater::get_instance().has_new_version();
	if (!ret) {
		return false;
	}
	const asst::VersionInfo& info = asst::Updater::get_instance().get_version_info();
	strcpy_s(tag_buffer, tag_bufsize, info.tag_name.c_str());
	strcpy_s(html_url_buffer, html_bufsize, info.html_url.c_str());
	strcpy_s(body_buffer, body_bufsize, info.body.c_str());
	return true;
}