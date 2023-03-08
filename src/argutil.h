#pragma once
#include "fmt/format.h"


// UnrefPtr 顾名思义就是去掉指针的引用类型，通过模版自动推导类型实现
// char*、void* value 为 false
// shared_ptr、unique_ptr、Arg* value 为 true

template<typename Arg>
struct UnrefPtr : std::false_type
{
    using type = Arg;
};

// 特化为 char*
template<>
struct UnrefPtr<char*> : std::false_type
{
    using type = char*;
};

// 特化为 void*
template<>
struct UnrefPtr<void*> : std::false_type
{
    using type = void*;
};

// shared_ptr value 设为 true
template<typename Arg>
struct UnrefPtr<std::shared_ptr<Arg>> : std::true_type
{
    using type = Arg;
};

// unique_ptr value 设为 true
template<typename Arg, typename D>
struct UnrefPtr<std::unique_ptr<Arg, D>> : std::true_type
{
    using type = Arg;
};

// 指针类型 value 设为 true
template<typename Arg>
struct UnrefPtr<Arg*> : std::true_type
{
    using type = Arg;
};


template<typename Arg>
static inline constexpr bool IsNamedArg()
{
    /*
    执行 fmt::remove_cvref_t<Arg> 之后，得到基础类型

    命名参数类型为 named_arg<Char, T>
    接下来会调用 is_named_arg 的特化版本
    template <typename T, typename Char>
    struct is_named_arg<named_arg<Char, T>> : std::true_type {};

    非命名参数类型为 T
    接下来会调用 is_named_arg 的特化版本
    template <typename T>
    struct is_named_arg : std::false_type {};
    */
    return fmt::detail::is_named_arg<fmt::remove_cvref_t<Arg>>::value;
}

template<typename Arg>
struct UnNamedType
{
    using type = Arg;
};

// 特化
template<typename Arg>
struct UnNamedType<fmt::detail::named_arg<char, Arg>>
{
    using type = Arg;
};

#if FMT_USE_NONTYPE_TEMPLATE_PARAMETERS
template<typename Arg, size_t N, fmt::detail_exported::fixed_string<char, N> Str>
struct unNamedType<fmt::detail::statically_named_arg<Arg, char, N, Str>>
{
    using type = Arg;
};
#endif

template<typename Arg>
static inline constexpr bool IsCstring()
{
    return fmt::detail::mapped_type_constant<Arg, fmt::format_context>::value ==
           fmt::detail::type::cstring_type;
}

template<typename Arg>
static inline constexpr bool IsString()
{
    return fmt::detail::mapped_type_constant<Arg, fmt::format_context>::value ==
           fmt::detail::type::string_type;
}

template<typename Arg>
static inline constexpr bool IsNeedCallDestructor()
{
    using ArgType = fmt::remove_cvref_t<Arg>;
    if constexpr (IsNamedArg<Arg>())
    {
        return IsNeedCallDestructor<typename UnNamedType<ArgType>::type>();
    }

    if constexpr (IsString<Arg>())
        return false;

    return !std::is_trivially_destructible<ArgType>::value;
}


template<size_t Idx, size_t NamedIdx>
static inline constexpr void StoreNamedArgs(fmt::detail::named_arg_info<char>* namedArgArr)
{
}

template<size_t Idx, size_t NamedIdx, typename Arg, typename... Args>
static inline constexpr void StoreNamedArgs(fmt::detail::named_arg_info<char>* namedArgArr,
                                            const Arg&                         arg,
                                            const Args&... args)
{
    if constexpr (IsNamedArg<Arg>())
    {
        namedArgArr[NamedIdx] = {arg.name, Idx};
        StoreNamedArgs<Idx + 1, NamedIdx + 1>(namedArgArr, args...);
    }
    else
    {
        StoreNamedArgs<Idx + 1, NamedIdx>(namedArgArr, args...);
    }
}


template<bool IsReorder, typename... Args>
fmt::string_view UnNameFormat(fmt::string_view in, uint32_t* reorderIdx, const Args&... args)
{
    constexpr size_t namedArgCnt = fmt::detail::count<IsNamedArg<Args>()...>();
    if constexpr (namedArgCnt == 0)
    {
        return in;
    }

    // namedArgArr 保存的是： {argName, argIdx}，这里充当的是查询表
    fmt::detail::named_arg_info<char> namedArgArr[std::max(namedArgCnt, (size_t)1)];
    StoreNamedArgs<0, 0>(namedArgArr, args...);

    std::unique_ptr<char[]> unNamedStrUptr(new char[in.size() + 1 + namedArgCnt * 5]);  // 为什么 + 1 + namedArgCnt * 5？为什么是 unique_ptr
    char*                   pOut        = (char*)unNamedStrUptr.get();
    const char*             pBegin      = in.data();
    const char*             pCur        = pBegin;
    uint8_t                 namedArgIdx = 0;

    // {HMSf} {s:<16} {l}[{t:<6}] => {} {:<16} {}[{:<6}]
    while (true)
    {
        char c = *pCur++;
        if (!c)  //  处理完成
        {
            size_t copySize = pCur - pBegin - 1;
            memcpy(pOut, pBegin, copySize);
            pOut += copySize;
            break;
        }

        if (c != '{') continue;

        size_t copySize = pCur - pBegin;  // 拷贝 { 之前的内容
        memcpy(pOut, pBegin, copySize);
        pOut += copySize;
        pBegin = pCur;  // pBegin + copySize
        c      = *pCur++;

        if (!c) fmt::detail::throw_format_error("invalid format string");

        if (fmt::detail::is_name_start(c))  // 判断是否是字母和下划线，pattern 只支持这些
        {
            // 跳过字母、数字、下划线
            while ((fmt::detail::is_name_start(c = *pCur) || ('0' <= c && c <= '9')))
            {
                ++pCur;
            }

            fmt::string_view name(pBegin, pCur - pBegin);  // 当前格式化项，比如 YmdHMS

            int              argIdx = -1;
            for (size_t i = 0; i < namedArgCnt; ++i)
            {
                if (namedArgArr[i].name == name)
                {
                    argIdx = namedArgArr[i].id;
                    break;
                }
            }

            if (argIdx < 0) fmt::detail::throw_format_error("invalid format string");

            if constexpr (IsReorder)
            {
                reorderIdx[argIdx] = namedArgIdx++;  // 映射回去，当前参数是第几个命名参数
            }
            else
            {
                pOut = fmt::format_to(pOut, "{}", argIdx);  // 返回结果只是让 pOut + 1，没有添加终止符
            }
        }
        else
        {
            *pOut++ = c;  // 特殊符号，比如 } ]
        }

        pBegin = pCur;
    }

    const char* ptr = unNamedStrUptr.release();

    return fmt::string_view(ptr, pOut - ptr);
}


template<size_t CstringIdx>
static inline constexpr size_t GetArgSize(size_t* pCstringSize)
{
    return 0;
}

template<size_t CstringIdx, typename Arg, typename... Args>
static inline constexpr size_t GetArgSize(size_t* pCstringSize, const Arg& arg, const Args&... args)
{
    if constexpr (IsNamedArg<Arg>())
    {
        // 如果是命名参数就提取其value
        return GetArgSize<CstringIdx>(pCstringSize, arg.value, args...);
    }
    else if constexpr (IsCstring<Arg>())
    {
        size_t len               = strlen(arg) + 1;  // +1 是因为后面有个 '/0'
        pCstringSize[CstringIdx] = len;
        return len + GetArgSize<CstringIdx + 1>(pCstringSize, args...);  // CstringIdx + 1 指向数组下一个位置
    }
    else if constexpr (IsString<Arg>())
    {
        size_t len = arg.size() + 1;  // C++11 之后 string 尾部带 null
        return len + GetArgSize<CstringIdx>(pCstringSize, args...);
    }
    else
    {
        // 内置数值变量
        return sizeof(Arg) + GetArgSize<CstringIdx>(pCstringSize, args...);
    }
}

template<size_t CstringIdx>
static inline constexpr char* EncodeArgs(size_t* pCstringSize, char* pOut)
{
    return pOut;
}

template<size_t CstringIdx, typename Arg, typename... Args>
static inline constexpr char* EncodeArgs(size_t* pCstringSize, char* pOut, Arg&& arg, Args&&... args)
{
    if constexpr (IsNamedArg<Arg>())
    {
        return EncodeArgs<CstringIdx>(pCstringSize, pOut, arg.value, std::forward<Args>(args)...);
    }
    else if constexpr (IsCstring<Arg>())
    {
        memcpy(pOut, arg, pCstringSize[CstringIdx]);
        return EncodeArgs<CstringIdx + 1>(pCstringSize, pOut + pCstringSize[CstringIdx], std::forward<Args>(args)...);
    }
    else if constexpr (IsString<Arg>())
    {
        size_t len = arg.size();
        memcpy(pOut, arg.data(), len);
        pOut[len] = 0;  // 末尾应该是补 \0 才对吧
        return EncodeArgs<CstringIdx>(pCstringSize, pOut + len + 1, std::forward<Args>(args)...);
    }
    else
    {
        // If Arg has alignment >= 16, gcc could emit aligned move instructions(e.g. movdqa) for
        // placement new even if the *out* is misaligned, which would cause segfault. So we use memcpy
        // when possible
        // if constexpr (std::is_trivially_copyable<fmt::remove_cvref_t<Arg>>::value)
        if constexpr (std::is_trivially_copyable_v<fmt::remove_cvref_t<Arg>>)
        {
            memcpy(pOut, &arg, sizeof(Arg));
        }
        else
        {
            // 不可简单拷贝说明就是容器, 还有?
            // placement new，使用完后需要手动调用析构
            new (pOut) fmt::remove_cvref_t<Arg>(std::forward<Arg>(arg));
        }
        return EncodeArgs<CstringIdx>(pCstringSize, pOut + sizeof(Arg), std::forward<Args>(args)...);
    }
}


template<bool ValueOnly, size_t Idx, size_t DestructIdx>
static inline const char* DecodeArgs(const char*                                 pInData,
                                     fmt::basic_format_arg<fmt::format_context>* pOutArg,
                                     const char**                                pNeedDestructArgPtrArr)
{
    return pInData;
}

template<bool ValueOnly, size_t Idx, size_t DestructIdx, typename Arg, typename... Args>
static inline const char* DecodeArgs(const char*                                 pInData,
                                     fmt::basic_format_arg<fmt::format_context>* pOutArg,
                                     const char**                                pOutNeedDestructArgPtrArr)
{
    using ArgType = fmt::remove_cvref_t<Arg>;

    if constexpr (IsNamedArg<ArgType>())
    {
        return DecodeArgs<ValueOnly,
                          Idx,
                          DestructIdx,
                          typename UnNamedType<ArgType>::type,  // 命名参数变成普通参数后再处理
                          Args...>(pInData, pOutArg, pOutNeedDestructArgPtrArr);
    }
    else if constexpr (IsCstring<Arg>() || IsString<Arg>())
    {
        size_t           size = strlen(pInData);
        fmt::string_view str(pInData, size);

        // 这里字符串赋值没有涉及到拷贝
        if constexpr (ValueOnly)
        {
            fmt::detail::value<fmt::format_context>& value = *(fmt::detail::value<fmt::format_context>*)(pOutArg + Idx);
            value                                          = fmt::detail::arg_mapper<fmt::format_context>().map(str);
        }
        else
        {
            pOutArg[Idx] = fmt::detail::make_arg<fmt::format_context>(str);
        }
        // Idx + 1 : 处理下一个参数；pInData + size + 1： +1 是因为末尾有结束/0
        return DecodeArgs<ValueOnly, Idx + 1, DestructIdx, Args...>(pInData + size + 1, pOutArg, pOutNeedDestructArgPtrArr);
    }
    else  // 其它标准变量及容器
    {
        if constexpr (ValueOnly)
        {
            fmt::detail::value<fmt::format_context>& value = *(fmt::detail::value<fmt::format_context>*)(pOutArg + Idx);
            if constexpr (UnrefPtr<ArgType>::value)  // 指针类型
            {
                // 此时 ArgType 为 pointer，以下为处理过程
                // const char* => ArgType*
                // ArgType* => ArgType
                // ArgType => value
                // .map => 返回安全类型（提升精度）
                value = fmt::detail::arg_mapper<fmt::format_context>().map(**(ArgType*)pInData);
            }
            else
            {
                value = fmt::detail::arg_mapper<fmt::format_context>().map(*(ArgType*)pInData);
            }
        }
        else
        {
            if constexpr (UnrefPtr<ArgType>::value)
            {
                pOutArg[Idx] = fmt::detail::make_arg<fmt::format_context>(**(ArgType*)pInData);
            }
            else
            {
                pOutArg[Idx] = fmt::detail::make_arg<fmt::format_context>(*(ArgType*)pInData);
            }
        }

        // 标准容器需要调用析构函数
        // 比如 vector 在 EncodeArgs 时只是拷贝了其三个指针 https://zhuanlan.zhihu.com/p/257423774
        // placement new 在堆上创建了一个新的 vector
        // DecodeArgs 使用完后需对其进行释放
        if constexpr (IsNeedCallDestructor<Arg>())
        {
            pOutNeedDestructArgPtrArr[DestructIdx] = pInData;
            return DecodeArgs<ValueOnly, Idx + 1, DestructIdx + 1, Args...>(pInData + sizeof(ArgType), pOutArg, pOutNeedDestructArgPtrArr);
        }
        else
        {
            return DecodeArgs<ValueOnly, Idx + 1, DestructIdx, Args...>(pInData + sizeof(ArgType), pOutArg, pOutNeedDestructArgPtrArr);
        }
    }
}

template<size_t DestructIdx>
static inline void DestructArgs(const char** pNeedDestructArgPtrArr)
{
}

template<size_t DestructIdx, typename Arg, typename... Args>
static inline void DestructArgs(const char** pNeedDestructArgPtrArr)
{
    using ArgType = fmt::remove_cvref_t<Arg>;

    if constexpr (IsNamedArg<ArgType>())
    {
        DestructArgs<DestructIdx, typename UnNamedType<ArgType>::type, Args...>(pNeedDestructArgPtrArr);
    }
    else if constexpr (IsNeedCallDestructor<Arg>())
    {
        ((ArgType*)pNeedDestructArgPtrArr[DestructIdx])->~ArgType();
        DestructArgs<DestructIdx + 1, Args...>(pNeedDestructArgPtrArr);  // 处理下一个
    }
    else
    {
        DestructArgs<DestructIdx, Args...>(pNeedDestructArgPtrArr);  // 处理下一个
    }
}


template<typename... Args>
const char* Format(fmt::string_view                                         format,
                   const char*                                              pData,
                   fmt::basic_memory_buffer<char, 10000>&                   out,
                   int&                                                     argIdx,
                   std::vector<fmt::basic_format_arg<fmt::format_context>>& argVec)
{
    constexpr size_t argNum            = sizeof...(Args);
    constexpr size_t needDestructorNum = fmt::detail::count<IsNeedCallDestructor<Args>()...>();
    const char*      pNeedDestructArgPtrArr[std::max(needDestructorNum, (size_t)1)];  // 需要手动销毁的参数索引
    const char*      pRet;

    if (argIdx < 0)
    {
        argIdx = (int)argVec.size();
        argVec.resize(argIdx + argNum);
        // template<bool ValueOnly, size_t Idx, size_t DestructIdx, typename Arg, typename... Args>
        pRet = DecodeArgs<false, 0, 0, Args...>(pData, argVec.data() + argIdx, pNeedDestructArgPtrArr);
    }
    else
    {
        pRet = DecodeArgs<true, 0, 0, Args...>(pData, argVec.data() + argIdx, pNeedDestructArgPtrArr);
    }

    // vformat_to(out, format, fmt::basic_format_args(argVec.data() + argIdx, argNum));
    fmt::detail::vformat_to(out, format, fmt::basic_format_args(argVec.data() + argIdx, argNum));
    DestructArgs<0, Args...>(pNeedDestructArgPtrArr);

    return pRet;
}


// static void VformatTo(fmt::basic_memory_buffer<char, 10000>& out,
//                       fmt::string_view                       fmt,
//                       fmt::format_args                       args)
// {
//     fmt::detail::vformat_to(out, fmt, args);
// }


// static void vformat_to(char*            out,
//                        fmt::string_view fmt,
//                        fmt::format_args args)
// {
//     fmt::vformat_to(out, fmt, args);
// }


static size_t FormattedSize(fmt::string_view fmt, fmt::format_args args)
{
    auto buf = fmt::detail::counting_buffer<>();
    fmt::detail::vformat_to(buf, fmt, args);
    return buf.count();
}