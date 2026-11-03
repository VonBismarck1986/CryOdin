#include "StdAfx.h"
#include "OdinComponents/CryOdinUserComponent.h"
#include "OdinComponents/CryOdinNetworkComponent.h"

#include "Core/CryOdin.h"
#include "Core/CryOdinAudioSystem.h"
#include "Utils/OdinJson.h"

#include <CryEntitySystem/IEntity.h>
#include <CrySchematyc/IObject.h>

namespace Cry
{
	namespace Odin
	{
		namespace
		{
			constexpr float kPositionUpdateInterval = 0.1f;
			// How often the encoder sends the position while silent, with Send Position on.
			constexpr int kPositionKeepAliveMs = 500;
			// How long a token from the server may take.
			constexpr float kServerTokenTimeoutSeconds = 15.f;

			struct SOdinJoinedSignal
			{
				SOdinJoinedSignal() = default;
				SOdinJoinedSignal(const char* szRoomId, const char* szUserId) : m_roomId(szRoomId), m_userId(szUserId) {}
				Schematyc::CSharedString m_roomId;
				Schematyc::CSharedString m_userId;
			};

			struct SOdinLeftSignal
			{
				SOdinLeftSignal() = default;
				SOdinLeftSignal(const char* szRoomId, const char* szReason) : m_roomId(szRoomId), m_reason(szReason) {}
				Schematyc::CSharedString m_roomId;
				Schematyc::CSharedString m_reason;
			};

			struct SOdinPeerSignal
			{
				SOdinPeerSignal() = default;
				SOdinPeerSignal(const char* szName, const char* szUserId) : m_name(szName), m_userId(szUserId) {}
				Schematyc::CSharedString m_name;
				Schematyc::CSharedString m_userId;
			};

			struct SOdinPeerJoinedSignal : public SOdinPeerSignal
			{
				using SOdinPeerSignal::SOdinPeerSignal;
			};

			struct SOdinPeerLeftSignal : public SOdinPeerSignal
			{
				using SOdinPeerSignal::SOdinPeerSignal;
			};

			struct SOdinPeerChangedSignal : public SOdinPeerSignal
			{
				using SOdinPeerSignal::SOdinPeerSignal;
			};

			struct SOdinTalkingSignal
			{
				SOdinTalkingSignal() = default;
				SOdinTalkingSignal(const char* szName, const char* szUserId, bool bLocal, bool bTalking) : m_name(szName), m_userId(szUserId), m_bLocal(bLocal), m_bTalking(bTalking) {}
				Schematyc::CSharedString m_name;
				Schematyc::CSharedString m_userId;
				bool m_bLocal = false;
				bool m_bTalking = false;
			};

			struct SOdinTextSignal
			{
				SOdinTextSignal() = default;
				SOdinTextSignal(const char* szName, const char* szUserId, const char* szText, const char* szChannel) : m_name(szName), m_userId(szUserId), m_text(szText), m_channel(szChannel) {}
				Schematyc::CSharedString m_name;
				Schematyc::CSharedString m_userId;
				Schematyc::CSharedString m_text;
				Schematyc::CSharedString m_channel;
			};

			struct SOdinPushToTalkSignal
			{
				SOdinPushToTalkSignal() = default;
				explicit SOdinPushToTalkSignal(bool bPressed) : m_bPressed(bPressed) {}
				bool m_bPressed = false;
			};

			struct SOdinJoinFailedSignal
			{
				SOdinJoinFailedSignal() = default;
				SOdinJoinFailedSignal(const char* szRoomId, const char* szReason) : m_roomId(szRoomId), m_reason(szReason) {}
				Schematyc::CSharedString m_roomId;
				Schematyc::CSharedString m_reason;
			};

			static void ReflectType(Schematyc::CTypeDesc<SOdinJoinFailedSignal>& desc)
			{
				desc.SetGUID("{09C669CD-CF69-4554-918B-78683841BF3A}"_cry_guid);
				desc.SetLabel("On Join Failed");
				desc.AddMember(&SOdinJoinFailedSignal::m_roomId, 'room', "RoomId", "Room Id", "Name of the room", "");
				desc.AddMember(&SOdinJoinFailedSignal::m_reason, 'reas', "Reason", "Reason", "Why no token came from the server", "");
			}

			static void ReflectType(Schematyc::CTypeDesc<SOdinPushToTalkSignal>& desc)
			{
				desc.SetGUID("{E0210971-D1C4-4179-87F5-9B953F91D965}"_cry_guid);
				desc.SetLabel("On Push To Talk Changed");
				desc.AddMember(&SOdinPushToTalkSignal::m_bPressed, 'pres', "Pressed", "Pressed", "True while talking in Push To Talk mode, from the key or SetPushToTalk", false);
			}

			static void ReflectType(Schematyc::CTypeDesc<SOdinJoinedSignal>& desc)
			{
				desc.SetGUID("{8FA2E2A8-C6AC-427E-8A02-8783893A90A8}"_cry_guid);
				desc.SetLabel("On Joined Room");
				desc.AddMember(&SOdinJoinedSignal::m_roomId, 'room', "RoomId", "Room Id", "Name of the room", "");
				desc.AddMember(&SOdinJoinedSignal::m_userId, 'user', "UserId", "User Id", "The id of the local user, for other players to bind this player's voice with", "");
			}

			static void ReflectType(Schematyc::CTypeDesc<SOdinLeftSignal>& desc)
			{
				desc.SetGUID("{E9C4CA07-913C-4F22-BF4B-9978C39AFFF8}"_cry_guid);
				desc.SetLabel("On Left Room");
				desc.AddMember(&SOdinLeftSignal::m_roomId, 'room', "RoomId", "Room Id", "Name of the room", "");
				desc.AddMember(&SOdinLeftSignal::m_reason, 'reas', "Reason", "Reason", "Why the room ended", "");
			}

			static void ReflectType(Schematyc::CTypeDesc<SOdinPeerJoinedSignal>& desc)
			{
				desc.SetGUID("{CBEB004D-92E7-4933-BE78-C5B7BC722823}"_cry_guid);
				desc.SetLabel("On Peer Joined");
				desc.AddMember(&SOdinPeerJoinedSignal::m_name, 'name', "Name", "Name", "Name of the user", "");
				desc.AddMember(&SOdinPeerJoinedSignal::m_userId, 'user', "UserId", "User Id", "Id of the user", "");
			}

			static void ReflectType(Schematyc::CTypeDesc<SOdinPeerLeftSignal>& desc)
			{
				desc.SetGUID("{28002776-E832-4935-A527-A45195823875}"_cry_guid);
				desc.SetLabel("On Peer Left");
				desc.AddMember(&SOdinPeerLeftSignal::m_name, 'name', "Name", "Name", "Name of the user", "");
				desc.AddMember(&SOdinPeerLeftSignal::m_userId, 'user', "UserId", "User Id", "Id of the user", "");
			}

			static void ReflectType(Schematyc::CTypeDesc<SOdinPeerChangedSignal>& desc)
			{
				desc.SetGUID("{8B360037-B53B-4DA6-8618-B6063387208F}"_cry_guid);
				desc.SetLabel("On Peer User Data Changed");
				desc.AddMember(&SOdinPeerChangedSignal::m_name, 'name', "Name", "Name", "Name of the user", "");
				desc.AddMember(&SOdinPeerChangedSignal::m_userId, 'user', "UserId", "User Id", "Id of the user", "");
			}

			static void ReflectType(Schematyc::CTypeDesc<SOdinTalkingSignal>& desc)
			{
				desc.SetGUID("{B831A80F-2AAB-484B-93FF-895CF2C36B3F}"_cry_guid);
				desc.SetLabel("On Talking Changed");
				desc.AddMember(&SOdinTalkingSignal::m_name, 'name', "Name", "Name", "Name of the user", "");
				desc.AddMember(&SOdinTalkingSignal::m_userId, 'user', "UserId", "User Id", "Id of the user", "");
				desc.AddMember(&SOdinTalkingSignal::m_bLocal, 'locl', "Local", "Local", "True for the local user", false);
				desc.AddMember(&SOdinTalkingSignal::m_bTalking, 'talk', "Talking", "Talking", "True while the user talks", false);
			}

			static void ReflectType(Schematyc::CTypeDesc<SOdinTextSignal>& desc)
			{
				desc.SetGUID("{77EC2D8C-8ED8-4828-B1C8-2D02F1C99062}"_cry_guid);
				desc.SetLabel("On Text Message");
				desc.AddMember(&SOdinTextSignal::m_name, 'name', "Name", "Name", "Name of the sender", "");
				desc.AddMember(&SOdinTextSignal::m_userId, 'user', "UserId", "User Id", "Id of the sender", "");
				desc.AddMember(&SOdinTextSignal::m_text, 'text', "Text", "Text", "The message", "");
				desc.AddMember(&SOdinTextSignal::m_channel, 'chan', "Channel", "Channel", "The channel given by the sender", "");
			}

			static void RegisterOdinUserComponent(Schematyc::IEnvRegistrar& registrar)
			{
				Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
				{
					Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CCryOdinUserComponent));

					componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SOdinJoinedSignal));
					componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SOdinLeftSignal));
					componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SOdinPeerJoinedSignal));
					componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SOdinPeerLeftSignal));
					componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SOdinPeerChangedSignal));
					componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SOdinTalkingSignal));
					componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SOdinTextSignal));
					componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SOdinPushToTalkSignal));
					componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SOdinJoinFailedSignal));
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::JoinSchematyc, "{FEE8F8CD-D061-45C4-89B1-502937B01215}"_cry_guid, "Join");
						pFunction->SetDescription("Joins the room set in the properties");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::Leave, "{5AE22176-27ED-4FD1-A85A-CDDC84BE272D}"_cry_guid, "Leave");
						pFunction->SetDescription("Leaves the room");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::SendTextSchematyc, "{CA8C0ED5-464C-4F6D-A059-2A47C774206E}"_cry_guid, "SendText");
						pFunction->SetDescription("Sends a text message to all users in the room");
						pFunction->BindInput(1, 'text', "Text");
						pFunction->BindInput(2, 'chan', "Channel");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::SetPushToTalkSchematyc, "{2F6BE12D-CDD6-4DEF-8C6B-011CF94F73D4}"_cry_guid, "SetPushToTalk");
						pFunction->SetDescription("Talks or stops talking in Push To Talk mode, for your own input code");
						pFunction->BindInput(1, 'pres', "Pressed");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::IsPushToTalkPressedSchematyc, "{B025AFD4-C091-4432-AFE5-DBA53D1D9ACE}"_cry_guid, "IsPushToTalkPressed");
						pFunction->SetDescription("True while talking in Push To Talk mode");
						pFunction->BindOutput(0, 'pres', "Pressed");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::SetPushToTalkInputsSchematyc, "{A35D51F2-1D75-4132-A222-85A645CEE3F3}"_cry_guid, "SetPushToTalkInputs");
						pFunction->SetDescription("Changes the push to talk key, such as v and xi_shoulderl. Empty for none");
						pFunction->BindInput(1, 'keyb', "Keyboard Or Mouse");
						pFunction->BindInput(2, 'pad', "Gamepad");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::SetPushToTalkToggleSchematyc, "{25DD1F5D-524D-482F-A501-23268030AFC3}"_cry_guid, "SetPushToTalkToggle");
						pFunction->SetDescription("True: press once to talk and again to stop. False: talk while the key is held");
						pFunction->BindInput(1, 'togl', "Toggle");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::SetUseBuiltInPushToTalkKeySchematyc, "{B66ED2AC-BCD5-48CD-AD31-84C0F3311D80}"_cry_guid, "SetUseBuiltInPushToTalkKey");
						pFunction->SetDescription("Off stops listening for the push to talk key, so SetPushToTalk can be called from your own input instead");
						pFunction->BindInput(1, 'enab', "Enabled");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::SetVoiceQuality, "{966201DA-32E8-465F-8418-8F47633980AC}"_cry_guid, "SetVoiceQuality");
						pFunction->SetDescription("Chooses the bitrate of your voice in this room. A change while joined gives a short gap");
						pFunction->BindInput(1, 'qual', "Quality");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::SetVoiceBitrateKbps, "{817D6459-BF34-4BCE-ABD3-F278C9F73E72}"_cry_guid, "SetVoiceBitrate");
						pFunction->SetDescription("Sets a custom bitrate of your voice in kilobits per second, 6 to 512. A change while joined gives a short gap");
						pFunction->BindInput(1, 'kbps', "Kbps");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::SetExpectedPacketLoss, "{2A235A01-B421-4B02-8D24-CDBD0635BC81}"_cry_guid, "SetExpectedPacketLoss");
						pFunction->SetDescription("0 to 100. Higher keeps your voice clearer on a bad connection, at some cost to quality");
						pFunction->BindInput(1, 'loss', "Percent");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::GetVoiceBitrateKbpsSchematyc, "{45C5ABA6-050F-4ADA-8EBB-5D533A8E14D0}"_cry_guid, "GetVoiceBitrate");
						pFunction->SetDescription("The bitrate of your voice in kilobits per second");
						pFunction->BindOutput(0, 'kbps', "Kbps");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::SetMicrophoneMuted, "{97E350E2-D9EB-4121-A225-545EFDEEF041}"_cry_guid, "SetMicrophoneMuted");
						pFunction->SetDescription("Mutes the microphone for all rooms");
						pFunction->BindInput(1, 'mute', "Muted");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::IsJoined, "{3E96FDC1-DE1F-417C-936B-91CD55680EF3}"_cry_guid, "IsJoined");
						pFunction->SetDescription("True while joined to the room");
						pFunction->BindOutput(0, 'join', "Joined");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::IsTalking, "{3DD1FE54-1608-4F9E-B645-C76DFD86C5B6}"_cry_guid, "IsTalking");
						pFunction->SetDescription("True while the local user is sending voice");
						pFunction->BindOutput(0, 'talk', "Talking");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::SetRoomIdSchematyc, "{45DEA85C-FE08-4DC7-8349-73DD4778135E}"_cry_guid, "SetRoomId");
						pFunction->SetDescription("Sets the room used by the next Join");
						pFunction->BindInput(1, 'room', "Room Id");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::SetUserIdSchematyc, "{1EBB7452-15C7-4586-8633-5F25F21C22CD}"_cry_guid, "SetUserId");
						pFunction->SetDescription("Sets the id of this user for the next Join");
						pFunction->BindInput(1, 'user', "User Id");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::GetUserIdSchematyc, "{FC18C99B-E81A-466A-B913-3D01615D0799}"_cry_guid, "GetUserId");
						pFunction->SetDescription("The id of this user, share it so other players can bind this player's voice");
						pFunction->BindOutput(0, 'user', "User Id");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::SetDisplayNameSchematyc, "{B7638D84-2A3F-466A-8F74-66C04E7E116D}"_cry_guid, "SetDisplayName");
						pFunction->SetDescription("Sets the name shown to other users");
						pFunction->BindInput(1, 'name', "Name");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::SetRoomTokenSchematyc, "{F2C78539-3EBA-45DB-8253-D79A6C52D299}"_cry_guid, "SetRoomToken");
						pFunction->SetDescription("Sets the token from your server for the next Join");
						pFunction->BindInput(1, 'tokn', "Token");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::SetTransmitMode, "{7A66B08D-28B1-471E-A25A-E972C38F0A3E}"_cry_guid, "SetTransmitMode");
						pFunction->SetDescription("Chooses when the microphone is sent");
						pFunction->BindInput(1, 'mode', "Mode");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::SetSendPosition, "{62181E8B-3592-4002-B392-B5772E51E721}"_cry_guid, "SetSendPosition");
						pFunction->SetDescription("Lets the server skip voices too far away. Set it before joining");
						pFunction->BindInput(1, 'prox', "Send Position");
						componentScope.Register(pFunction);
					}
					{
						auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CCryOdinUserComponent::SetRoomRadioPreset, "{C64DC24D-DF1E-4D6F-88F4-6C1A3C457D04}"_cry_guid, "SetRoomRadioPreset");
						pFunction->SetDescription("Plays all voices in this room through the radio overlay. Custom turns it off");
						pFunction->BindInput(1, 'pres', "Preset");
						componentScope.Register(pFunction);
					}
				}
			}

			CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterOdinUserComponent);

			template<typename TSignal>
			void SendOdinSignal(IEntity* pEntity, const CryGUID& componentGuid, const TSignal& signal)
			{
				if (Schematyc::IObject* pObject = pEntity->GetSchematycObject())
				{
					pObject->ProcessSignal(signal, componentGuid);
				}
			}
		}

		CCryOdinUserComponent::~CCryOdinUserComponent()
		{
			if (m_bPushToTalkSubscribed)
			{
				m_bPushToTalkSubscribed = false;
				if (CCryOdin* pOdin = CCryOdin::Get())
				{
					pOdin->UnsubscribePushToTalk(*this);
				}
			}
			UnregisterListener();
		}

		bool CCryOdinUserComponent::IsMyRoom(const ICryOdinRoom& room) const
		{
			return &room == m_pRoom || std::find(m_leavingRooms.begin(), m_leavingRooms.end(), &room) != m_leavingRooms.end();
		}

		void CCryOdinUserComponent::Initialize()
		{
			RegisterListener();
			m_sessionProcessing = m_processing;
			m_appliedEncoder = m_encoder;
			m_bAppliedSendPosition = m_bSendPosition;

			// GameplayStarted only reaches entities that exist at that moment. The mode is also Game
			// while a level loads in the Sandbox, which must not join anything.
			if (m_pEntity->GetSimulationMode() == EEntitySimulationMode::Game && !gEnv->IsEditing())
			{
				StartGameplay();
			}
		}

		void CCryOdinUserComponent::OnShutDown()
		{
			// The events from leaving could make game code try to join again.
			m_bEntityShuttingDown = true;
			StopGameplay();
			UnregisterListener();
		}

		void CCryOdinUserComponent::RegisterListener()
		{
			CCryOdin* pOdin = CCryOdin::Get();
			if (pOdin != nullptr && !m_bListening)
			{
				pOdin->AddListener(*this, "CCryOdinUserComponent");
				m_bListening = true;
			}
		}

		void CCryOdinUserComponent::UnregisterListener()
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

		Cry::Entity::EventFlags CCryOdinUserComponent::GetEventMask() const
		{
			Cry::Entity::EventFlags flags = Cry::Entity::EEvent::GameplayStarted | Cry::Entity::EEvent::Reset | Cry::Entity::EEvent::BecomeLocalPlayer | Cry::Entity::EEvent::EditorPropertyChanged;
			if (m_bSendPosition || m_pendingServerJoin.requestId != 0 || m_bAutoJoinPending)
			{
				flags |= Cry::Entity::EEvent::Update;
			}
			return flags;
		}

		void CCryOdinUserComponent::ProcessEvent(const SEntityEvent& event)
		{
			if (event.event == Cry::Entity::EEvent::GameplayStarted)
			{
				// Simulating physics and AI in the Sandbox also sends this, which must not open the microphone.
				if (!gEnv->IsEditing())
				{
					StartGameplay();
					LogWhyAutoJoinWaits();
				}
			}
			else if (event.event == Cry::Entity::EEvent::BecomeLocalPlayer)
			{
				// The engine sets the local player flag only after this event, so remember it here.
				m_bBecameLocalPlayer = true;
				if (m_bGameplayStarted)
				{
					StartGameplay();
				}
			}
			else if (event.event == Cry::Entity::EEvent::EditorPropertyChanged)
			{
				// The mask depends on Send Position.
				m_pEntity->UpdateComponentEventMask(this);
				UpdatePushToTalkKey();
				// Only when it was this property that changed, which also leaves settings from JoinWithParams alone.
				if (m_encoder != m_appliedEncoder)
				{
					ApplyEncoderSettings();
				}
				// The panel already wrote the new value. Put the old one back, so SetSendPosition sees the change.
				if (m_bSendPosition != m_bAppliedSendPosition)
				{
					const bool bSendPosition = m_bSendPosition;
					m_bSendPosition = m_bAppliedSendPosition;
					SetSendPosition(bSendPosition);
				}
			}
			else if (event.event == Cry::Entity::EEvent::Reset)
			{
				// Leaving game mode in the Sandbox
				if (event.nParam[0] == 0)
				{
					StopGameplay();
					m_appliedEncoder = m_encoder;
					m_bAppliedSendPosition = m_bSendPosition;
					// The properties have been put back, and the mask depends on Send Position.
					m_pEntity->UpdateComponentEventMask(this);
				}
			}
			else if (event.event == Cry::Entity::EEvent::Update)
			{
				if (m_bAutoJoinPending && !gEnv->IsEditing())
				{
					m_bAutoJoinPending = false;
					m_pEntity->UpdateComponentEventMask(this);
					TryAutoJoin();
				}

				if (m_pendingServerJoin.requestId != 0)
				{
					m_pendingServerJoin.fTimeLeft -= event.fParam[0];
					if (m_pendingServerJoin.fTimeLeft <= 0.f)
					{
						const string roomId = m_pendingServerJoin.roomId;
						CancelServerToken();
						ODIN_WARNING("No token from the server for room '%s' after %d seconds. The server needs CryOdin, and this entity needs OdinNetworkComponent on the server as well", roomId.c_str(), static_cast<int>(kServerTokenTimeoutSeconds));
						SendJoinFailed(roomId.c_str(), "no answer from the server");
					}
				}

				if (m_bSendPosition && m_pRoom != nullptr && m_fProximityRadius > FLT_EPSILON && !gEnv->IsEditing())
				{
					m_fPositionTimer -= event.fParam[0];
					if (m_fPositionTimer <= 0.f)
					{
						m_fPositionTimer = kPositionUpdateInterval;
						m_pRoom->SetPosition(m_pEntity->GetWorldPos() / m_fProximityRadius);
					}
				}
			}
		}

		void CCryOdinUserComponent::StartGameplay()
		{
			m_bGameplayStarted = true;
			UpdatePushToTalkKey();
			// This can run from Initialize, before the entity's other components exist or have initialized.
			if (m_bAutoJoin && !m_bAutoJoinPending)
			{
				m_bAutoJoinPending = true;
				m_pEntity->UpdateComponentEventMask(this);
			}
		}

		void CCryOdinUserComponent::TryAutoJoin()
		{
			if (m_bAutoJoin && m_bGameplayStarted && !m_bEntityShuttingDown && m_pRoom == nullptr && !IsWaitingForToken() && !m_roomId.empty()
				&& IsAllowedToJoin() && (!m_bRequireToken || !m_token.empty() || HasServerTokenSource()))
			{
				Join();
			}
		}

		bool CCryOdinUserComponent::HasServerTokenSource() const
		{
			return m_pEntity->GetComponent<CCryOdinNetworkComponent>() != nullptr;
		}

		void CCryOdinUserComponent::CancelServerToken()
		{
			if (m_pendingServerJoin.requestId != 0)
			{
				m_pendingServerJoin = SPendingServerJoin();
				// The mask depends on it. Not while the entity is going away.
				if (!m_bEntityShuttingDown)
				{
					m_pEntity->UpdateComponentEventMask(this);
				}
			}
		}

		void CCryOdinUserComponent::SendJoinFailed(const char* szRoomId, const char* szReason)
		{
			SendOdinSignal(m_pEntity, GetGUID(), SOdinJoinFailedSignal(szRoomId, szReason));
		}

		void CCryOdinUserComponent::OnServerTokenReply(uint32 requestId, bool bGranted, const char* szRoomId, const char* szUserId, const char* szToken, const char* szReason)
		{
			// Another component's request, or one given up on.
			if (requestId == 0 || requestId != m_pendingServerJoin.requestId)
			{
				return;
			}

			SPendingServerJoin pending = m_pendingServerJoin;
			CancelServerToken();

			if (!bGranted)
			{
				ODIN_WARNING("The server refused a token for room '%s' to entity '%s': %s", pending.roomId.c_str(), m_pEntity->GetName(), szReason);
				SendJoinFailed(pending.roomId.c_str(), szReason);
				return;
			}

			// Copied, the network message goes away after this call.
			const string token = szToken != nullptr ? szToken : "";
			const string userId = szUserId != nullptr ? szUserId : "";

			SCryOdinRoomParams params = pending.params;
			params.szRoomId = pending.roomId.c_str();
			params.szUserName = pending.userName.c_str();
			params.szGateway = pending.gateway.c_str();
			params.szUserDataJson = pending.userDataJson.c_str();
			params.szExtraAuthJson = pending.extraAuthJson.c_str();
			params.szPassword = pending.password.c_str();
			params.szUserId = userId.c_str();
			params.szToken = token.c_str();
			if (!JoinWithParams(params) && !m_bEntityShuttingDown)
			{
				SendJoinFailed(pending.roomId.c_str(), "the token arrived, but joining failed, see the log");
			}
		}

		void CCryOdinUserComponent::LogWhyAutoJoinWaits() const
		{
			if (!m_bAutoJoin || m_pRoom != nullptr || IsWaitingForToken())
			{
				return;
			}

			if (m_roomId.empty())
			{
				ODIN_LOG("Auto Join is on for entity '%s' but its Room Id is empty", m_pEntity->GetName());
			}
			else if (!IsAllowedToJoin())
			{
				ODIN_LOG("Entity '%s' waits to join room '%s' until it's the local player. Turn off Local Player Only if it never will be", m_pEntity->GetName(), m_roomId.c_str());
			}
			else if (m_bRequireToken && m_token.empty() && !HasServerTokenSource())
			{
				ODIN_LOG("Entity '%s' waits to join room '%s' until SetRoomToken is called, or add an OdinNetworkComponent to get tokens from the server", m_pEntity->GetName(), m_roomId.c_str());
			}
		}

		void CCryOdinUserComponent::StopGameplay()
		{
			m_bGameplayStarted = false;
			if (m_bAutoJoinPending)
			{
				m_bAutoJoinPending = false;
				if (!m_bEntityShuttingDown)
				{
					m_pEntity->UpdateComponentEventMask(this);
				}
			}
			// The release of a held key won't arrive any more, and this runs with the next session in mind.
			m_bPushToTalkPressed = false;
			m_bPushToTalkSignalled = false;
			m_bBecameLocalPlayer = false;
			// A change asked for in this session mustn't replace the game's inputs in the next one.
			m_bPushToTalkReplacePending = false;
			UpdatePushToTalkKey();
			Leave();
		}

		bool CCryOdinUserComponent::IsAllowedToJoin() const
		{
			return !m_bLocalPlayerOnly || m_bBecameLocalPlayer || (m_pEntity->GetFlags() & ENTITY_FLAG_LOCAL_PLAYER) != 0;
		}

		SCryOdinAudioProcessing CCryOdinUserComponent::GetEffectiveProcessing() const
		{
			// With these modes the key or the mode alone decides, the voice detection isn't asked as well.
			SCryOdinAudioProcessing processing = m_sessionProcessing;
			if (m_transmitMode != ECryOdinTransmitMode::VoiceActivity)
			{
				processing.bVoiceActivity = false;
				processing.bVolumeGate = false;
			}
			return processing;
		}

		void CCryOdinUserComponent::UpdateTransmit()
		{
			if (m_pRoom != nullptr)
			{
				m_pRoom->SetTransmitEnabled(m_transmitMode != ECryOdinTransmitMode::PushToTalk || m_bPushToTalkPressed);
			}
		}

		bool CCryOdinUserComponent::Join(const char* szRoomId)
		{
			SCryOdinRoomParams params;
			params.szRoomId = (szRoomId != nullptr && szRoomId[0] != '\0') ? szRoomId : m_roomId.c_str();
			params.szUserName = m_userName.c_str();
			params.szUserId = m_userId.c_str();
			params.szGateway = m_gateway.c_str();
			params.processing = m_processing;
			params.encoder = m_encoder;
			params.voice = m_roomVoice;
			return JoinWithParams(params);
		}

		bool CCryOdinUserComponent::JoinWithParams(const SCryOdinRoomParams& params)
		{
			CCryOdin* pOdin = CCryOdin::Get();
			if (pOdin == nullptr || m_bEntityShuttingDown)
			{
				return false;
			}

			// A graph could try this after leaving game mode, which would open the microphone while editing.
			if (gEnv->IsEditing())
			{
				ODIN_WARNING("Entity '%s' can't join a room while editing", m_pEntity->GetName());
				return false;
			}

			// Leaving sends events, which could change what these strings point to.
			const string roomId = params.szRoomId != nullptr ? params.szRoomId : "";
			const string userName = params.szUserName != nullptr ? params.szUserName : "";
			string userId = params.szUserId != nullptr ? params.szUserId : "";
			string token = (params.szToken != nullptr && params.szToken[0] != '\0') ? string(params.szToken) : m_token;
			const string gateway = params.szGateway != nullptr ? params.szGateway : "";
			const string userData = params.szUserDataJson != nullptr ? params.szUserDataJson : "";
			const string extraAuth = params.szExtraAuthJson != nullptr ? params.szExtraAuthJson : "";
			const string password = params.szPassword != nullptr ? params.szPassword : "";

			// Sharing a room would let one component end it for the other. Checked before leaving
			// the current room, or asking the server, so a refused join changes nothing.
			const ICryOdinRoom* pExisting = pOdin->FindRoom(roomId.c_str());
			if (pExisting != nullptr && pExisting != m_pRoom)
			{
				ODIN_WARNING("Room '%s' is already in use, one component can't share it with another", roomId.c_str());
				return false;
			}

			// The server holds the access key. Its answer joins, and until then the current room stays.
			CCryOdinNetworkComponent* pNetwork = token.empty() ? m_pEntity->GetComponent<CCryOdinNetworkComponent>() : nullptr;
			if (pNetwork != nullptr)
			{
				CancelServerToken();

				uint32 requestId = 0;
				string reason;
				const CCryOdinNetworkComponent::ETokenRequest result = pNetwork->RequestRoomToken(roomId.c_str(), requestId, token, userId, reason);
				if (result == CCryOdinNetworkComponent::ETokenRequest::Failed)
				{
					ODIN_WARNING("Entity '%s' can't get a token for room '%s': %s", m_pEntity->GetName(), roomId.c_str(), reason.c_str());
					SendJoinFailed(roomId.c_str(), reason.c_str());
					return false;
				}
				if (result == CCryOdinNetworkComponent::ETokenRequest::Sent)
				{
					m_pendingServerJoin.requestId = requestId;
					m_pendingServerJoin.fTimeLeft = kServerTokenTimeoutSeconds;
					m_pendingServerJoin.params = params;
					m_pendingServerJoin.roomId = roomId;
					m_pendingServerJoin.userName = userName;
					m_pendingServerJoin.gateway = gateway;
					m_pendingServerJoin.userDataJson = userData;
					m_pendingServerJoin.extraAuthJson = extraAuth;
					m_pendingServerJoin.password = password;
					// These point into the caller's memory, which won't last until the answer.
					m_pendingServerJoin.params.szRoomId = nullptr;
					m_pendingServerJoin.params.szUserName = nullptr;
					m_pendingServerJoin.params.szUserId = nullptr;
					m_pendingServerJoin.params.szToken = nullptr;
					m_pendingServerJoin.params.szGateway = nullptr;
					m_pendingServerJoin.params.szUserDataJson = nullptr;
					m_pendingServerJoin.params.szExtraAuthJson = nullptr;
					m_pendingServerJoin.params.szPassword = nullptr;
					m_pEntity->UpdateComponentEventMask(this);

					if (pOdin->IsDebugEnabled())
					{
						ODIN_LOG("Entity '%s' asked the server for a token for room '%s'", m_pEntity->GetName(), roomId.c_str());
					}
					return true;
				}
				// Signed here, on the server or in single player. Joins right away below.
			}

			if (m_bRequireToken && token.empty())
			{
				ODIN_WARNING("Room '%s' needs a token, call SetRoomToken first or add an OdinNetworkComponent", roomId.c_str());
				return false;
			}

			Leave();
			RegisterListener();

			// An event from leaving already joined something, which takes priority.
			if (m_pRoom != nullptr)
			{
				return true;
			}
			if (pOdin->FindRoom(roomId.c_str()) != nullptr)
			{
				ODIN_WARNING("Room '%s' is already in use, one component can't share it with another", roomId.c_str());
				return false;
			}

			m_sessionProcessing = params.processing;

			SCryOdinRoomParams joinParams = params;
			joinParams.szRoomId = roomId.c_str();
			joinParams.szUserName = userName.c_str();
			joinParams.szUserId = userId.c_str();
			joinParams.szToken = token.c_str();
			joinParams.szGateway = gateway.c_str();
			joinParams.szUserDataJson = userData.c_str();
			joinParams.szExtraAuthJson = extraAuth.c_str();
			joinParams.szPassword = password.c_str();
			joinParams.processing = GetEffectiveProcessing();
			joinParams.bTransmit = m_transmitMode != ECryOdinTransmitMode::PushToTalk || m_bPushToTalkPressed;
			if (m_bSendPosition && joinParams.encoder.iPositionUpdateIntervalMs <= 0)
			{
				// Lets the server keep culling while this user is silent.
				joinParams.encoder.iPositionUpdateIntervalMs = kPositionKeepAliveMs;
			}
			m_appliedEncoder = m_encoder;
			m_bAppliedSendPosition = m_bSendPosition;

			m_pRoom = pOdin->JoinRoom(joinParams);

			// A token is only good for one join, and expires soon anyway. If it wasn't used, it's still good.
			if (m_pRoom != nullptr)
			{
				m_token.clear();
			}

			return m_pRoom != nullptr;
		}

		void CCryOdinUserComponent::Leave()
		{
			CancelServerToken();

			if (m_pRoom != nullptr)
			{
				ICryOdinRoom* pRoom = m_pRoom;
				m_leavingRooms.push_back(pRoom);
				if (CCryOdin* pOdin = CCryOdin::Get())
				{
					pOdin->LeaveRoom(pRoom);
				}
				// An event from leaving could have already joined something new.
				if (m_pRoom == pRoom)
				{
					m_pRoom = nullptr;
				}
			}
		}

		bool CCryOdinUserComponent::IsJoined() const
		{
			return m_pRoom != nullptr && m_pRoom->GetStatus() == ECryOdinRoomStatus::Joined;
		}

		ICryOdinUser* CCryOdinUserComponent::GetLocalUser() const
		{
			return m_pRoom != nullptr ? m_pRoom->GetLocalUser() : nullptr;
		}

		bool CCryOdinUserComponent::SendText(const char* szText, const char* szChannel)
		{
			return m_pRoom != nullptr && m_pRoom->SendText(szText, szChannel);
		}

		void CCryOdinUserComponent::SetPushToTalk(bool bPressed)
		{
			m_bPushToTalkPressed = bPressed;
			UpdateTransmit();

			// A listener of OnTransmitChanged can have called this again from inside UpdateTransmit.
			// Compared with the last signal sent, so the graph always ends up with the current state.
			if (m_bPushToTalkSignalled != m_bPushToTalkPressed)
			{
				m_bPushToTalkSignalled = m_bPushToTalkPressed;
				CCryOdin* pOdin = CCryOdin::Get();
				if (pOdin != nullptr && pOdin->IsDebugEnabled())
				{
					ODIN_LOG("Push to talk %s for entity '%s'", m_bPushToTalkSignalled ? "on" : "off", m_pEntity->GetName());
				}
				SendOdinSignal(m_pEntity, GetGUID(), SOdinPushToTalkSignal(m_bPushToTalkSignalled));
			}
		}

		void CCryOdinUserComponent::OnPushToTalkInput(bool bDown)
		{
			if (m_pushToTalkKey.bToggle)
			{
				// Only the press counts. An up from a key the plugin lost track of mustn't stop talking.
				if (bDown)
				{
					SetPushToTalk(!m_bPushToTalkPressed);
				}
			}
			else
			{
				SetPushToTalk(bDown);
			}
		}

		void CCryOdinUserComponent::UpdatePushToTalkKey()
		{
			CCryOdin* pOdin = CCryOdin::Get();
			const bool bWanted = pOdin != nullptr && m_pushToTalkKey.bEnabled && m_transmitMode == ECryOdinTransmitMode::PushToTalk
				&& m_bGameplayStarted && !m_bEntityShuttingDown && !gEnv->IsEditing() && IsAllowedToJoin();

			if (bWanted)
			{
				if (!m_bPushToTalkSubscribed || m_bPushToTalkReplacePending || m_subscribedKey != m_pushToTalkKey)
				{
					// Toggled on and now a hold key, which isn't held. Nothing else would stop it.
					if (m_bPushToTalkSubscribed && m_subscribedKey.bToggle && !m_pushToTalkKey.bToggle && m_bPushToTalkPressed)
					{
						SetPushToTalk(false);
					}

					pOdin->SubscribePushToTalk(*this, m_pEntity->GetName(), m_pushToTalkKey, m_bPushToTalkReplacePending);
					m_bPushToTalkSubscribed = true;
					m_bPushToTalkReplacePending = false;
					m_subscribedKey = m_pushToTalkKey;
				}
			}
			else if (m_bPushToTalkSubscribed)
			{
				m_bPushToTalkSubscribed = false;
				if (pOdin != nullptr)
				{
					pOdin->UnsubscribePushToTalk(*this);
				}
				// The release of the key can't arrive any more.
				if (m_bPushToTalkPressed)
				{
					SetPushToTalk(false);
				}
			}
		}

		void CCryOdinUserComponent::SetPushToTalkKey(const SCryOdinPushToTalkKey& key)
		{
			// Compared as the engine sees the names, so "V" for "v" doesn't throw away a key the player rebound.
			if (!key.HasSameInputs(m_pushToTalkKey))
			{
				m_bPushToTalkReplacePending = true;
			}
			m_pushToTalkKey = key;
			UpdatePushToTalkKey();
		}

		void CCryOdinUserComponent::SetPushToTalkInputs(const char* szKeyboard, const char* szGamepad)
		{
			SCryOdinPushToTalkKey key = m_pushToTalkKey;
			key.keyboard = szKeyboard != nullptr ? szKeyboard : "";
			key.gamepad = szGamepad != nullptr ? szGamepad : "";
			SetPushToTalkKey(key);
		}

		void CCryOdinUserComponent::SetPushToTalkToggle(bool bToggle)
		{
			SCryOdinPushToTalkKey key = m_pushToTalkKey;
			key.bToggle = bToggle;
			SetPushToTalkKey(key);
		}

		void CCryOdinUserComponent::SetTransmitMode(ECryOdinTransmitMode mode)
		{
			m_transmitMode = mode;
			UpdateTransmit();
			if (m_pRoom != nullptr)
			{
				m_pRoom->SetAudioProcessing(GetEffectiveProcessing());
			}
			UpdatePushToTalkKey();
		}

		void CCryOdinUserComponent::SetMicrophoneMuted(bool bMuted)
		{
			if (CCryOdin* pOdin = CCryOdin::Get())
			{
				if (ICryOdinAudioSystem* pAudio = pOdin->GetAudioSystem())
				{
					pAudio->SetMicrophoneMuted(bMuted);
				}
			}
		}

		void CCryOdinUserComponent::SetAudioProcessing(const SCryOdinAudioProcessing& processing)
		{
			m_processing = processing;
			m_sessionProcessing = processing;
			if (m_pRoom != nullptr)
			{
				m_pRoom->SetAudioProcessing(GetEffectiveProcessing());
			}
		}

		void CCryOdinUserComponent::SetRoomId(const char* szRoomId)
		{
			m_roomId = szRoomId != nullptr ? szRoomId : "";
		}

		void CCryOdinUserComponent::SetUserId(const char* szUserId)
		{
			m_userId = szUserId != nullptr ? szUserId : "";
		}

		const char* CCryOdinUserComponent::GetUserId() const
		{
			const ICryOdinUser* pUser = GetLocalUser();
			return pUser != nullptr ? pUser->GetUserId() : m_userId.c_str();
		}

		void CCryOdinUserComponent::SetRoomToken(const char* szToken)
		{
			m_token = szToken != nullptr ? szToken : "";

			// Auto Join was waiting for this.
			if (m_bRequireToken && m_bGameplayStarted)
			{
				StartGameplay();
			}
		}

		void CCryOdinUserComponent::SetRoomVoiceSettings(const SCryOdinVoiceSettings& settings)
		{
			m_roomVoice = settings;
			if (m_pRoom != nullptr)
			{
				m_pRoom->SetDefaultVoiceSettings(m_roomVoice, true);
			}
		}

		void CCryOdinUserComponent::SetRoomRadioPreset(ECryOdinRadioPreset preset)
		{
			SCryOdinVoiceSettings settings = m_pRoom != nullptr ? m_pRoom->GetDefaultVoiceSettings() : m_roomVoice;
			if (preset == ECryOdinRadioPreset::Custom)
			{
				settings.radio.bEnabled = false;
			}
			else
			{
				const bool bDisableSpatialization = settings.radio.bDisableSpatialization;
				settings.radio = GetCryOdinRadioPreset(preset);
				settings.radio.bDisableSpatialization = bDisableSpatialization;
			}
			SetRoomVoiceSettings(settings);
		}

		void CCryOdinUserComponent::SetSendPosition(bool bSendPosition)
		{
			// The server would keep culling against the last position sent. Back to where everyone starts.
			if (m_bSendPosition && !bSendPosition && m_pRoom != nullptr)
			{
				m_pRoom->SetPosition(Vec3(ZERO));
			}

			m_bSendPosition = bSendPosition;
			m_bAppliedSendPosition = bSendPosition;
			m_pEntity->UpdateComponentEventMask(this);

			// Joined without position updates, so the encoder is made again with them. Only that is
			// changed, the room keeps the bitrate it has.
			if (m_bSendPosition && m_pRoom != nullptr && m_pRoom->GetEncoderSettings().GetPositionUpdateIntervalMs() <= 0)
			{
				SCryOdinEncoderSettings settings = m_pRoom->GetEncoderSettings();
				settings.iPositionUpdateIntervalMs = kPositionKeepAliveMs;
				m_pRoom->SetEncoderSettings(settings);
			}
		}

		SCryOdinEncoderSettings CCryOdinUserComponent::GetEffectiveEncoder() const
		{
			SCryOdinEncoderSettings settings = m_encoder;
			if (m_pRoom != nullptr)
			{
				// Never lowered here. Turning position updates off would make a new encoder for nothing.
				settings.iPositionUpdateIntervalMs = std::max(settings.GetPositionUpdateIntervalMs(), m_pRoom->GetEncoderSettings().GetPositionUpdateIntervalMs());
			}
			if (m_bSendPosition && settings.iPositionUpdateIntervalMs <= 0)
			{
				settings.iPositionUpdateIntervalMs = kPositionKeepAliveMs;
			}
			return settings;
		}

		void CCryOdinUserComponent::ApplyEncoderSettings()
		{
			m_appliedEncoder = m_encoder;
			if (m_pRoom != nullptr)
			{
				m_pRoom->SetEncoderSettings(GetEffectiveEncoder());
			}
		}

		void CCryOdinUserComponent::SetEncoderSettings(const SCryOdinEncoderSettings& settings)
		{
			m_encoder = settings;
			ApplyEncoderSettings();
		}

		SCryOdinEncoderSettings CCryOdinUserComponent::GetCurrentEncoderForChange() const
		{
			// What the room really uses, which JoinWithParams or the console can have set, so changing one
			// value doesn't undo the others. The position interval stays the component's own.
			SCryOdinEncoderSettings settings = m_pRoom != nullptr ? m_pRoom->GetEncoderSettings() : m_encoder;
			settings.iPositionUpdateIntervalMs = m_encoder.iPositionUpdateIntervalMs;
			return settings;
		}

		void CCryOdinUserComponent::SetVoiceQuality(ECryOdinVoiceQuality quality)
		{
			SCryOdinEncoderSettings settings = GetCurrentEncoderForChange();
			settings.quality = quality;
			SetEncoderSettings(settings);
		}

		void CCryOdinUserComponent::SetVoiceBitrateKbps(int kbps)
		{
			SCryOdinEncoderSettings settings = GetCurrentEncoderForChange();
			settings.quality = ECryOdinVoiceQuality::Custom;
			settings.iBitrateBps = SCryOdinEncoderSettings::KbpsToBps(kbps);
			SetEncoderSettings(settings);
		}

		void CCryOdinUserComponent::SetExpectedPacketLoss(int percent)
		{
			SCryOdinEncoderSettings settings = GetCurrentEncoderForChange();
			settings.iExpectedPacketLossPercent = crymath::clamp(percent, 0, 100);
			SetEncoderSettings(settings);
		}

		void CCryOdinUserComponent::SetProximityRadius(float fRadius)
		{
			m_fProximityRadius = fRadius;
		}

		bool CCryOdinUserComponent::IsTalking() const
		{
			const ICryOdinUser* pUser = GetLocalUser();
			return pUser != nullptr && pUser->IsTalking();
		}

		void CCryOdinUserComponent::SetDisplayName(const char* szUserName)
		{
			m_userName = szUserName != nullptr ? szUserName : "";

			ICryOdinUser* pUser = GetLocalUser();
			if (pUser == nullptr)
			{
				// Still joining, the room started with the old name.
				m_bDisplayNamePending = m_pRoom != nullptr;
				return;
			}

			// Keep the rest of the user data as it is.
			OdinJson userData = OdinJsonParse(pUser->GetUserData());
			if (userData.is_discarded() || !userData.is_object())
			{
				userData = OdinJson::object();
			}
			userData["name"] = m_userName.c_str();
			m_pRoom->SetUserData(OdinJsonDump(userData).c_str());
		}

		void CCryOdinUserComponent::OnJoinedRoom(ICryOdinRoom& room)
		{
			if (IsMyRoom(room))
			{
				if (m_bDisplayNamePending && &room == m_pRoom)
				{
					m_bDisplayNamePending = false;
					const string name = m_userName.c_str();
					SetDisplayName(name.c_str());
				}
				SendOdinSignal(m_pEntity, GetGUID(), SOdinJoinedSignal(room.GetRoomId(), GetUserId()));
			}
		}

		void CCryOdinUserComponent::OnLeftRoom(ICryOdinRoom& room, const char* szReason)
		{
			if (IsMyRoom(room))
			{
				if (&room == m_pRoom)
				{
					m_pRoom = nullptr;
				}
				stl::find_and_erase(m_leavingRooms, &room);
				SendOdinSignal(m_pEntity, GetGUID(), SOdinLeftSignal(room.GetRoomId(), szReason));
			}
		}

		void CCryOdinUserComponent::OnPeerJoined(ICryOdinRoom& room, ICryOdinUser& user)
		{
			if (IsMyRoom(room))
			{
				SendOdinSignal(m_pEntity, GetGUID(), SOdinPeerJoinedSignal(user.GetName(), user.GetUserId()));
			}
		}

		void CCryOdinUserComponent::OnPeerLeft(ICryOdinRoom& room, ICryOdinUser& user)
		{
			// The local user never had an On Peer Joined signal, but it could be showing as talking.
			if (IsMyRoom(room) && user.IsLocal())
			{
				if (user.IsTalking())
				{
					SendOdinSignal(m_pEntity, GetGUID(), SOdinTalkingSignal(user.GetName(), user.GetUserId(), true, false));
				}
			}
			else if (IsMyRoom(room))
			{
				SendOdinSignal(m_pEntity, GetGUID(), SOdinPeerLeftSignal(user.GetName(), user.GetUserId()));
			}
		}

		void CCryOdinUserComponent::OnPeerUserDataChanged(ICryOdinRoom& room, ICryOdinUser& user)
		{
			if (IsMyRoom(room))
			{
				SendOdinSignal(m_pEntity, GetGUID(), SOdinPeerChangedSignal(user.GetName(), user.GetUserId()));
			}
		}

		void CCryOdinUserComponent::OnPeerTalkingChanged(ICryOdinRoom& room, ICryOdinUser& user, bool bTalking)
		{
			if (IsMyRoom(room))
			{
				SendOdinSignal(m_pEntity, GetGUID(), SOdinTalkingSignal(user.GetName(), user.GetUserId(), user.IsLocal(), bTalking));
			}
		}

		void CCryOdinUserComponent::OnTextMessage(ICryOdinRoom& room, ICryOdinUser* pSender, const char* szText, const char* szChannel)
		{
			if (IsMyRoom(room))
			{
				SendOdinSignal(m_pEntity, GetGUID(), SOdinTextSignal(pSender != nullptr ? pSender->GetName() : "", pSender != nullptr ? pSender->GetUserId() : "", szText, szChannel));
			}
		}
	}
}
