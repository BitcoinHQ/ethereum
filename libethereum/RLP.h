/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file RLP.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 *
 * RLP (de-)serialisation.
 */

#pragma once

#include <exception>
#include <iostream>
#include <iomanip>
#include "vector_ref.h"
#include "Common.h"

namespace eth
{

class RLP;
typedef std::vector<RLP> RLPs;

/**
 * @brief Class for interpreting Recursive Linear-Prefix Data.
 * @by Gav Wood, 2013
 *
 * Class for reading byte arrays of data in RLP format.
 */
class RLP
{
public:
	class BadCast: public std::exception {};

	/// Construct a null node.
	RLP() {}

	/// Construct a node of value given in the bytes.
	explicit RLP(bytesConstRef _d): m_data(_d) {}

	/// Construct a node of value given in the bytes.
	explicit RLP(bytes const& _d): m_data(const_cast<bytes*>(&_d)) {}	// a bit horrible, but we know we won't be altering the data. TODO: allow vector<T> const* to be passed to vector_ref<T const>.

	/// Construct a node to read RLP data in the bytes given.
	RLP(byte const* _b, uint _s): m_data(bytesConstRef(_b, _s)) {}

	/// Construct a node to read RLP data in the string.
	explicit RLP(std::string const& _s): m_data(bytesConstRef((byte const*)_s.data(), _s.size())) {}

	bytesConstRef data() const { return m_data; }

	/// @returns true if the RLP is non-null.
	explicit operator bool() const { return !isNull(); }

	/// No value.
	bool isNull() const { return m_data.size() == 0; }

	/// Contains a zero-length string or zero-length list.
	bool isEmpty() const { return m_data[0] == 0x40 || m_data[0] == 0x80; }

	/// String value.
	bool isString() const { assert(!isNull()); return m_data[0] >= 0x40 && m_data[0] < 0x80; }

	/// List value.
	bool isList() const { assert(!isNull()); return m_data[0] >= 0x80 && m_data[0] < 0xc0; }

	/// Integer value. Either isSlimInt(), isFatInt() or isBigInt().
	bool isInt() const { assert(!isNull()); return m_data[0] < 0x40; }

	/// Fits into eth::uint type. Can use toSlimInt() to read (as well as toFatInt() or toBigInt() ).
	bool isSlimInt() const { assert(!isNull()); return m_data[0] < 0x20; }

	/// Fits into eth::u256 or eth::bigint type. Use only toFatInt() or toBigInt() to read.
	bool isFatInt() const { assert(!isNull()); return m_data[0] >= 0x20 && m_data[0] < 0x38; }

	/// Fits into eth::u256 type, though might fit into eth::uint type.
	bool isFixedInt() const { assert(!isNull()); return m_data[0] < 0x38; }

	/// Fits only into eth::bigint type. Use only toBigInt() to read.
	bool isBigInt() const { assert(!isNull()); return m_data[0] >= 0x38 && m_data[0] < 0x40; }

	/// @returns the number of items in the list, or zero if it isn't a list.
	uint itemCount() const { return isList() ? items() : 0; }
	uint itemCountStrict() const { if (!isList()) throw BadCast(); return items(); }

	/// @returns the number of characters in the string, or zero if it isn't a string.
	uint stringSize() const { return isString() ? items() : 0; }

	/// Equality operators; does best-effort conversion and checks for equality.
	bool operator==(char const* _s) const { return isString() && toString() == _s; }
	bool operator!=(char const* _s) const { return isString() && toString() != _s; }
	bool operator==(std::string const& _s) const { return isString() && toString() == _s; }
	bool operator!=(std::string const& _s) const { return isString() && toString() != _s; }
	bool operator==(uint const& _i) const { return (isInt() || isString()) && toSlimInt() == _i; }
	bool operator!=(uint const& _i) const { return (isInt() || isString()) && toSlimInt() != _i; }
	bool operator==(u256 const& _i) const { return (isInt() || isString()) && toFatInt() == _i; }
	bool operator!=(u256 const& _i) const { return (isInt() || isString()) && toFatInt() != _i; }
	bool operator==(bigint const& _i) const { return (isInt() || isString()) && toBigInt() == _i; }
	bool operator!=(bigint const& _i) const { return (isInt() || isString()) && toBigInt() != _i; }

	/// Subscript operator.
	/// @returns the list item @a _i if isList() and @a _i < listItems(), or RLP() otherwise.
	/// @note if used to access items in ascending order, this is efficient.
	RLP operator[](uint _i) const;

	typedef RLP element_type;

	/// @brief Iterator class for iterating through items of RLP list.
	class iterator
	{
		friend class RLP;

	public:
		typedef RLP value_type;
		typedef RLP element_type;

		iterator& operator++();
		iterator operator++(int) { auto ret = *this; operator++(); return ret; }
		RLP operator*() const { return RLP(m_lastItem); }
		bool operator==(iterator const& _cmp) const { return m_lastItem == _cmp.m_lastItem; }
		bool operator!=(iterator const& _cmp) const { return !operator==(_cmp); }

	private:
		iterator() {}
		iterator(RLP const& _parent, bool _begin);

		uint m_remaining = 0;
		bytesConstRef m_lastItem;
	};

	/// @brief Iterator into beginning of sub-item list (valid only if we are a list).
	iterator begin() const { return iterator(*this, true); }

	/// @brief Iterator into end of sub-item list (valid only if we are a list).
	iterator end() const { return iterator(*this, false); }

	/// Best-effort conversion operators.
	explicit operator std::string() const { return toString(); }
	explicit operator RLPs() const { return toList(); }
	explicit operator uint() const { return toSlimInt(); }
	explicit operator u256() const { return toFatInt(); }
	explicit operator bigint() const { return toBigInt(); }

	/// Converts to string. @returns the empty string if not a string.
	std::string toString() const { if (!isString()) return std::string(); return payload().cropped(0, items()).toString(); }
	/// Converts to string. @throws BadCast if not a string.
	std::string toStringStrict() const { if (!isString()) throw BadCast(); return payload().cropped(0, items()).toString(); }

	/// Converts to int of type given; if isString(), decodes as big-endian bytestream. @returns 0 if not an int or string.
	template <class _T = uint> _T toInt() const
	{
		if (!isString() && !isInt())
			return 0;
		if (isDirectValueInt())
			return m_data[0];
		_T ret = 0;
		auto s = isInt() ? intSize() - lengthSize() : isString() ? items() : 0;
		uint o = lengthSize() + 1;
		for (uint i = 0; i < s; ++i)
			ret = (ret << 8) | m_data[i + o];
		return ret;
	}

	/// Converts to eth::uint. @see toInt()
	uint toSlimInt() const { return toInt<uint>(); }
	/// Converts to eth::u256. @see toInt()
	u256 toFatInt() const { return toInt<u256>(); }
	/// Converts to eth::bigint. @see toInt()
	bigint toBigInt() const { return toInt<bigint>(); }

	/// Converts to eth::uint. @throws BadCast if not isInt(). @see toInt()
	uint toSlimIntStrict() const { if (!isSlimInt()) throw BadCast(); return toInt<uint>(); }
	/// Converts to eth::u256. @throws BadCast if not isInt(). @see toInt()
	u256 toFatIntStrict() const { if (!isFatInt() && !isSlimInt()) throw BadCast(); return toInt<u256>(); }
	/// Converts to eth::bigint. @throws BadCast if not isInt(). @see toInt()
	bigint toBigIntStrict() const { if (!isInt()) throw BadCast(); return toInt<bigint>(); }

	/// Converts to eth::uint using the toString() as a big-endian bytestream. @throws BadCast if not isString(). @see toInt()
	uint toSlimIntFromString() const { if (!isString()) throw BadCast(); return toInt<uint>(); }
	/// Converts to eth::u256 using the toString() as a big-endian bytestream. @throws BadCast if not isString(). @see toInt()
	u256 toFatIntFromString() const { if (!isString()) throw BadCast(); return toInt<u256>(); }
	/// Converts to eth::bigint using the toString() as a big-endian bytestream. @throws BadCast if not isString(). @see toInt()
	bigint toBigIntFromString() const { if (!isString()) throw BadCast(); return toInt<bigint>(); }

	/// Converts to RLPs collection object. Useful if you need random access to sub items or will iterate over multiple times.
	RLPs toList() const;

private:
	/// Direct value integer.
	bool isDirectValueInt() const { assert(!isNull()); return m_data[0] < 0x18; }

	/// Indirect-value integer.
	bool isIndirectValueInt() const { assert(!isNull()); return m_data[0] >= 0x18 && m_data[0] < 0x38; }

	/// Indirect addressed integer.
	bool isIndirectAddressedInt() const { assert(!isNull()); return m_data[0] < 0x40 && m_data[0] >= 0x38; }

	/// Direct-length string.
	bool isSmallString() const { assert(!isNull()); return m_data[0] >= 0x40 && m_data[0] < 0x78; }

	/// Direct-length list.
	bool isSmallList() const { assert(!isNull()); return m_data[0] >= 0x80 && m_data[0] < 0xb8; }

	/// @returns the theoretical size of this item; if it's a list, will require a deep traversal which could take a while.
	/// @note Under normal circumstances, is equivalent to m_data.size() - use that unless you know it won't work.
	uint actualSize() const;

	/// @returns the total additional bytes used to encode the integer. Includes the data-size and potentially the length-size. Returns 0 if not isInt().
	uint intSize() const { return (!isInt() || isDirectValueInt()) ? 0 : isIndirectAddressedInt() ? lengthSize() + items() : (m_data[0] - 0x17); }

	/// @returns the bytes used to encode the length of the data. Valid for all types.
	uint lengthSize() const { auto n = (m_data[0] & 0x3f); return n > 0x37 ? n - 0x37 : 0; }

	/// @returns the number of data items (bytes in the case of strings & ints, items in the case of lists). Valid for all types.
	uint items() const;

	/// @returns the data payload. Valid for all types.
	bytesConstRef payload() const { auto n = (m_data[0] & 0x3f); return m_data.cropped(1 + (n < 0x38 ? 0 : (n - 0x37))); }

	/// Our byte data.
	bytesConstRef m_data;

	/// The list-indexing cache.
	mutable uint m_lastIndex = (uint)-1;
	mutable uint m_lastEnd = 0;
	mutable bytesConstRef m_lastItem;
};

/**
 * @brief Class for writing to an RLP bytestream.
 */
class RLPStream
{
public:
	/// Initializes empty RLPStream.
	RLPStream() {}

	/// Initializes the RLPStream as a list of @a _listItems items.
	explicit RLPStream(uint _listItems) { appendList(_listItems); }

	/// Append given data to the byte stream.
	RLPStream& append(uint _s);
	RLPStream& append(u256 _s);
	RLPStream& append(bigint _s);
	RLPStream& append(std::string const& _s);
	RLPStream& appendList(uint _count);

	/// Shift operators for appending data items.
	RLPStream& operator<<(uint _i) { return append(_i); }
	RLPStream& operator<<(u256 _i) { return append(_i); }
	RLPStream& operator<<(bigint _i) { return append(_i); }
	RLPStream& operator<<(char const* _s) { return append(std::string(_s)); }
	RLPStream& operator<<(std::string const& _s) { return append(_s); }
	template <class _T> RLPStream& operator<<(std::vector<_T> const& _s) { appendList(_s.size()); for (auto const& i: _s) append(i); return *this; }

	/// Read the byte stream.
	bytes const& out() const { return m_out; }

private:
	/// Push the node-type byte (using @a _base) along with the item count @a _count.
	/// @arg _count is number of characters for strings, data-bytes for ints, or items for lists.
	void pushCount(uint _count, byte _base);

	/// Push an integer as a raw big-endian byte-stream.
	template <class _T> void pushInt(_T _i, uint _br)
	{
		m_out.resize(m_out.size() + _br);
		byte* b = &m_out.back();
		for (; _i; _i >>= 8)
			*(b--) = (byte)_i;
	}

	/// Determine bytes required to encode the given integer value. @returns 0 if @a _i is zero.
	template <class _T> static uint bytesRequired(_T _i)
	{
		_i >>= 8;
		uint i = 1;
		for (; _i != 0; ++i, _i >>= 8) {}
		return i;
	}

	/// Our output byte stream.
	bytes m_out;
};

template <class _T> void rlpListAux(RLPStream& _out, _T _t) { _out << _t; }
template <class _T, class ... _Ts> void rlpListAux(RLPStream& _out, _T _t, _Ts ... _ts) { rlpListAux(_out << _t, _ts...); }

/// Export a single item in RLP format, returning a byte array.
template <class _T> bytes rlp(_T _t) { return (RLPStream() << _t).out(); }

/// Export a list of items in RLP format, returning a byte array.
inline bytes rlpList() { return RLPStream(0).out(); }
template <class ... _Ts> bytes rlpList(_Ts ... _ts)
{
	RLPStream out(sizeof ...(_Ts));
	rlpListAux(out, _ts...);
	return out.out();
}

/// The empty string in RLP format.
extern bytes RLPNull;

}

/// Human readable version of RLP.
std::ostream& operator<<(std::ostream& _out, eth::RLP _d);