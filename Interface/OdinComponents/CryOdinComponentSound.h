#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Utils/SharedString.h>

#include <ICryOdin.h>

namespace Cry
{
	namespace Odin
	{
		//! Plays the voice of a user from this entity. Add it to the entity of each remote player.
		//! Tell it who the entity is with the User Id property or with BindToUserId, using the
		//! same id the user gave in SCryOdinRoomParams::szUserId. With an OdinNetworkComponent on the
		//! same entity, the server's user id for that player is used by itself.
		// Not final. Calls on a final class skip the vtable, and a game DLL couldn't link them.
		class CCryOdinComponentSound
			: public IEntityComponent
			, public ICryOdin::IListener
		{
		public:
			CCryOdinComponentSound() = default;
			virtual ~CCryOdinComponentSound();

		protected:
			virtual void Initialize() override;
			virtual Cry::Entity::EventFlags GetEventMask() const override;
			virtual void ProcessEvent(const SEntityEvent& event) override;
			virtual void OnShutDown() override;

			// ICryOdin::IListener
			virtual void OnLeftRoom(ICryOdinRoom& room, const char* szReason) override;
			virtual void OnPeerJoined(ICryOdinRoom& room, ICryOdinUser& user) override;
			virtual void OnPeerLeft(ICryOdinRoom& room, ICryOdinUser& user) override;
			virtual void OnPeerTalkingChanged(ICryOdinRoom& room, ICryOdinUser& user, bool bTalking) override;
			// ~ICryOdin::IListener

		public:
			// Reflect type to set a unique identifier for this component
			static void ReflectType(Schematyc::CTypeDesc<CCryOdinComponentSound>& desc)
			{
				desc.SetGUID("{0DB50E3F-243A-4732-B582-3F143841667C}"_cry_guid);
				desc.SetLabel("OdinComponentSound");
				desc.SetEditorCategory("Odin");
				desc.SetDescription("Plays the voice of a user from this entity");
				desc.SetComponentFlags({ IEntityComponent::EFlags::ClientOnly });
				desc.AddMember(&CCryOdinComponentSound::m_userId, 'user', "UserId", "User Id", "The id the user joined with", "");
				desc.AddMember(&CCryOdinComponentSound::m_roomId, 'room', "RoomId", "Room Id", "Only match this room, leave empty for any room", "");
				desc.AddMember(&CCryOdinComponentSound::m_settings, 'voic', "Voice", "Voice", "How the voice is played", SCryOdinVoiceSettings());
				desc.AddMember(&CCryOdinComponentSound::m_fVoiceHeight, 'vhgt', "VoiceHeight", "Voice Height", "Meters above the entity's origin the voice comes from, about mouth height for a player. 0 for an entity already at the mouth", 1.6f);
			}

			//! Meters above the entity's origin the voice comes from.
			virtual void SetVoiceHeight(float fHeight) { m_fVoiceHeight = fHeight; }
			virtual float GetVoiceHeight() const { return m_fVoiceHeight; }

			//! Plays this user's voice from the entity. Finds the user now or when they join.
			//! @param szRoomId Limits the match to one room, empty matches any room. Null keeps the Room Id property.
			virtual void BindToUserId(const char* szUserId, const char* szRoomId = nullptr);
			//! Plays this user's voice from the entity.
			virtual void BindToUser(ICryOdinUser& user);
			//! Stops using this entity, and doesn't look for the user again until told to.
			//! The voice goes back to the room's default settings.
			virtual void Unbind();

			//! The user currently bound, or null.
			virtual ICryOdinUser* GetUser() const { return m_pUser; }
			//! The user id this entity plays or waits for, from the User Id property, BindToUserId or an
			//! OdinNetworkComponent on the same entity. Empty for none.
			virtual const char* GetUserId() const { return m_userId.c_str(); }
			//! The voice currently bound, or null.
			virtual ICryOdinSound* GetSound() const;

			virtual void SetSettings(const SCryOdinVoiceSettings& settings);
			virtual const SCryOdinVoiceSettings& GetSettings() const { return m_settings; }

			virtual void SetVolume(float fVolume);
			virtual float GetVolume() const { return m_settings.fVolume; }

			virtual void SetMuted(bool bMuted);
			virtual bool IsMuted() const { return m_settings.bMuted; }

			virtual void SetPositioning(ECryOdinVoicePositioning positioning);

			virtual void SetRadioEnabled(bool bEnabled);
			virtual bool IsRadioEnabled() const { return m_settings.radio.bEnabled; }
			//! Uses a preset, and turns the radio on. Custom keeps the current values.
			virtual void SetRadioPreset(ECryOdinRadioPreset preset);
			virtual void SetRadioSettings(const SCryOdinRadioSettings& settings);

			virtual void SetOcclusionEnabled(bool bEnabled);

			virtual bool IsTalking() const;

			// Used by Schematyc
			void BindToUserIdSchematyc(const Schematyc::CSharedString& userId, const Schematyc::CSharedString& roomId) { BindToUserId(userId.c_str(), roomId.empty() ? nullptr : roomId.c_str()); }

		private:
			void ReleaseUser();
			void ApplySettings();
			void FindUser();
			void SendTransform();
			void SendTalkingSignal(bool bTalking);
			void RegisterListener();
			void UnregisterListener();

			Schematyc::CSharedString m_userId;
			Schematyc::CSharedString m_roomId;
			SCryOdinVoiceSettings m_settings;
			float m_fVoiceHeight = 1.6f;

			ICryOdinUser* m_pUser = nullptr;
			bool m_bWantsUser = true;
			bool m_bListening = false;
		};
	}
}
