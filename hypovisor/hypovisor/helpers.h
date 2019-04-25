#pragma once
#include <wdm.h>

void SetBit(UINT64 Addr, UINT64 bit, BOOLEAN Set) {
	UINT64 byte = bit / 8;
	UINT64 temp = bit % 8;
	UINT64 n = 7 - temp;

	unsigned char* Addr2 = (unsigned char*)Addr;
	if (Set)
	{
		Addr2[byte] |= (1 << n);
	}
	else
	{
		Addr2[byte] &= ~(1 << n);

	}

}

int GetBit(PVOID Addr, UINT64 bit) {
	UINT64 byte = 0, k = 0;
	byte = bit / 8;
	k = 7 - bit % 8;
	unsigned char* Addr2 = Addr;

	return Addr2[byte] & (1 << k);
}

