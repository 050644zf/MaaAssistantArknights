#pragma once

#include "InfrastAbstractTask.h"

namespace asst {
	// ������פ����
	class InfrastStationTask : public InfrastAbstractTask
	{
	public:
		using InfrastAbstractTask::InfrastAbstractTask;
		virtual ~InfrastStationTask() = default;

		virtual bool run() override;

		void set_all_opers_info(std::unordered_map<std::string, OperInfrastInfo> infos) {
			m_all_opers_info = std::move(infos);
		}
	protected:
		constexpr static int SwipeMaxTimes = 17;
		constexpr static int MaxOpers = 3;	// ��������վ/ó��վ����Ա��

		// һ�߻���һ��ʶ��
		std::optional<std::unordered_map<std::string, OperInfrastInfo>> swipe_and_detect();
		// �������Ž��Ա���
		std::list<std::string> calc_optimal_comb(const std::unordered_map<std::string, OperInfrastInfo>& cur_opers_info) const;
		// һ�߻���һ��ʶ�𲢵����Ա��
		bool swipe_and_select(std::list<std::string>& name_comb, int swipe_max_times = SwipeMaxTimes);

		std::string m_facility;	// ��ʩ��������վ��ó��վ���������࡭����
		std::unordered_map<std::string, OperInfrastInfo> m_all_opers_info;
	};
}