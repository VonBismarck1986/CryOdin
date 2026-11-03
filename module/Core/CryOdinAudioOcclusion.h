#pragma once

#include "Utils/OdinHelpers.h"
#include <CryEntitySystem/IEntityBasicTypes.h>
#include <vector>

namespace Cry
{
	namespace Odin
	{
		class CCryOdinSound;

		// Casts rays from the listener to the voices that have occlusion turned on, a few per frame.
		// Main thread only.
		class CCryOdinOcclusion final
		{
		public:
			DISABLE_COPY_AND_MOVE(CCryOdinOcclusion)
			CCryOdinOcclusion() = default;
			~CCryOdinOcclusion() = default;

			void AddSound(CCryOdinSound* pSound);
			void RemoveSound(CCryOdinSound* pSound);

			void Update(const Vec3& listenerPosition, EntityId ignoreEntity, int raysPerFrame, float fPerHitOcclusion);

		private:
			float CastRay(const Vec3& from, const Vec3& to, EntityId ignoreEntityA, EntityId ignoreEntityB, float fPerHitOcclusion) const;

			std::vector<CCryOdinSound*> m_sounds;
			size_t m_nextSound = 0;
		};
	}
}
