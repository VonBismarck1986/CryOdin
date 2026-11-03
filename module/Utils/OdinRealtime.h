#pragma once

#include <atomic>

namespace Cry
{
	namespace Odin
	{
		// Lets the main thread wait for a real time callback to finish, without the callback taking a
		// lock. The callback brackets its work with Enter and Leave. To free something a callback can
		// see, the main thread first removes it from what the callback loads, then calls WaitIdle.
		// A callback that starts after that will load the new data.
		class CCryOdinRtSection
		{
		public:
			void Enter() { m_bBusy.store(true, std::memory_order_seq_cst); }
			void Leave() { m_bBusy.store(false, std::memory_order_seq_cst); }

			void WaitIdle() const
			{
				while (m_bBusy.load(std::memory_order_seq_cst))
				{
					CrySleep(0);
				}
			}

		private:
			std::atomic<bool> m_bBusy{ false };
		};

		class CCryOdinRtScope
		{
		public:
			explicit CCryOdinRtScope(CCryOdinRtSection& section) : m_section(section) { m_section.Enter(); }
			~CCryOdinRtScope() { m_section.Leave(); }

		private:
			CCryOdinRtSection& m_section;
		};

		// Passes a plain struct from the main thread to an audio thread. The writer spins only for
		// the length of a struct copy. The reader never waits, and gets the value in a later block
		// if the writer happens to be busy.
		template<typename T>
		class TCryOdinRtMailbox
		{
		public:
			void Write(const T& value)
			{
				while (m_lock.test_and_set(std::memory_order_acquire))
				{
				}
				m_value = value;
				m_version.fetch_add(1, std::memory_order_release);
				m_lock.clear(std::memory_order_release);
			}

			// Returns true and copies the value if there's a newer one than lastVersion.
			bool TryRead(T& outValue, uint32& lastVersion)
			{
				if (m_version.load(std::memory_order_acquire) == lastVersion)
				{
					return false;
				}
				if (m_lock.test_and_set(std::memory_order_acquire))
				{
					return false;
				}
				outValue = m_value;
				lastVersion = m_version.load(std::memory_order_relaxed);
				m_lock.clear(std::memory_order_release);
				return true;
			}

		private:
			std::atomic_flag m_lock = ATOMIC_FLAG_INIT;
			std::atomic<uint32> m_version{ 0 };
			T m_value{};
		};
	}
}
