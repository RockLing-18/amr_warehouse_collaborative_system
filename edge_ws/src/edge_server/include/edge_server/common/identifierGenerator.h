#pragma once

#include <stdint.h>
#include <atomic>

template<typename T> class IdentifierGenerator
{
public:
	IdentifierGenerator() : m_id(1) {}
	~IdentifierGenerator() = default;

	T generate()
	{
		T i = 0;

		if (0 == (i = m_id.fetch_add(1)))
		{
			i = m_id.fetch_add(1);
		}

		return i;
	}

	void recycle(T id)
	{
	}

private:
	IdentifierGenerator(const IdentifierGenerator&) = delete;
	IdentifierGenerator& operator=(const IdentifierGenerator&) = delete;

private:
	std::atomic<T> m_id;
};

typedef IdentifierGenerator<uint8_t>	IdGeneratorU8_t;
typedef IdentifierGenerator<uint16_t>	IdGeneratorU16_t;
typedef IdentifierGenerator<uint32_t>	IdGeneratorU32_t;
typedef IdentifierGenerator<uint64_t>	IdGeneratorU64_t;