#pragma once

#include <ostream>
#include <memory>
#include <unordered_map>
#include <functional>

#include <json.h>

namespace asst {
	enum class AsstMsg {
		/* Error Msg */
		PtrIsNull,
		ImageIsEmpty,
		WindowMinimized,
		InitFaild,
		/* Info Msg */
		TaskStart,
		ImageFindResult,
		ImageMatched,
		TaskMatched,
		ReachedLimit,
		ReadyToSleep,
		EndOfSleep,
		AppendProcessTask,	// �����ϢAssistance�����������ⲿ����Ҫ����
		AppendTask,			// �����ϢAssistance�����������ⲿ����Ҫ����
		TaskCompleted,
		PrintWindow,
		TaskStop,
		TaskError,
		/* Open Recruit Msg */
		TextDetected,
		RecruitTagsDetected,
		OcrResultError,
		RecruitSpecialTag,
		RecruitResult,
		/* Infrast Msg*/
		OpersDetected,		// ʶ���˸�Աs
		OpersIdtfResult,	// ��Աʶ�������ܵģ�
		InfrastComb			// ��ǰ���õ����Ÿ�Ա���
	};

	static std::ostream& operator<<(std::ostream& os, const AsstMsg& type)
	{
		static const std::unordered_map<AsstMsg, std::string> _type_name = {
			{AsstMsg::PtrIsNull, "PtrIsNull"},
			{AsstMsg::ImageIsEmpty, "ImageIsEmpty"},
			{AsstMsg::WindowMinimized, "WindowMinimized"},
			{AsstMsg::InitFaild, "InitFaild"},
			{AsstMsg::TaskStart, "TaskStart"},
			{AsstMsg::ImageFindResult, "ImageFindResult"},
			{AsstMsg::ImageMatched, "ImageMatched"},
			{AsstMsg::TaskMatched, "TaskMatched"},
			{AsstMsg::ReachedLimit, "ReachedLimit"},
			{AsstMsg::ReadyToSleep, "ReadyToSleep"},
			{AsstMsg::EndOfSleep, "EndOfSleep"},
			{AsstMsg::AppendProcessTask, "AppendProcessTask"},
			{AsstMsg::TaskCompleted, "TaskCompleted"},
			{AsstMsg::PrintWindow, "PrintWindow"},
			{AsstMsg::TaskError, "TaskError"},
			{AsstMsg::TaskStop, "TaskStop"},
			{AsstMsg::TextDetected, "TextDetected"},
			{AsstMsg::RecruitTagsDetected, "RecruitTagsDetected"},
			{AsstMsg::OcrResultError, "OcrResultError"},
			{AsstMsg::RecruitSpecialTag, "RecruitSpecialTag"},
			{AsstMsg::RecruitResult, "RecruitResult"},
			{AsstMsg::AppendTask, "AppendTask"},
			{AsstMsg::OpersDetected, "OpersDetected"},
			{AsstMsg::OpersIdtfResult, "OpersIdtfResult"},
			{AsstMsg::InfrastComb, "InfrastComb"}
		};
		return os << _type_name.at(type);
	}

	// AsstCallback ��Ϣ�ص�����
	// ������
	// AsstMsg ��Ϣ����
	// const json::value& ��Ϣ����json��ÿ����Ϣ��ͬ��Todo����Ҫ�����Э���ĵ�ɶ��
	// void* �ⲿ�������Զ��������ÿ�λص������ȥ�����鴫��(void*)thisָ�����
	using AsstCallback = std::function<void(AsstMsg, const json::value&, void*)>;
}