#pragma once

#define ACCESS_PRIVATE_MEMBER(OwnerType, MemberName) \
	namespace PrivateAccess \
	{ \
		template<auto PropertyPtr> \
		class T##OwnerType##Accessor \
		{ \
			friend auto& MemberName(OwnerType& Object) \
			{ \
				return Object.*PropertyPtr; \
			} \
			friend const auto& MemberName(const OwnerType& Object) \
			{ \
				return Object.*PropertyPtr; \
			} \
		}; \
		template class T##OwnerType##Accessor<&OwnerType::MemberName>; \
		auto& MemberName(OwnerType& Object); \
		const auto& MemberName(const OwnerType& Object); \
	};

#define ACCESS_PRIVATE_MEMBER_STATIC(OwnerType, MemberName) \
	namespace PrivateAccess \
	{ \
		template<auto PropertyPtr> \
		class T##OwnerType##Accessor \
		{ \
			friend auto& OwnerType##_##MemberName() \
			{ \
				return *PropertyPtr; \
			} \
		}; \
		template class T##OwnerType##Accessor<&OwnerType::MemberName>; \
		auto& OwnerType##_##MemberName(); \
	};

#define ACCESS_PRIVATE_METHOD_ONE_PARAM(OwnerType, ReturnType, MethodName, ArgType0, ArgName0) \
    namespace PrivateAccess \
    { \
        template<auto MethodPtr> \
        class T##OwnerType##Accessor \
        { \
            friend ReturnType MethodName(OwnerType& Object, ArgType0 ArgName0) \
            { \
                return (Object.*MethodPtr)(ArgName0); \
            } \
        }; \
        template class T##OwnerType##Accessor<&OwnerType::MethodName>; \
        ReturnType MethodName(OwnerType& Object, ArgType0 ArgName0); \
        ReturnType MethodName(const OwnerType& Object, ArgType0 ArgName0); \
    };
