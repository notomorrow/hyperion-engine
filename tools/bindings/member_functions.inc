<%def name="do_member_fn(num_args, is_const)">
    <% arg_template_str = ', '.join(["class Arg{}Type".format(i) for i in range(0, num_args)]) %>

    template <class ReturnType, class ThisType${", {}".format(arg_template_str) if len(arg_template_str) else ""}, ReturnType(ThisType::*MemFn)(${', '.join(["Arg{}Type".format(i) for i in range(0, num_args)])})${" const" if is_const else ""}>
    HYP_SCRIPT_FUNCTION(CxxMemberFn)
    {
        HYP_SCRIPT_CHECK_ARGS(==, ${num_args + 1});

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        <% invoke_args = [] %>

        auto &&this_arg = GetArgument<0, ThisType *>(params);

        % for i in range(0, num_args):
            auto &&arg${i} = GetArgument<${i + 1}, Arg${i}Type>(params);

            <% invoke_args.append("std::forward<Arg{}Type>(arg{})".format(i, i)) %>
        % endfor
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID((this_arg->*MemFn)(${', '.join(invoke_args)}));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, (this_arg->*MemFn)(${', '.join(invoke_args)}));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }

    template <class ReturnType, class WrappedThisType, class ThisType${", {}".format(arg_template_str) if len(arg_template_str) else ""}, ReturnType(ThisType::*MemFn)(${', '.join(["Arg{}Type".format(i) for i in range(0, num_args)])})${" const" if is_const else ""}>
    HYP_SCRIPT_FUNCTION(CxxMemberFnWrapped)
    {
        HYP_SCRIPT_CHECK_ARGS(==, ${num_args + 1});

        static_assert(std::is_class_v<ThisType>);

        using Normalized = NormalizedType<ReturnType>;

        <% invoke_args = [] %>

        auto &&this_arg = *GetArgument<0, WrappedThisType *>(params);

        % for i in range(0, num_args):
            auto &&arg${i} = GetArgument<${i + 1}, Arg${i}Type>(params);

            <% invoke_args.append("std::forward<Arg{}Type>(arg{})".format(i, i)) %>
        % endfor
        
        if constexpr (std::is_same_v<void, Normalized>) {
            HYP_SCRIPT_RETURN_VOID(((*this_arg).*MemFn)(${', '.join(invoke_args)}));
        } else {
            auto return_value = CxxToScriptValueInternal<ReturnType>(params.api_instance, ((*this_arg).*MemFn)(${', '.join(invoke_args)}));
                                                                                                 
            HYP_SCRIPT_RETURN(return_value);
        }
    }
</%def>

% for i in range(0, 10):
    ${do_member_fn(i, False)}
    ${do_member_fn(i, True)}
% endfor