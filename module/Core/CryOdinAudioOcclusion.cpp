#include "StdAfx.h"
#include "CryOdinAudioOcclusion.h"

#include "CryOdinAudioSound.h"

#include <CryEntitySystem/IEntity.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryPhysics/physinterface.h>

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			constexpr int kMaxOcclusionHits = 8;
			constexpr float kMinimumRayLength = 0.25f;
			// Stops short of both ends, so the speaker's or listener's surroundings don't count.
			constexpr float kRayEndOffset = 0.1f;

			IPhysicalEntity* GetPhysicsOfEntity(EntityId entityId)
			{
				if (entityId != INVALID_ENTITYID && gEnv->pEntitySystem != nullptr)
				{
					if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId))
					{
						return pEntity->GetPhysics();
					}
				}
				return nullptr;
			}
		}

		void CCryOdinOcclusion::AddSound(CCryOdinSound* pSound)
		{
			m_sounds.push_back(pSound);
		}

		void CCryOdinOcclusion::RemoveSound(CCryOdinSound* pSound)
		{
			stl::find_and_erase(m_sounds, pSound);
		}

		void CCryOdinOcclusion::Update(const Vec3& listenerPosition, EntityId ignoreEntity, int raysPerFrame, float fPerHitOcclusion)
		{
			if (m_sounds.empty() || gEnv->pPhysicalWorld == nullptr)
			{
				return;
			}

			// Look at each voice once at most, and skip the ones that don't want a ray without using up the budget.
			int raysLeft = raysPerFrame;
			for (size_t checked = 0; checked < m_sounds.size() && raysLeft > 0; ++checked)
			{
				m_nextSound = (m_nextSound + 1) % m_sounds.size();
				CCryOdinSound* pSound = m_sounds[m_nextSound];

				if (pSound->WantsOcclusionRays())
				{
					pSound->SetOcclusionFromRays(CastRay(listenerPosition, pSound->GetPosition(), ignoreEntity, pSound->GetOcclusionIgnoreEntity(), fPerHitOcclusion));
					--raysLeft;
				}
			}
		}

		float CCryOdinOcclusion::CastRay(const Vec3& from, const Vec3& to, EntityId ignoreEntityA, EntityId ignoreEntityB, float fPerHitOcclusion) const
		{
			if (!IsFiniteVec3(from) || !IsFiniteVec3(to))
			{
				return 0.f;
			}

			const Vec3 direction = to - from;
			const float fLength = direction.GetLength();
			if (fLength < kMinimumRayLength)
			{
				return 0.f;
			}

			const Vec3 unitDirection = direction / fLength;
			const Vec3 origin = from + unitDirection * kRayEndOffset;
			const Vec3 ray = unitDirection * (fLength - 2.f * kRayEndOffset);

			// With rwi_max_piercing the first slot is reserved for a solid hit, and the others follow it.
			ray_hit hits[kMaxOcclusionHits];
			const int objectTypes = ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid;
			const int hitCount = gEnv->pPhysicalWorld->RayWorldIntersection(origin, ray, objectTypes, rwi_max_piercing, hits, kMaxOcclusionHits,
				GetPhysicsOfEntity(ignoreEntityA), GetPhysicsOfEntity(ignoreEntityB));

			float fOcclusion = 0.f;
			for (int i = 0; i < hitCount && i + 1 < kMaxOcclusionHits; ++i)
			{
				if (hits[i + 1].dist > 0.f)
				{
					fOcclusion += fPerHitOcclusion;
				}
			}
			return std::min(fOcclusion, 1.f);
		}
	}
}
