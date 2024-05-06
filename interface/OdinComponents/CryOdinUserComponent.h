#pragma once

#include <ICryOdinUser.h>
#include <CryNetwork/INetwork.h>
#include <CryNetwork/INetEntity.h>

#include <OdinComponents/CryOdinComponentSound.h>

namespace Cry
{
	namespace Odin
	{
		class CCryOdinUserComponent
			: public IEntityComponent
		{
		public:
			CCryOdinUserComponent() = default;
			virtual ~CCryOdinUserComponent() = default;

		protected:
			virtual void Initialize() override;
			virtual Cry::Entity::EventFlags GetEventMask() const override;
			virtual void ProcessEvent(const SEntityEvent& event) override;

		public:
			// Reflect type to set a unique identifier for this component
			static void ReflectType(Schematyc::CTypeDesc<CCryOdinUserComponent>& desc)
			{
				desc.SetGUID("{16EB0B3D-747B-479A-BFF1-581881DFA9FC}"_cry_guid);
				desc.SetLabel("OdinUserComponent");
				desc.SetEditorCategory("Odin");
			}

			virtual string GetUserName() const { return m_UserName; }
			virtual uint64_t GetUserId() const;
			virtual uint64_t GetPeerId() const { return m_PeerID; }
			virtual const char* RoomName() const { return m_RoomName.c_str(); }
			virtual string GetStatus() const { return m_OnlineStatus; }

			virtual OdinMediaStreamHandle GetMediaHandle(EAudioHandleType type) const;
			virtual OdinRoomHandle GetRoomHandle() const { return m_room; }

			virtual string ToStringDebug() const;

			virtual void SetMediaHandle(EAudioHandleType type, OdinMediaStreamHandle handle);
			virtual void SetRoomHandle(OdinRoomHandle room);


			void SetPeerId(uint64_t id) { m_PeerID = id; }
			void SetOnlineStatus(string stat) { m_OnlineStatus = stat; }
			void SetUserName(string userName) { m_UserName = userName; }
			void SetRoomName(const char* room_name) { m_RoomName = room_name; }

			// TODO:: Maybe redo below doesn't seem right to me since we passing pointers and dangly pointer could cause issues

			void SetSoundComponent(CCryOdinComponentSound* ref) { m_pSoundComponent.reset(std::move(ref)); }
			std::shared_ptr<CCryOdinComponentSound> GetSoundComponent() { return m_pSoundComponent; }
		private:
			mutable OdinMediaStreamHandle m_mediaHandles[3]; // 1 is input , 2 is output , 3 is ignored
			mutable OdinRoomHandle m_room;

			mutable string m_UserName;
			mutable uint64_t m_PeerID;
			mutable uint64_t m_UserID;

			CryFixedStringT<128> m_RoomName;
			string m_debugString;
			mutable string m_OnlineStatus;

			bool m_gameplayStarted = false;

			std::shared_ptr<CCryOdinComponentSound> m_pSoundComponent = nullptr;
		};
	}
}