#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include <array>
#include "CAnimationData.h"
#include "noncopyable.h"

class MotiondataManager : NonCopyable{
	static inline std::unordered_map<std::string, std::unique_ptr<CAnimationData>> m_motiondatacontainer{};
public:

	static bool RegisterMotion(std::string key, std::unique_ptr<CAnimationData> motiondata){
		// ‘¶İ‚µ‚Ä‚éê‡‚Í‰½‚à‚µ‚È‚¢
		if (m_motiondatacontainer.contains(key)) return false;

		// ‘¶İ‚µ‚Ä‚¢‚È‚¯‚ê‚Î“o˜^
		m_motiondatacontainer[key] = std::move(motiondata);
		return true;
	}

	static CAnimationData* getMotion(std::string key) {
		return m_motiondatacontainer[key].get();
	}
};
