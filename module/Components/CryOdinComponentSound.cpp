#include "StdAfx.h"
#include "OdinComponents/CryOdinComponentSound.h"

#include "Core/CryOdin.h"

#include <CryEntitySystem/IEntity.h>
#include <CrySchematyc/IObject.h>

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			struct SOdinVoiceTalkingSignal
			{
				SOdinVoiceTalkingSignal() = default;
				explicit SOdinVoiceTalkingSignal(bool bTalking) : m_bTalking(bTalking) {}

				bool m_bTalking = false;
			};

			static void ReflectType(Schematyc::CTypeDesc<SOdinVoiceTalkingSignal>& desc)
			{
				desc.SetGUID("{62A8890A-81BB-4A87-A480-CF5A95D3F4E9}"_cry_guid);
				desc.SetLabel("On Talking Changed");
				desc.AddMember(&SOdinVoiceTalkingSignal::m_bTalking, 'talk', "Talking", "Talking", "True while this user talks", false);
			}

			static void RegisterOdinSoundComponent(Schematyc::IEnvRegistrar& registrar)
			{
				Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
				{
					Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CCryOdinComponentSound));

					componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SOdinVoiceTalkingSignal));
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinComponentSound::BindToUserIdSchematyc, "{CC0297E1-5C00-45B2-8E53-5BBBA09C2E0F}"_cry_guid, "BindToUserId");
						pFunction->SetDescription("Plays the voice of the user with this id from this entity. An empty room keeps the Room Id property");
						pFunction->BindInput(1, 'user', "User Id");
						pFunction->BindInput(2, 'room', "Room Id");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinComponentSound::Unbind, "{406E1ACC-49FE-4946-B044-3DE137C73897}"_cry_guid, "Unbind");
						pFunction->SetDescription("Stops playing the user's voice from this entity");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinComponentSound::SetVolume, "{F90D6880-6140-4053-9E95-11D6AC663A7F}"_cry_guid, "SetVolume");
						pFunction->SetDescription("Sets the volume of this voice");
						pFunction->BindInput(1, 'volu', "Volume");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinComponentSound::SetMuted, "{8FC370B7-B4F4-40C1-9B1C-C01A520B3212}"_cry_guid, "SetMuted");
						pFunction->SetDescription("Mutes this voice on this client");
						pFunction->BindInput(1, 'mute', "Muted");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinComponentSound::SetRadioEnabled, "{20C8FD24-DB76-4D3B-9D60-DB8DC81624BB}"_cry_guid, "SetRadioEnabled");
						pFunction->SetDescription("Plays this voice through the radio overlay");
						pFunction->BindInput(1, 'radi', "Enabled");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinComponentSound::SetRadioPreset, "{E9FE96C0-8CB6-4855-B910-2B6E114E1442}"_cry_guid, "SetRadioPreset");
						pFunction->SetDescription("Uses a radio preset, and turns the radio on");
						pFunction->BindInput(1, 'pres', "Preset");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinComponentSound::SetPositioning, "{E8E23F72-725F-47B8-919D-22ED8DA28564}"_cry_guid, "SetPositioning");
						pFunction->SetDescription("Chooses between 2D and 3D playback");
						pFunction->BindInput(1, 'posi', "Positioning");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinComponentSound::SetOcclusionEnabled, "{490AD351-C257-4FCE-9ED3-3E6B541A3073}"_cry_guid, "SetOcclusionEnabled");
						pFunction->SetDescription("Muffles the voice when geometry is in the way");
						pFunction->BindInput(1, 'occl', "Enabled");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinComponentSound::IsTalking, "{00874B65-2E3A-44D9-8615-896BD3A64E3D}"_cry_guid, "IsTalking");
						pFunction->SetDescription("True while this user talks");
						pFunction->BindOutput(0, 'talk', "Talking");
						componentScope.Register(pFunction);
					}
				}
			}

			CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterOdinSoundComponent);
		}

		CCryOdinComponentSound::~CCryOdinComponentSound()
		{
			UnregisterListener();
		}

		void CCryOdinComponentSound::Initialize()
		{
			RegisterListener();
			FindUser();
		}

		void CCryOdinComponentSound::OnShutDown()
		{
			ReleaseUser();
			UnregisterListener();
		}

		void CCryOdinComponentSound::RegisterListener()
		{
			CCryOdin* pOdin = CCryOdin::Get();
			if (pOdin != nullptr && !m_bListening)
			{
				pOdin->AddListener(*this, "CCryOdinComponentSound");
				m_bListening = true;
			}
		}

		void CCryOdinComponentSound::UnregisterListener()
		{
			if (m_bListening)
			{
				m_bListening = false;
				if (CCryOdin* pOdin = CCryOdin::Get())
				{
					pOdin->RemoveListener(*this);
				}
			}
		}

		Cry::Entity::EventFlags CCryOdinComponentSound::GetEventMask() const
		{
			Cry::Entity::EventFlags flags = Cry::Entity::EEvent::EditorPropertyChanged | Cry::Entity::EEvent::Reset;
			// No need to run every frame until there's a voice to move.
			if (m_pUser != nullptr)
			{
				flags |= Cry::Entity::EEvent::Update;
			}
			return flags;
		}

		void CCryOdinComponentSound::ProcessEvent(const SEntityEvent& event)
		{
			if (event.event == Cry::Entity::EEvent::Update)
			{
				if (!gEnv->IsEditing())
				{
					SendTransform();
				}
			}
			else if (event.event == Cry::Entity::EEvent::EditorPropertyChanged)
			{
				ApplySettings();
				FindUser();
			}
			else if (event.event == Cry::Entity::EEvent::Reset)
			{
				// Each game mode session in the Sandbox starts from the properties, even after an Unbind.
				if (event.nParam[0] == 0)
				{
					m_bWantsUser = true;
				}
			}
		}

		void CCryOdinComponentSound::SendTransform()
		{
			ICryOdinSound* pSound = GetSound();
			if (pSound == nullptr)
			{
				return;
			}

			const Matrix34& matrix = m_pEntity->GetWorldTM();
			pSound->SetTransform(matrix.GetTranslation(), matrix.GetColumn1());
		}

		void CCryOdinComponentSound::SendTalkingSignal(bool bTalking)
		{
			if (Schematyc::IObject* pObject = m_pEntity->GetSchematycObject())
			{
				pObject->ProcessSignal(SOdinVoiceTalkingSignal(bTalking), GetGUID());
			}
		}

		void CCryOdinComponentSound::ApplySettings()
		{
			if (ICryOdinSound* pSound = GetSound())
			{
				pSound->SetIgnoreRoomDefaults(true);
				pSound->SetSettings(m_settings);
				pSound->SetOcclusionIgnoreEntity(m_pEntity->GetId());
			}
		}

		void CCryOdinComponentSound::FindUser()
		{
			CCryOdin* pOdin = CCryOdin::Get();
			if (pOdin == nullptr || m_pUser != nullptr || !m_bWantsUser || m_userId.empty())
			{
				return;
			}

			for (int i = 0; i < pOdin->GetRoomCount(); ++i)
			{
				ICryOdinRoom* pRoom = pOdin->GetRoom(i);
				if (m_roomId.empty() || strcmp(m_roomId.c_str(), pRoom->GetRoomId()) == 0)
				{
					if (ICryOdinUser* pUser = pRoom->FindPeerByUserId(m_userId.c_str()))
					{
						BindToUser(*pUser);
						return;
					}
				}
			}
		}

		void CCryOdinComponentSound::BindToUserId(const char* szUserId, const char* szRoomId)
		{
			ReleaseUser();
			m_bWantsUser = true;
			m_userId = szUserId != nullptr ? szUserId : "";
			if (szRoomId != nullptr)
			{
				m_roomId = szRoomId;
			}
			RegisterListener();
			FindUser();
		}

		void CCryOdinComponentSound::BindToUser(ICryOdinUser& user)
		{
			// An entity waiting to be deleted can't add itself to the update list again.
			if (m_pUser == &user || m_pEntity->IsGarbage())
			{
				return;
			}

			ReleaseUser();
			RegisterListener();

			m_pUser = &user;
			m_bWantsUser = true;

			ApplySettings();
			SendTransform();

			m_pEntity->UpdateComponentEventMask(this);
		}

		void CCryOdinComponentSound::Unbind()
		{
			m_bWantsUser = false;
			ReleaseUser();
		}

		void CCryOdinComponentSound::ReleaseUser()
		{
			if (m_pUser == nullptr)
			{
				return;
			}

			if (ICryOdinSound* pSound = GetSound())
			{
				pSound->SetIgnoreRoomDefaults(false);
				pSound->SetSettings(m_pUser->GetRoom().GetDefaultVoiceSettings());
				pSound->SetOcclusionIgnoreEntity(INVALID_ENTITYID);
			}

			m_pUser = nullptr;
			m_pEntity->UpdateComponentEventMask(this);
		}

		ICryOdinSound* CCryOdinComponentSound::GetSound() const
		{
			return m_pUser != nullptr ? m_pUser->GetSound() : nullptr;
		}

		void CCryOdinComponentSound::SetSettings(const SCryOdinVoiceSettings& settings)
		{
			m_settings = settings;
			ApplySettings();
		}

		void CCryOdinComponentSound::SetVolume(float fVolume)
		{
			m_settings.fVolume = fVolume;
			ApplySettings();
		}

		void CCryOdinComponentSound::SetMuted(bool bMuted)
		{
			m_settings.bMuted = bMuted;
			ApplySettings();
		}

		void CCryOdinComponentSound::SetPositioning(ECryOdinVoicePositioning positioning)
		{
			m_settings.positioning = positioning;
			ApplySettings();
		}

		void CCryOdinComponentSound::SetRadioEnabled(bool bEnabled)
		{
			m_settings.radio.bEnabled = bEnabled;
			ApplySettings();
		}

		void CCryOdinComponentSound::SetRadioPreset(ECryOdinRadioPreset preset)
		{
			if (preset != ECryOdinRadioPreset::Custom)
			{
				const bool bDisableSpatialization = m_settings.radio.bDisableSpatialization;
				m_settings.radio = GetCryOdinRadioPreset(preset);
				m_settings.radio.bDisableSpatialization = bDisableSpatialization;
			}
			m_settings.radio.bEnabled = true;
			ApplySettings();
		}

		void CCryOdinComponentSound::SetRadioSettings(const SCryOdinRadioSettings& settings)
		{
			m_settings.radio = settings;
			ApplySettings();
		}

		void CCryOdinComponentSound::SetOcclusionEnabled(bool bEnabled)
		{
			m_settings.bOcclusion = bEnabled;
			ApplySettings();
		}

		bool CCryOdinComponentSound::IsTalking() const
		{
			return m_pUser != nullptr && m_pUser->IsTalking();
		}

		void CCryOdinComponentSound::OnPeerJoined(ICryOdinRoom& room, ICryOdinUser& user)
		{
			if (m_pUser == nullptr && m_bWantsUser && !m_userId.empty() && strcmp(m_userId.c_str(), user.GetUserId()) == 0
				&& (m_roomId.empty() || strcmp(m_roomId.c_str(), room.GetRoomId()) == 0))
			{
				BindToUser(user);
			}
		}

		void CCryOdinComponentSound::OnPeerLeft(ICryOdinRoom& room, ICryOdinUser& user)
		{
			if (m_pUser == &user)
			{
				if (user.IsTalking())
				{
					SendTalkingSignal(false);
				}
				m_pUser = nullptr;
				m_pEntity->UpdateComponentEventMask(this);

				// A restarted client can have joined again before its old session timed out.
				FindUser();
			}
		}

		void CCryOdinComponentSound::OnLeftRoom(ICryOdinRoom& room, const char* szReason)
		{
			if (m_pUser != nullptr && &m_pUser->GetRoom() == &room)
			{
				m_pUser = nullptr;
				m_pEntity->UpdateComponentEventMask(this);
			}
		}

		void CCryOdinComponentSound::OnPeerTalkingChanged(ICryOdinRoom& room, ICryOdinUser& user, bool bTalking)
		{
			if (m_pUser == &user)
			{
				SendTalkingSignal(bTalking);
			}
		}
	}
}
