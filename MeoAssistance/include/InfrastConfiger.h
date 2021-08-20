#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "AsstDef.h"

namespace asst {
	class InfrastConfiger
	{
	public:
		~InfrastConfiger() = default;

		static InfrastConfiger& get_instance() {
			static InfrastConfiger unique_instance;
			return unique_instance;
		}

		bool load(const std::string& filename);
		std::unordered_set<std::string> m_all_opers_name;				// ���и�Ա������
		std::unordered_map<std::string, std::string> m_oper_name_feat;	// ���ݹؼ�����Ҫ��������Ա�������OCRʶ����key�����ݵ���ȴû��value�����ݣ��������������һ��ȷ��
		std::unordered_set<std::string> m_oper_name_feat_whatever;		// ������ζ������������ĸ�Ա��

		std::unordered_map<std::string, std::vector<std::vector<OperInfrastInfo>>> m_infrast_combs;		// ������ʩ�ڵĿ��ܸ�Ա���

	private:
		InfrastConfiger() = default;
		InfrastConfiger(const InfrastConfiger& rhs) = default;
		InfrastConfiger(InfrastConfiger&& rhs) noexcept = default;

		InfrastConfiger& operator=(const InfrastConfiger& rhs) = default;
		InfrastConfiger& operator=(InfrastConfiger && rhs) noexcept = default;

		bool _load(const std::string& filename);
	};
}
