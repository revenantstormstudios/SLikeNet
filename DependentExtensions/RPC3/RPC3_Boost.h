/*
 *  Original work: Copyright (c) 2014, Oculus VR, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  RakNet License.txt file in the licenses directory of this source tree. An additional grant 
 *  of patent rights can be found in the RakNet Patents.txt file in the same directory.
 *
 *
 *  Modified work: Copyright (c) 2017-2020, SLikeSoft UG (haftungsbeschränkt)
 *
 *  This source code was modified by SLikeSoft. Modifications are licensed under the MIT-style
 *  license found in the license.txt file in the root directory of this source tree.
 */

#ifndef __RPC3_BOOST_H
#define __RPC3_BOOST_H

// This RPC system was originally powered by Boost. It has since been rewritten
// to depend only on the C++ standard library (C++17 or later) and no longer
// requires any third-party library.
#include <functional>   // std::function, std::invoke
#include <tuple>        // std::tuple, std::apply, std::get, std::tuple_size, std::tuple_element
#include <type_traits>  // std::conditional_t, std::is_*_v, std::remove_*_t, std::bool_constant
#include <utility>      // std::make_index_sequence, std::index_sequence
#include <cassert>      // assert (was previously pulled in transitively via Boost)
#include <cstring>      // memcpy

#include "slikenet/NetworkIDManager.h"
#include "slikenet/NetworkIDObject.h"
#include "slikenet/BitStream.h"

namespace SLNet
{
class RPC3;
class BitStream;


namespace _RPC3
{

enum InvokeResultCodes
{
	IRC_SUCCESS,
	IRC_NEED_BITSTREAM,
	IRC_NEED_NETWORK_ID_MANAGER,
	IRC_NEED_NETWORK_ID,
	IRC_NEED_CLASS_OBJECT,
};

struct InvokeArgs
{
	// Bitstream to use to deserialize
	SLNet::BitStream *bitStream;

	// NetworkIDManager to use to lookup objects
	NetworkIDManager *networkIDManager;

	// C++ class member object
	NetworkID classMemberObjectId;

	// The calling plugin
	RPC3 *caller;

	// The this pointer for C++
	NetworkIDObject *thisPtr;
};

typedef std::tuple<bool, std::function<InvokeResultCodes (InvokeArgs)> > FunctionPointer;

struct StrWithDestructor
{
	char *c;
	~StrWithDestructor() {if (c) delete[] c;}
};

enum RPC3TagFlag
{
	RPC3_TAG_FLAG_DEREF=1,
	RPC3_TAG_FLAG_ARRAY=2,
};

struct RPC3Tag
{
	RPC3Tag() {}
	RPC3Tag(void *_v, unsigned int _count, RPC3TagFlag _flag) : v(_v), count(_count), flag((unsigned char)_flag) {}
	void* v;
	unsigned int count;
	unsigned char flag;
};

// Maximum number of parameters an RPC function may have
// (formerly the Boost.Fusion macro BOOST_FUSION_INVOKE_MAX_ARITY)
static const int RPC3_MAX_ARITY = 10;

// Track the pointers tagged with SLNet::_RPC3::Deref
static RPC3Tag __RPC3TagPtrs[RPC3_MAX_ARITY+1];
static int __RPC3TagHead=0;
static int __RPC3TagTail=0;

// If this assert hits, then SLNet::_RPC3::Deref was called more times than the argument was passed to the function
// [[maybe_unused]]: these tag helpers are only referenced from the serialization paths, which are not
// instantiated in every translation unit that includes this header.
[[maybe_unused]] static void __RPC3_Tag_AddHead(const RPC3Tag &p)
{
	// Update tag if already in array
	int i;
	for (i=__RPC3TagTail; i!=__RPC3TagHead; i=(i+1)%RPC3_MAX_ARITY)
	{
		if (__RPC3TagPtrs[i].v==p.v)
		{
			if (p.flag==RPC3_TAG_FLAG_ARRAY)
			{
				__RPC3TagPtrs[i].count=p.count;
			}
			__RPC3TagPtrs[i].flag|=p.flag;

			return;
		}
	}

	__RPC3TagPtrs[__RPC3TagHead]=p;
	__RPC3TagHead = (__RPC3TagHead + 1) % RPC3_MAX_ARITY;
	assert(__RPC3TagHead!=__RPC3TagTail);
}
[[maybe_unused]] static void __RPC3ClearTail(void) {
	while (__RPC3TagTail!=__RPC3TagHead)
	{
		if (__RPC3TagPtrs[__RPC3TagTail].v==0)
			__RPC3TagTail = (__RPC3TagTail+1) % RPC3_MAX_ARITY;
		else
			return;
	}
}
[[maybe_unused]] static bool __RPC3ClearPtr(void* p, RPC3Tag *tag) {
	int i;
	for (i=__RPC3TagTail; i!=__RPC3TagHead; i=(i+1)%RPC3_MAX_ARITY)
	{
		if (__RPC3TagPtrs[i].v==p)
		{
			*tag=__RPC3TagPtrs[i];
			__RPC3TagPtrs[i].v=0;
			__RPC3ClearTail();			
			return true;
		}
	}
	tag->flag=0;
	tag->count=1;
	return false;
}

template <class templateType>
inline const templateType& Deref(const templateType & t) {
	__RPC3_Tag_AddHead(RPC3Tag((void*)t,1,RPC3_TAG_FLAG_DEREF));
	return t;
}

template <class templateType>
inline const templateType& PtrToArray(unsigned int count, const templateType & t) {
	__RPC3_Tag_AddHead(RPC3Tag((void*)t,count,RPC3_TAG_FLAG_ARRAY));
	return t;
}

struct ReadBitstream
{
	static void applyArray(SLNet::BitStream &bitStream, SLNet::BitStream* t){apply(bitStream,t);}

	static void apply(SLNet::BitStream &bitStream, SLNet::BitStream* t)
	{
		BitSize_t numBitsUsed;
		bitStream.ReadCompressed(numBitsUsed);
		bitStream.Read(t,numBitsUsed);
	}
};

//template <typename T>
struct ReadPtr
{
	template <typename T2>
	static inline void applyArray(SLNet::BitStream &bitStream, T2 *t) {bitStream >> (*t);}
	template <typename T2>
	static inline void apply(SLNet::BitStream &bitStream, T2 *t) {bitStream >> (*t);}

	static inline void apply(SLNet::BitStream &bitStream, char *&t) {applyStr(bitStream, (char *&) t);}
	static inline void apply(SLNet::BitStream &bitStream, unsigned char *&t) {applyStr(bitStream, (char *&) t);}
	static inline void apply(SLNet::BitStream &bitStream, const char *&t) {applyStr(bitStream, (char *&) t);}
	static inline void apply(SLNet::BitStream &bitStream, const unsigned char *&t) {applyStr(bitStream, (char *&) t);}
	static inline void applyStr(SLNet::BitStream &bitStream, char *&t)
	{
		SLNet::RakString rs;
		bitStream >> rs;
		size_t len = rs.GetLength()+1;
		
		// The caller should have already allocated memory, so we need to free
		// it and allocate a new buffer.
		RakAssert("Expected allocated array, got a null pointer" && (nullptr != t));
		delete [] t;

		t = new char [len];
		memcpy(t,rs.C_String(),len);
	}
};


template< typename T >
struct DoRead
{
	typedef std::conditional_t< std::is_convertible_v<T*, SLNet::BitStream*>,
		ReadBitstream,
		ReadPtr > type;
};


template< typename T >
struct ReadWithoutNetworkIDNoPtr
{
	static InvokeResultCodes apply(InvokeArgs &args, T &t)
	{
//		printf("ReadWithoutNetworkIDNoPtr\n");

		DoRead< std::remove_pointer_t<T> >::type::apply(* (args.bitStream),&t);

		return IRC_SUCCESS;
	}

	// typedef boost::mpl::false_ Cleanup;
	template< typename T2 >
	static void Cleanup(T2 &t)
	{
		// unused parameters
		(void)t;
	}
};

template< typename T >
struct ReadWithNetworkIDPtr
{
	static InvokeResultCodes apply(InvokeArgs &args, T &t)
	{
//		printf("ReadWithNetworkIDPtr\n");
		// Read the network ID

		bool isNull;
		args.bitStream->Read(isNull);
		if (isNull)
		{
			t=0;
			return IRC_SUCCESS;
		}

		bool deref, isArray;
		args.bitStream->Read(deref);
		args.bitStream->Read(isArray);
		unsigned int count;
		if (isArray)
			args.bitStream->ReadCompressed(count);
		else
			count=1;
		NetworkID networkId;
		for (unsigned int i=0; i < count; i++)
		{
			args.bitStream->Read(networkId);
			t = args.networkIDManager->GET_OBJECT_FROM_ID< T >(networkId);
			if (deref)
			{
				BitSize_t bitsUsed;
				args.bitStream->AlignReadToByteBoundary();
				args.bitStream->Read(bitsUsed);

				if (t)
				{
					DoRead< std::remove_pointer_t<T> >::type::apply(* (args.bitStream),t);
				}
				else
				{
					// Skip data!
					args.bitStream->IgnoreBits(bitsUsed);
				}
			}
		}
		
		return IRC_SUCCESS;
	}

	template< typename T2 >
	static void Cleanup(T2 &t)
	{
		// unused parameters
		(void)t;
	}
};

template< typename T >
struct ReadWithoutNetworkIDPtr
{
	template <typename T2>
	static InvokeResultCodes apply(InvokeArgs &args, T2 &t)
	{
//		printf("ReadWithoutNetworkIDPtr\n");
		
		bool isNull=false;
		args.bitStream->Read(isNull);
		if (isNull)
		{
			t=0;
			return IRC_SUCCESS;
		}

		typedef std::remove_pointer_t< T > ActualObjectType;

		bool isArray=false;
		unsigned int count;
		args.bitStream->Read(isArray);
		if (isArray)
			args.bitStream->ReadCompressed(count);
		else
			count=1;

		t = new ActualObjectType[count]();
		if (isArray)
		{
			for (unsigned int i=0; i < count; i++)
			{
				DoRead< std::remove_pointer_t<T> >::type::applyArray(* (args.bitStream),t+i);
			}
		}
		else
		{
			DoRead< std::remove_pointer_t<T> >::type::apply(* (args.bitStream),t);
		}

		return IRC_SUCCESS;
	}

	template< typename T2 >
	static void Cleanup(T2 &t)
	{
		if (t)
			delete [] t;
	}
};

template< typename T >
struct SetRPC3Ptr
{
	static InvokeResultCodes apply(InvokeArgs &args, T &obj)
	{
		obj=args.caller;
		return IRC_SUCCESS;
	}

	//typedef boost::mpl::false_ Cleanup;
	template< typename T2 >
	static void Cleanup(T2 &t)
	{
		// unused parameters
		(void)t;
	}
};

template< typename T >
struct ReadWithoutNetworkID
{
	typedef std::conditional_t< std::is_pointer_v<T>
		, ReadWithoutNetworkIDPtr<T> // true
		, ReadWithoutNetworkIDNoPtr<T>
	> type;
};

template< typename T >
struct IsRPC3Ptr : std::bool_constant< std::is_convertible_v<T, RPC3*> > {};

template< typename T >
struct ShouldReadNetworkID : std::bool_constant< std::is_convertible_v<T, NetworkIDObject*> > {};

template< typename T >
struct GetReadFunction
{
	typedef std::conditional_t< ShouldReadNetworkID<T>::value
		, ReadWithNetworkIDPtr<T>
		, typename ReadWithoutNetworkID<T>::type
	> type;
};

template< typename T >
struct ProcessArgType
{
	typedef std::conditional_t< IsRPC3Ptr<T>::value
		, SetRPC3Ptr<T>
		, typename GetReadFunction<T>::type
	> type;
};

// Compile-time traits describing a registered function's parameters.
// Replaces boost::function_types::parameter_types + boost::mpl iteration.
template <class F> struct RPCFunctionTraits;                       // primary template: intentionally undefined

// Free function pointer
template <class R, class... Args>
struct RPCFunctionTraits<R(*)(Args...)>
{
	static constexpr bool isMember = false;
	typedef std::tuple<std::remove_reference_t<Args>...> StorageTuple;
};
template <class R, class... Args>
struct RPCFunctionTraits<R(*)(Args...) noexcept> : RPCFunctionTraits<R(*)(Args...)> {};

// Member function pointer. The object is supplied separately (from thisPtr), so it is NOT stored.
template <class R, class C, class... Args>
struct RPCFunctionTraits<R(C::*)(Args...)>
{
	static constexpr bool isMember = true;
	typedef C ClassType;
	typedef std::tuple<std::remove_reference_t<Args>...> StorageTuple;
};
template <class R, class C, class... Args>
struct RPCFunctionTraits<R(C::*)(Args...) const>          : RPCFunctionTraits<R(C::*)(Args...)> {};
template <class R, class C, class... Args>
struct RPCFunctionTraits<R(C::*)(Args...) noexcept>       : RPCFunctionTraits<R(C::*)(Args...)> {};
template <class R, class C, class... Args>
struct RPCFunctionTraits<R(C::*)(Args...) const noexcept> : RPCFunctionTraits<R(C::*)(Args...)> {};

// Deserialize every parameter from the bitstream, in order. The braced-initializer
// expansion guarantees left-to-right evaluation, which is required because the wire
// format is read sequentially. The leading 0 keeps the array non-empty for a 0-arg call.
template <class Tuple, std::size_t... I>
inline void RPC3DeserializeArgs(InvokeArgs &functionArgs, Tuple &storage, std::index_sequence<I...>)
{
	// The (void) casts keep this warning-clean under /W4 /WX for zero-parameter functions (empty pack).
	(void)functionArgs; (void)storage;
	int seq[] = { 0, ( (void)ProcessArgType< std::tuple_element_t<I, Tuple> >::type::apply(functionArgs, std::get<I>(storage)), 0 )... };
	(void)seq;
}
template <class Tuple, std::size_t... I>
inline void RPC3CleanupArgs(Tuple &storage, std::index_sequence<I...>)
{
	(void)storage;
	int seq[] = { 0, ( (void)ProcessArgType< std::tuple_element_t<I, Tuple> >::type::Cleanup(std::get<I>(storage)), 0 )... };
	(void)seq;
}

// Invoke a registered C function: deserialize args, call, then clean up (in that order).
template <class Function>
inline InvokeResultCodes RPC3InvokeFunction(Function func, InvokeArgs functionArgs)
{
	typedef typename RPCFunctionTraits<Function>::StorageTuple StorageTuple;
	StorageTuple storage;                                                  // value-initializes each element
	constexpr std::size_t argCount = std::tuple_size<StorageTuple>::value;
	RPC3DeserializeArgs(functionArgs, storage, std::make_index_sequence<argCount>{});
	std::apply(func, storage);
	RPC3CleanupArgs(storage, std::make_index_sequence<argCount>{});
	return IRC_SUCCESS;
}

// Invoke a registered C++ member function. The object is recovered from thisPtr
// (a NetworkIDObject*) via the same static downcast the original Boost code performed.
template <class Function>
inline InvokeResultCodes RPC3InvokeMemberFunction(Function func, InvokeArgs functionArgs)
{
	typedef RPCFunctionTraits<Function> Traits;
	typedef typename Traits::StorageTuple StorageTuple;
	typedef typename Traits::ClassType ClassType;
	StorageTuple storage;
	constexpr std::size_t argCount = std::tuple_size<StorageTuple>::value;
	RPC3DeserializeArgs(functionArgs, storage, std::make_index_sequence<argCount>{});
	ClassType &obj = static_cast<ClassType &>(*functionArgs.thisPtr);
	std::apply([&](auto&... unpackedArgs){ std::invoke(func, obj, unpackedArgs...); }, storage);
	RPC3CleanupArgs(storage, std::make_index_sequence<argCount>{});
	return IRC_SUCCESS;
}

template <typename T>
struct DoNothing
{
	static void apply(SLNet::BitStream &bitStream, T& t)
	{
		(void) bitStream;
		(void) t;
//		printf("DoNothing\n");
	}
};

struct WriteBitstream
{
	static void applyArray(SLNet::BitStream &bitStream, SLNet::BitStream* t) {apply(bitStream,t);}
	static void apply(SLNet::BitStream &bitStream, SLNet::BitStream* t)
	{
		BitSize_t oldReadOffset = t->GetReadOffset();
		t->ResetReadPointer();
		bitStream.WriteCompressed(t->GetNumberOfBitsUsed());
		bitStream.Write(t);
		t->SetReadOffset(oldReadOffset);
	}
};

//template <typename T>
struct WritePtr
{
	template <typename T2>
	static inline void applyArray(SLNet::BitStream &bitStream, T2 *t) {bitStream << (*t);}
	template <typename T2>
	static inline void apply(SLNet::BitStream &bitStream, T2 *t) {bitStream << (*t);}
//	template <>
	static inline void apply(SLNet::BitStream &bitStream, char *t) {bitStream << t;}
//	template <>
	static inline void apply(SLNet::BitStream &bitStream, unsigned char *t) {bitStream << t;}
//	template <>
	static inline void apply(SLNet::BitStream &bitStream, const char *t) {bitStream << t;}
//	template <>
	static inline void apply(SLNet::BitStream &bitStream, const unsigned char *t) {bitStream << t;}
};

template< typename T >
struct DoWrite
{
	typedef std::conditional_t< std::is_convertible_v<T*, SLNet::BitStream*>,
		WriteBitstream,
		WritePtr > type;
};

template <typename T>
struct WriteWithNetworkIDPtr
{
	static void apply(SLNet::BitStream &bitStream, T& t)
	{
		bool isNull;
		isNull=(t==0);
		bitStream.Write(isNull);
		if (isNull)
			return;
		RPC3Tag tag;
		__RPC3ClearPtr(t, &tag);
		bool deref = (tag.flag & RPC3_TAG_FLAG_DEREF) !=0;
		bool isArray = (tag.flag & RPC3_TAG_FLAG_ARRAY) !=0;
		bitStream.Write(deref);
		bitStream.Write(isArray);
		if (isArray)
		{
			bitStream.WriteCompressed(tag.count);
		}
		for (unsigned int i=0; i < tag.count; i++)
		{
			NetworkID inNetworkID=t->GetNetworkID();
			bitStream << inNetworkID;
			if (deref)
			{
				// skip bytes, write data, go back, write number of bits written, reset cursor
				bitStream.AlignWriteToByteBoundary();
				BitSize_t writeOffset1 = bitStream.GetWriteOffset();
				BitSize_t bitsUsed1=bitStream.GetNumberOfBitsUsed();
				bitStream.Write(bitsUsed1);
				bitsUsed1=bitStream.GetNumberOfBitsUsed();
				DoWrite< std::remove_pointer_t<T> >::type::apply(bitStream,t);
				BitSize_t writeOffset2 = bitStream.GetWriteOffset();
				BitSize_t bitsUsed2=bitStream.GetNumberOfBitsUsed();
				bitStream.SetWriteOffset(writeOffset1);
				bitStream.Write(bitsUsed2-bitsUsed1);
				bitStream.SetWriteOffset(writeOffset2);
			}
		}		
	}
};

template <typename T>
struct WriteWithoutNetworkIDNoPtr
{
	static void apply(SLNet::BitStream &bitStream, T& t)
	{
		DoWrite< std::remove_pointer_t<T> >::type::apply(bitStream,&t);
	}
};

template <typename T>
struct WriteWithoutNetworkIDPtr
{
	static void apply(SLNet::BitStream &bitStream, T& t)
	{
		bool isNull;
		isNull=(t==0);
		bitStream.Write(isNull);
		if (isNull)
			return;

		RPC3Tag tag;
		__RPC3ClearPtr((void*) t, &tag);
		bool isArray = (tag.flag & RPC3_TAG_FLAG_ARRAY) !=0;
		bitStream.Write(isArray);
		if (isArray)
		{
			bitStream.WriteCompressed(tag.count);
		}
		if (isArray)
		{
			for (unsigned int i=0; i < tag.count; i++)
				DoWrite< std::remove_pointer_t<T> >::type::applyArray(bitStream,t+i);
		}
		else
		{
			DoWrite< std::remove_pointer_t<T> >::type::apply(bitStream,t);
		}
		
	}
};

template <typename T>
struct SerializeCallParameterBranch
{
	typedef std::conditional_t< IsRPC3Ptr<T>::value
		, DoNothing<T>
		, WriteWithoutNetworkIDPtr<T>
	> typeCheck1;

	typedef std::conditional_t< std::is_pointer_v<T>
		, typeCheck1
		, WriteWithoutNetworkIDNoPtr<T>
	> typeCheck2;

	typedef std::conditional_t< ShouldReadNetworkID<T>::value
		, WriteWithNetworkIDPtr<T>
		, typeCheck2
	> type;
};
template<typename Function>
FunctionPointer GetBoundPointer(Function f)
{
	if constexpr (std::is_member_function_pointer_v<Function>)
		return FunctionPointer(true,  [f](InvokeArgs args){ return RPC3InvokeMemberFunction<Function>(f, args); });
	else
		return FunctionPointer(false, [f](InvokeArgs args){ return RPC3InvokeFunction<Function>(f, args); });
}


}
}

#endif
