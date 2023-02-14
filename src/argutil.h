#pragma once
#include "fmt/format.h"


// 定义 std::false_type 类型的模板类
template<typename Arg>
struct UnrefPtr : std::false_type
{
    using type = Arg;
};

template<>
struct UnrefPtr<char*> : std::false_type
{
    using type = char*;
};

template<>
struct UnrefPtr<void*> : std::false_type
{
    using type = void*;
};

template<typename Arg>
struct UnrefPtr<std::shared_ptr<Arg>> : std::true_type
{
    using type = Arg;
};

template<typename Arg, typename D>
struct UnrefPtr<std::unique_ptr<Arg, D>> : std::true_type
{
    using type = Arg;
};

template<typename Arg>
struct UnrefPtr<Arg*> : std::true_type
{
    using type = Arg;
};


template<typename Arg>
static inline constexpr bool IsNamedArg()
{
    /*
    执行 fmt::remove_cvref_t<Arg> 之后

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
        size_t len               = strlen(arg) + 1;  // +1 是因为后面有个 '/ 0'
        pCstringSize[CstringIdx] = len;
        return len + GetArgSize<CstringIdx + 1>(pCstringSize, args...);  // CstringIdx + 1 指向数组下一个位置
    }
    else if constexpr (IsString<Arg>())
    {
        size_t len = arg.size() + 1;  // string 为何要 +1？
        return len + GetArgSize<CstringIdx>(pCstringSize, args...);
    }
    else
    {
        // 内置数值变量
        return sizeof(Arg) + GetArgSize<CstringIdx>(pCstringSize, args...);
    }
}

template<size_t CstringIdx>
static inline constexpr char* EncodeArgs(size_t* pCstringSize, char* out)
{
    return out;
}

template<size_t CstringIdx, typename Arg, typename... Args>
static inline constexpr char* EncodeArgs(size_t* pCstringSize, char* out, Arg&& arg, Args&&... args)
{
    if constexpr (IsNamedArg<Arg>())
    {
        return EncodeArgs<CstringIdx>(pCstringSize, out, arg.value, std::forward<Args>(args)...);
    }
    else if constexpr (IsCstring<Arg>())
    {
        memcpy(out, arg, pCstringSize[CstringIdx]);
        return EncodeArgs<CstringIdx + 1>(pCstringSize, out + pCstringSize[CstringIdx], std::forward<Args>(args)...);
    }
    else if constexpr (IsString<Arg>())
    {
        size_t len = arg.size();
        memcpy(out, arg.data(), len);
        out[len] = 0;  // 末尾补 0 作何用？
        return EncodeArgs<CstringIdx>(pCstringSize, out + len + 1, std::forward<Args>(args)...);
    }
    else
    {
        // If Arg has alignment >= 16, gcc could emit aligned move instructions(e.g. movdqa) for
        // placement new even if the *out* is misaligned, which would cause segfault. So we use memcpy
        // when possible
        // if constexpr (std::is_trivially_copyable<fmt::remove_cvref_t<Arg>>::value)
        if constexpr (std::is_trivially_copyable_v<fmt::remove_cvref_t<Arg>>)
        {
            memcpy(out, &arg, sizeof(Arg));
        }
        else
        {
            // placement new
            new (out) fmt::remove_cvref_t<Arg>(std::forward<Arg>(arg));
        }
        return EncodeArgs<CstringIdx>(pCstringSize, out + sizeof(Arg), std::forward<Args>(args)...);
    }
}

template<size_t Idx, size_t NamedIdx>
static inline constexpr void StoreNamedArgs(fmt::detail::named_arg_info<char>* named_args_store)
{
}

template<size_t Idx, size_t NamedIdx, typename Arg, typename... Args>
static inline constexpr void StoreNamedArgs(fmt::detail::named_arg_info<char>* named_args_store,
                                            const Arg&                         arg,
                                            const Args&... args)
{
    if constexpr (IsNamedArg<Arg>())
    {
        named_args_store[NamedIdx] = {arg.name, Idx};
        StoreNamedArgs<Idx + 1, NamedIdx + 1>(named_args_store, args...);
    }
    else
    {
        StoreNamedArgs<Idx + 1, NamedIdx>(named_args_store, args...);
    }
}

template<bool ValueOnly, size_t Idx, size_t DestructIdx>
static inline const char* DecodeArgs(const char*                                 in,
                                     fmt::basic_format_arg<fmt::format_context>* args,
                                     const char**                                destruct_args)
{
    return in;
}

template<bool ValueOnly, size_t Idx, size_t DestructIdx, typename Arg, typename... Args>
static inline const char* DecodeArgs(const char*                                 in,
                                     fmt::basic_format_arg<fmt::format_context>* args,
                                     const char**                                destruct_args)
{
    // using namespace fmtlogdetail;
    using ArgType = fmt::remove_cvref_t<Arg>;
    if constexpr (IsNamedArg<ArgType>())
    {
        return DecodeArgs<ValueOnly, Idx, DestructIdx, typename UnNamedType<ArgType>::type, Args...>(
            in,
            args,
            destruct_args);
    }
    else if constexpr (IsCstring<Arg>() || IsString<Arg>())
    {
        size_t           size = strlen(in);
        fmt::string_view v(in, size);
        if constexpr (ValueOnly)
        {
            fmt::detail::value<fmt::format_context>& value_ = *(fmt::detail::value<fmt::format_context>*)(args + Idx);
            value_                                          = fmt::detail::arg_mapper<fmt::format_context>().map(v);
        }
        else
        {
            args[Idx] = fmt::detail::make_arg<fmt::format_context>(v);
        }
        return DecodeArgs<ValueOnly, Idx + 1, DestructIdx, Args...>(in + size + 1, args, destruct_args);
    }
    else
    {
        if constexpr (ValueOnly)
        {
            fmt::detail::value<fmt::format_context>& value_ = *(fmt::detail::value<fmt::format_context>*)(args + Idx);
            if constexpr (UnrefPtr<ArgType>::value)
            {
                value_ = fmt::detail::arg_mapper<fmt::format_context>().map(**(ArgType*)in);
            }
            else
            {
                value_ = fmt::detail::arg_mapper<fmt::format_context>().map(*(ArgType*)in);
            }
        }
        else
        {
            if constexpr (UnrefPtr<ArgType>::value)
            {
                args[Idx] = fmt::detail::make_arg<fmt::format_context>(**(ArgType*)in);
            }
            else
            {
                args[Idx] = fmt::detail::make_arg<fmt::format_context>(*(ArgType*)in);
            }
        }

        if constexpr (IsNeedCallDestructor<Arg>())
        {
            destruct_args[DestructIdx] = in;
            return DecodeArgs<ValueOnly, Idx + 1, DestructIdx + 1, Args...>(in + sizeof(ArgType), args, destruct_args);
        }
        else
        {
            return DecodeArgs<ValueOnly, Idx + 1, DestructIdx, Args...>(in + sizeof(ArgType), args, destruct_args);
        }
    }
}

template<size_t DestructIdx>
static inline void DestructArgs(const char** destruct_args)
{
}

template<size_t DestructIdx, typename Arg, typename... Args>
static inline void DestructArgs(const char** destruct_args)
{
    using ArgType = fmt::remove_cvref_t<Arg>;
    if constexpr (IsNamedArg<ArgType>())
    {
        DestructArgs<DestructIdx, typename UnNamedType<ArgType>::type, Args...>(destruct_args);
    }
    else if constexpr (IsNeedCallDestructor<Arg>())
    {
        ((ArgType*)destruct_args[DestructIdx])->~ArgType();
        DestructArgs<DestructIdx + 1, Args...>(destruct_args);
    }
    else
    {
        DestructArgs<DestructIdx, Args...>(destruct_args);
    }
}

template<bool Reorder, typename... Args>
fmt::string_view UnNameFormat(fmt::string_view in, uint32_t* reorderIdx, const Args&... args)
{
    constexpr size_t namedArgCnt = fmt::detail::count<IsNamedArg<Args>()...>();
    if constexpr (namedArgCnt == 0)
    {
        return in;
    }

    // namedArgArr 保存的是 namedArgIdx -- {argName, argIdx}
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
        if (!c)
        {
            //  处理完成
            size_t copySize = pCur - pBegin - 1;
            memcpy(pOut, pBegin, copySize);
            pOut += copySize;
            break;
        }

        if (c != '{') continue;

        size_t copySize = pCur - pBegin;  // 第一次到这里时为 1，拷贝 {
        memcpy(pOut, pBegin, copySize);
        pOut += copySize;
        pBegin = pCur;
        c      = *pCur++;

        if (!c) fmt::detail::throw_format_error("invalid format string");

        if (fmt::detail::is_name_start(c))
        {
            while ((fmt::detail::is_name_start(c = *pCur) || ('0' <= c && c <= '9')))
            {
                ++pCur;
            }

            fmt::string_view name(pBegin, pCur - pBegin);

            int              id = -1;  // argIdx
            for (size_t i = 0; i < namedArgCnt; ++i)
            {
                if (namedArgArr[i].name == name)
                {
                    id = namedArgArr[i].id;
                    break;
                }
            }

            if (id < 0) fmt::detail::throw_format_error("invalid format string");

            if constexpr (Reorder)
            {
                reorderIdx[id] = namedArgIdx++;  // 映射回去
            }
            else
            {
                pOut = fmt::format_to(pOut, "{}", id);
            }
        }
        else
        {
            *pOut++ = c;
        }

        pBegin = pCur;
    }

    const char* ptr = unNamedStrUptr.release();

    return fmt::string_view(ptr, pOut - ptr);
}

template<typename... Args>
const char* FormatTo(fmt::string_view                                         format,
                     const char*                                              data,
                     fmt::basic_memory_buffer<char, 10000>&                   out,
                     int&                                                     argIdx,
                     std::vector<fmt::basic_format_arg<fmt::format_context>>& args)
{
    constexpr size_t num_args  = sizeof...(Args);
    constexpr size_t num_dtors = fmt::detail::count<IsNeedCallDestructor<Args>()...>();
    const char*      dtor_args[std::max(num_dtors, (size_t)1)];  // 需要手动销毁的参数索引
    const char*      ret;
    if (argIdx < 0)
    {
        argIdx = (int)args.size();
        args.resize(argIdx + num_args);
        // template<bool ValueOnly, size_t Idx, size_t DestructIdx, typename Arg, typename... Args>
        ret = DecodeArgs<false, 0, 0, Args...>(data, args.data() + argIdx, dtor_args);
    }
    else
    {
        ret = DecodeArgs<true, 0, 0, Args...>(data, args.data() + argIdx, dtor_args);
    }
    vformat_to(out, format, fmt::basic_format_args(args.data() + argIdx, num_args));
    DestructArgs<0, Args...>(dtor_args);

    return ret;
}


static void vformat_to(fmt::basic_memory_buffer<char, 10000>& out,
                       fmt::string_view                       fmt,
                       fmt::format_args                       args)
{
    fmt::detail::vformat_to(out, fmt, args);
}


static void vformat_to(char*            out,
                       fmt::string_view fmt,
                       fmt::format_args args)
{
    fmt::vformat_to(out, fmt, args);
}


static size_t formatted_size(fmt::string_view fmt, fmt::format_args args)
{
    auto buf = fmt::detail::counting_buffer<>();
    fmt::detail::vformat_to(buf, fmt, args);
    return buf.count();
}